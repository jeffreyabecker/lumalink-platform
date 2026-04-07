#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string_view>


#include "ByteStream.h"

namespace httpadv::v1::transport {
enum class FileOpenMode { Read, Write, ReadWrite };

class IFile : public IByteChannel {
public:
  ~IFile() override = default;

  virtual bool isDirectory() const = 0;
  virtual void close() = 0;
  virtual std::optional<std::size_t> size() const = 0;
  virtual std::string_view name() const = 0;
  virtual std::string_view path() const = 0;
  virtual std::optional<uint32_t> lastWriteEpochSeconds() const = 0;
};

using FileHandle = std::unique_ptr<IFile>;

struct DirectoryEntry {
  std::string_view name;
  std::string_view path;
  bool isDirectory = false;
};
using DirectoryEntryCallback = std::function<bool(const DirectoryEntry &entry)>;

class IFileSystem {
public:
  virtual ~IFileSystem() = default;

  virtual bool exists(std::string_view path) const = 0;
  virtual bool mkdir(std::string_view path) = 0;
  virtual FileHandle open(std::string_view path, FileOpenMode mode) = 0;
  virtual bool remove(std::string_view path) = 0;
  virtual bool rename(std::string_view from, std::string_view to) = 0;
  // Canonical implementation surface.
  // Return false only for real enumeration failure.
  // If the callback returns false, stop early and still return true.
  virtual bool list(std::string_view directoryPath,
                    const DirectoryEntryCallback &callback,
                    bool recursive = false) = 0;
};
} // namespace httpadv::v1::transport