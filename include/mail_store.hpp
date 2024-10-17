#pragma once

#include <filesystem>
#include <string_view>

#include <asio.hpp>
#include <asio/stream_file.hpp>

#include "lmdb++.h"

using asio::awaitable;

class MailStoreFile;

class MailStore {
  public:
    MailStore(asio::io_context& io_service, const std::string&& path, size_t max_size);

    MailStoreFile create_file() const;
    MailStoreFile open_file(const std::string_view filename) const;

  private:
    asio::io_service& _io_service;
    const std::filesystem::path _path;
    const std::filesystem::path _tmp_path;
    size_t _max_size;
    lmdb::env _db_env;
    
    lmdb::dbi _db_main;
    lmdb::dbi _db_subjects;
};

class MailStoreFile {
    friend class MailStore;

  public:
    ~MailStoreFile();

    awaitable<size_t> write(const std::string_view sv);
    awaitable<size_t> write_encoded(const std::string_view sv);
    awaitable<std::string> read(size_t size);

    const std::string& get_filename() const { return _filename; }

    [[nodiscard]] bool close();

  protected:
    MailStoreFile(asio::io_context& io_service, const std::string& filename,
                  const std::filesystem::path& tmp_path, const std::filesystem::path& final_path,
                  size_t max_size);
    MailStoreFile(asio::io_context& io_service, const std::string_view filename,
                  const std::filesystem::path& path);

  private:
    asio::stream_file _file;
    const std::string _filename;
    const std::string _tmp_path;
    const std::string _final_path;
    const size_t _max_size;
    size_t _size;
    bool _new{false};
    bool _finished{false};
    
    size_t _chars_since_cr{0};
    std::string _leftover{};
};