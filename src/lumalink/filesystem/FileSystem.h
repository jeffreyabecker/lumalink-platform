#pragma once

#include "lumalink/buffers/ByteStream.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace lumalink::platform::filesystem {
    inline constexpr std::string_view ContractName = "filesystem";
    enum class FileOpenMode { Read, Write, ReadWrite };

    class IFile : public lumalink::platform::buffers::IByteChannel {
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
        std::string name;
        std::string path;
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
        virtual bool list(std::string_view directoryPath,
                          const DirectoryEntryCallback &callback,
                          bool recursive = false) = 0;
    };
}