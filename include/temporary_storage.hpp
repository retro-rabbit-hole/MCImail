#pragma once

#include <filesystem>
#include <string_view>

#include <asio.hpp>
#include <asio/stream_file.hpp>

using asio::awaitable;

class TemporaryFile;

class TemporaryStorage {
  public:
    TemporaryStorage(asio::io_context& io_service, const std::string&& path, size_t max_size);

    TemporaryFile create_file() const;

  private:
    asio::io_service& _io_service;
    const std::filesystem::path _path;
    const std::filesystem::path _tmp_path;
    size_t _max_size;
};

class TemporaryFile {
    friend class TemporaryStorage;

  public:
    ~TemporaryFile();

    awaitable<size_t> write(const std::string_view sv);
    const std::string& get_filename() const { return _filename; }

    [[nodiscard]] bool close();

  protected:
    TemporaryFile(asio::io_context& io_service, const std::string& filename,
                  const std::filesystem::path& tmp_path, const std::filesystem::path& final_path, size_t max_size);

  private:
    asio::stream_file _file;
    const std::string _filename;
    const std::string _tmp_path;
    const std::string _final_path;
    const size_t _max_size;
    size_t _size;
    bool _finished{false};
};