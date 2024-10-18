#include <fcntl.h>
#include <unistd.h>

#include <cerrno>
#include <filesystem>
#include <print>
#include <random>
#include <string_view>

#include <asio/use_awaitable.hpp>

#include "mail_store.hpp"
#include "string_utils.hpp"

using asio::use_awaitable;

static std::string generate_filename(int length) {
    static constexpr auto charset =
        std::string_view{"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_"};

    static thread_local std::mt19937 rng{std::random_device{}()};
    static thread_local std::uniform_int_distribution<> dist(0, charset.size() - 1);

    std::string result(length, '\0');
    std::ranges::generate(result, [&] { return charset[dist(rng)]; });

    return result;
}

MailStore::MailStore(asio::io_context& io_service, const std::string&& path, size_t max_size)
    : _io_service(io_service), _path(path), _tmp_path(_path / "tmp"), _max_size(max_size),
      _db_env(lmdb::env::create()) {

    // MIGHT BLOCK
    std::filesystem::create_directories(_tmp_path);
    std::filesystem::create_directories(_path / "db");

    _db_env.set_mapsize(1UL * 1024UL * 1024UL * 1024UL);
    _db_env.set_max_dbs(5);
    _db_env.open((_path / "db").c_str(), 0, 0664);

    // Open databases
    auto txn = lmdb::txn::begin(_db_env);
    try {
        _db_main = lmdb::dbi::open(txn, "main", MDB_CREATE);
        _db_subjects = lmdb::dbi::open(txn, "subject_index", MDB_CREATE | MDB_DUPSORT);
        txn.commit();
    } catch (const lmdb::error& e) {
        txn.abort();
        throw;
    }
}

// Might throw
MailStoreFile MailStore::create_file() const {
    const std::string filename = generate_filename(10);
    const std::filesystem::path tmp_path = _tmp_path / filename;
    const std::filesystem::path final_path = _path / filename;

    return MailStoreFile(_io_service, filename, tmp_path, final_path, _max_size);
}

// Might throw
MailStoreFile MailStore::open_file(const std::string_view filename) const {
    const std::filesystem::path path = _path / filename;

    return MailStoreFile(_io_service, filename, path);
}

MailStoreFile::MailStoreFile(asio::io_context& io_service, const std::string& filename,
                             const std::filesystem::path& tmp_path,
                             const std::filesystem::path& final_path, size_t max_size)
    : _file(io_service, tmp_path,
            asio::stream_file::write_only | asio::stream_file::create |
                asio::stream_file::exclusive),
      _filename(filename), _tmp_path(tmp_path), _final_path(final_path), _max_size(max_size),
      _new{true} {}

MailStoreFile::MailStoreFile(asio::io_context& io_service, const std::string_view filename,
                             const std::filesystem::path& path)
    : _file(io_service, path, asio::stream_file::read_only), _filename(filename), _final_path(path),
      _max_size{0}, _new{false} {}

MailStoreFile::~MailStoreFile() {
    asio::error_code ec;

    if (_finished)
        return;

    if (_new) {
        // MIGHT BLOCK
        unlink(_tmp_path.c_str());
    }

    ec = _file.close(ec);
    _finished = true;
}

struct Combined {
    Combined(std::string_view first, std::string_view second) : _first(first), _second(second) {}

    unsigned char operator[](size_t x) const {
        if (x < _first.size()) {
            return _first[x];
        } else {
            return _second[x - _first.size()];
        }
    }

    size_t size() const { return _first.size() + _second.size(); }

    std::string_view _first;
    std::string_view _second;
};

static std::string mep2_decode(std::string_view input, std::string& leftover) {
    std::string result;

    Combined c(leftover, input);

    size_t total_size = c.size();
    size_t i = 0;

    result.reserve(total_size);

    for (i = 0; i < total_size; ++i) {
        if (c[i] == '%') {
            if (i + 2 < total_size) {
                // Check for transparent newline
                if (c[i + 1] == '\r' && c[i + 2] == '\n') {
                    i += 2; // Skip the transparent newline
                    continue;
                }

                // Regular percent encoding
                char decoded_char = (hex_to_char(c[i + 1]) << 4) | hex_to_char(c[i + 2]);
                result.push_back(decoded_char);
                i += 2;
            } else {
                // % encoded value, but not enough space
                break;
            }
        } else {
            result.push_back(c[i]);
        }
    }

    std::string new_leftover;
    for (size_t k = i; k < total_size; ++k) {
        new_leftover.push_back(c[k]);
    }

    leftover = std::move(new_leftover);
    return result;
}

awaitable<size_t> MailStoreFile::write(std::string_view sv) {
    size_t size =
        co_await asio::async_write(_file, asio::buffer(sv.data(), sv.size()), use_awaitable);
    co_return size;
}

awaitable<size_t> MailStoreFile::write_encoded(std::string_view sv) {
    std::string decoded = mep2_decode(sv, _leftover);
    // size_t size =
    co_await asio::async_write(_file, asio::dynamic_buffer(decoded), use_awaitable);
    co_return sv.size();
}

awaitable<std::string> MailStoreFile::read(size_t size) {
    asio::error_code ec;
    std::string data;
    data.reserve(size);

    co_await asio::async_read(_file, asio::dynamic_buffer(data, size),
                              asio::redirect_error(use_awaitable, ec));

    if (ec && ec != asio::error::eof) {
        throw std::runtime_error(
            std::format("Error reading from {}: {}", _final_path, ec.message()));
    }

    co_return data;
}

bool MailStoreFile::close() {
    asio::error_code ec;

    if (_finished)
        return true;

    if (_new) {
        // MIGHT BLOCK
        if (link(_tmp_path.c_str(), _final_path.c_str())) {
            throw std::runtime_error(
                std::format("Error linking {}: {}", _final_path, strerror(errno)));
        }

        // MIGHT BLOCK
        unlink(_tmp_path.c_str());
    }

    ec = _file.close(ec);
    if (ec) {
        return false;
    }

    _finished = true;

    return true;
}
