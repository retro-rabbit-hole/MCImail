#include <fcntl.h>
#include <unistd.h>

#include <cerrno>
#include <filesystem>
#include <random>
#include <string_view>

#include <print>

#include <asio/use_awaitable.hpp>

#include "temporary_storage.hpp"

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

TemporaryStorage::TemporaryStorage(asio::io_context& io_service, const std::string&& path,
                                   size_t max_size)
    : _io_service(io_service), _path(path), _tmp_path(_path / "tmp"), _max_size(max_size) {

	// MIGHT BLOCK
    std::filesystem::create_directories(_tmp_path);
}

// Might throw
TemporaryFile TemporaryStorage::create_file() const {
    const std::string filename = generate_filename(10);
    const std::filesystem::path tmp_path = _tmp_path / filename;
    const std::filesystem::path final_path = _path / filename;

    return TemporaryFile(_io_service, filename, tmp_path, final_path, _max_size);
}

TemporaryFile::TemporaryFile(asio::io_context& io_service, const std::string& filename,
                             const std::filesystem::path& tmp_path,
                             const std::filesystem::path& final_path, size_t max_size)
    : _file(io_service, tmp_path,
            asio::stream_file::write_only | asio::stream_file::create |
                asio::stream_file::exclusive),
      _filename(filename), _tmp_path(tmp_path), _final_path(final_path), _max_size(max_size) {}

TemporaryFile::~TemporaryFile() {
    asio::error_code ec;

    if (_finished)
        return;

    // MIGHT BLOCK
    unlink(_tmp_path.c_str());

    ec = _file.close(ec);
    _finished = true;
}

awaitable<size_t> TemporaryFile::write(std::string_view sv) {
    size_t size =
        co_await asio::async_write(_file, asio::buffer(sv.data(), sv.size()), use_awaitable);
    co_return size;
}

bool TemporaryFile::close() {
    asio::error_code ec;

    if (_finished)
        return true;

    // MIGHT BLOCK
    if (link(_tmp_path.c_str(), _final_path.c_str())) {
        std::println("Error linking: {}", strerror(errno));
        return false;
    }

    // MIGHT BLOCK
    unlink(_tmp_path.c_str());

    ec = _file.close(ec);
    if (ec) {
        return false;
    }

    _finished = true;

    return true;
}
