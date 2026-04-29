#pragma once

#include "../filesystem/FileSystem.h"
#include "../filesystem/ScopedFileSystem.h"
#include "../filesystem/PathUtility.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

#include <algorithm>
#include <fstream>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace lumalink::platform::windows
{
    using lumalink::platform::buffers::AvailableBytes;
    using lumalink::platform::buffers::ByteAvailability;
    using lumalink::platform::buffers::ExhaustedResult;
    using lumalink::platform::buffers::TemporarilyUnavailableResult;
    using lumalink::platform::filesystem::DirectoryEntry;
    using lumalink::platform::filesystem::DirectoryEntryCallback;
    using lumalink::platform::filesystem::FileHandle;
    using lumalink::platform::filesystem::FileOpenMode;
    using lumalink::platform::filesystem::IFile;
    using lumalink::platform::filesystem::IFileSystem;
    using lumalink::platform::filesystem::PathUtility;

    struct FileMetadata
    {
        bool exists = false;
        bool directory = false;
        std::optional<std::size_t> size;
        std::optional<uint32_t> lastWrite;
    };

    inline constexpr char HostPathSeparator = '\\';
    inline constexpr char ExposedPathSeparator = '/';

    inline std::string ToHostPath(std::string_view exposedPath)
    {
        std::string result(exposedPath);
        std::replace(result.begin(), result.end(), ExposedPathSeparator, HostPathSeparator);
        return result;
    }

    inline std::string ToExposedPath(std::string_view hostPath)
    {
        std::string result(hostPath);
        std::replace(result.begin(), result.end(), HostPathSeparator, ExposedPathSeparator);
        return result;
    }

    inline std::optional<uint32_t> ToEpochSeconds(const FILETIME &time)
    {
        ULARGE_INTEGER rawTime;
        rawTime.LowPart = time.dwLowDateTime;
        rawTime.HighPart = time.dwHighDateTime;

        static constexpr unsigned long long WindowsEpochOffset = 116444736000000000ULL;
        static constexpr unsigned long long TicksPerSecond = 10000000ULL;

        if (rawTime.QuadPart <= WindowsEpochOffset)
        {
            return static_cast<uint32_t>(0);
        }

        const auto seconds = (rawTime.QuadPart - WindowsEpochOffset) / TicksPerSecond;
        if (seconds >= static_cast<unsigned long long>(std::numeric_limits<uint32_t>::max()))
        {
            return std::numeric_limits<uint32_t>::max();
        }

        return static_cast<uint32_t>(seconds);
    }

    inline FileMetadata ReadHostMetadata(std::string_view hostPath)
    {
        FileMetadata metadata;

        WIN32_FILE_ATTRIBUTE_DATA fileData;
        if (!GetFileAttributesExA(std::string(hostPath).c_str(), GetFileExInfoStandard, &fileData))
        {
            return metadata;
        }

        metadata.exists = true;
        metadata.directory = (fileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

        if (!metadata.directory)
        {
            ULARGE_INTEGER fileSize;
            fileSize.LowPart = fileData.nFileSizeLow;
            fileSize.HighPart = fileData.nFileSizeHigh;
            metadata.size = static_cast<std::size_t>(fileSize.QuadPart);
        }

        metadata.lastWrite = ToEpochSeconds(fileData.ftLastWriteTime);
        return metadata;
    }

    inline bool CreateHostDirectory(std::string_view hostPath)
    {
        const std::string ownedPath(hostPath);
        if (CreateDirectoryA(ownedPath.c_str(), nullptr) != 0)
        {
            return true;
        }

        if (GetLastError() == ERROR_ALREADY_EXISTS)
        {
            const FileMetadata metadata = ReadHostMetadata(ownedPath);
            return metadata.exists && metadata.directory;
        }

        return false;
    }

    inline bool RemoveHostPath(std::string_view hostPath, bool isDirectory)
    {
        const std::string ownedPath(hostPath);
        return isDirectory ? (RemoveDirectoryA(ownedPath.c_str()) != 0) : (DeleteFileA(ownedPath.c_str()) != 0);
    }

    inline bool RenameHostPath(std::string_view fromHostPath, std::string_view toHostPath)
    {
        const std::string ownedFrom(fromHostPath);
        const std::string ownedTo(toHostPath);
        return MoveFileA(ownedFrom.c_str(), ownedTo.c_str()) != 0;
    }

    inline std::string JoinHostPath(std::string_view hostBasePath, std::string_view childName)
    {
        return ToHostPath(PathUtility::join(ToExposedPath(hostBasePath), ToExposedPath(childName)));
    }

    inline std::string JoinExposedPath(std::string_view exposedBasePath, std::string_view childName)
    {
        return PathUtility::join(ToExposedPath(exposedBasePath), ToExposedPath(childName));
    }

    template <typename Callback>
    inline bool EnumerateHostDirectory(std::string_view hostDirectoryPath, Callback &&callback)
    {
        const std::string searchPattern = JoinHostPath(hostDirectoryPath, "*");

        WIN32_FIND_DATAA findData;
        HANDLE handle = FindFirstFileA(searchPattern.c_str(), &findData);
        if (handle == INVALID_HANDLE_VALUE)
        {
            return GetLastError() == ERROR_FILE_NOT_FOUND;
        }

        DWORD lastError = ERROR_SUCCESS;
        do
        {
            const std::string nameStr(findData.cFileName);
            if (nameStr == "." || nameStr == "..")
            {
                continue;
            }

            const bool isDirectory = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
            const std::string nameForCallback = ToExposedPath(nameStr);
            if (!callback(std::string_view(nameForCallback), isDirectory))
            {
                FindClose(handle);
                return true;
            }
        } while (FindNextFileA(handle, &findData) != 0);

        lastError = GetLastError();
        FindClose(handle);
        return lastError == ERROR_NO_MORE_FILES;
    }

    class WindowsFile : public IFile
    {
    public:
        WindowsFile(std::unique_ptr<std::fstream> stream, std::string hostPath, std::string exposedPath, bool directory,
                    std::optional<std::size_t> size, std::optional<uint32_t> lastWrite, FileOpenMode mode)
            : stream_(std::move(stream)), hostPath_(std::move(hostPath)), exposedPath_(std::move(exposedPath)),
                    name_(std::string(PathUtility::getFileName(exposedPath_))), directory_(directory), size_(size),
                            lastWrite_(lastWrite), mode_(mode)
        {
        }

        ~WindowsFile() override
        {
            close();
        }

        ByteAvailability available() override
        {
            if (directory_)
            {
                return ExhaustedResult();
            }

            if (stream_ == nullptr || !isReadable())
            {
                return ExhaustedResult();
            }

            if (!size_.has_value())
            {
                return TemporarilyUnavailableResult();
            }

            if (position_ >= *size_)
            {
                return ExhaustedResult();
            }

            return AvailableBytes(*size_ - position_);
        }

        size_t read(std::span<uint8_t> buffer) override
        {
            if (directory_ || stream_ == nullptr || !isReadable() || buffer.empty())
            {
                return 0;
            }

            if (size_.has_value() && position_ >= *size_)
            {
                return 0;
            }

            if (!seekReadPosition())
            {
                return 0;
            }

            const std::streamsize chunkSize = static_cast<std::streamsize>(
                (std::min)(buffer.size(), static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max())));
            stream_->read(reinterpret_cast<char *>(buffer.data()), chunkSize);
            const std::streamsize bytesRead = stream_->gcount();
            if (bytesRead <= 0)
            {
                return 0;
            }

            position_ += static_cast<std::size_t>(bytesRead);
            return static_cast<std::size_t>(bytesRead);
        }

        size_t peek(std::span<uint8_t> buffer) override
        {
            if (directory_ || stream_ == nullptr || !isReadable() || buffer.empty())
            {
                return 0;
            }

            if (size_.has_value() && position_ >= *size_)
            {
                return 0;
            }

            if (!seekReadPosition())
            {
                return 0;
            }

            const std::streamsize chunkSize = static_cast<std::streamsize>(
                (std::min)(buffer.size(), static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max())));
            stream_->read(reinterpret_cast<char *>(buffer.data()), chunkSize);
            const std::streamsize bytesRead = stream_->gcount();
            if (bytesRead <= 0)
            {
                return 0;
            }

            return static_cast<std::size_t>(bytesRead);
        }

        std::size_t write(std::span<const uint8_t> buffer) override
        {
            if (stream_ == nullptr || directory_ || !isWritable() || buffer.empty())
            {
                return 0;
            }

            if (!seekWritePosition())
            {
                return 0;
            }

            const std::streamsize chunkSize = static_cast<std::streamsize>(
                (std::min)(buffer.size(), static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max())));
            stream_->write(reinterpret_cast<const char *>(buffer.data()), chunkSize);
            if (!stream_->good())
            {
                return 0;
            }

            position_ += static_cast<std::size_t>(chunkSize);
            size_ = (std::max)(size_.value_or(0), position_);
            return static_cast<std::size_t>(chunkSize);
        }

        void flush() override
        {
            if (stream_ != nullptr && !directory_ && isWritable())
            {
                stream_->flush();
                refreshMetadata();
            }
        }

        bool isDirectory() const override
        {
            return directory_;
        }

        void close() override
        {
            if (stream_ != nullptr)
            {
                if (isWritable())
                {
                    stream_->flush();
                }

                stream_->close();
                stream_.reset();
                refreshMetadata();
            }
        }

        std::optional<std::size_t> size() const override
        {
            return size_;
        }

        std::string_view name() const override
        {
            return name_;
        }

        std::string_view path() const override
        {
            return exposedPath_;
        }

        std::optional<uint32_t> lastWriteEpochSeconds() const override
        {
            return lastWrite_;
        }

    private:
        bool isReadable() const
        {
            return mode_ == FileOpenMode::Read || mode_ == FileOpenMode::ReadWrite;
        }

        bool isWritable() const
        {
            return mode_ == FileOpenMode::Write || mode_ == FileOpenMode::ReadWrite;
        }

        bool seekReadPosition()
        {
            if (stream_ == nullptr)
            {
                return false;
            }

            stream_->clear();
            stream_->seekg(static_cast<std::streamoff>(position_), std::ios::beg);
            return static_cast<bool>(*stream_);
        }

        bool seekWritePosition()
        {
            if (stream_ == nullptr)
            {
                return false;
            }

            stream_->clear();
            stream_->seekp(static_cast<std::streamoff>(position_), std::ios::beg);
            return static_cast<bool>(*stream_);
        }

        void refreshMetadata()
        {
            if (directory_)
            {
                return;
            }

            const FileMetadata metadata = ReadHostMetadata(hostPath_);
            if (metadata.exists)
            {
                size_ = metadata.size;
                lastWrite_ = metadata.lastWrite;
            }
        }

        std::unique_ptr<std::fstream> stream_;
        std::string hostPath_;
        std::string exposedPath_;
        std::string name_;
        bool directory_ = false;
        std::optional<std::size_t> size_;
        std::optional<uint32_t> lastWrite_;
        FileOpenMode mode_ = FileOpenMode::Read;
        std::size_t position_ = 0;
    };

    class WindowsFs : public IFileSystem
    {
    public:
        WindowsFs() = default;

        explicit WindowsFs(std::string_view /*rootPath*/)
        {
        }

        std::string normalizePath(std::string_view path) const override
        {
            if (path.empty())
            {
                return std::string();
            }

            auto readEnvVar = [](const char *name) -> std::optional<std::string>
            {
                const DWORD required = GetEnvironmentVariableA(name, nullptr, 0);
                if (required == 0)
                {
                    return std::nullopt;
                }

                std::string value(required - 1, '\0');
                GetEnvironmentVariableA(name, value.data(), required);
                return value;
            };

            auto replaceAll = [](std::string &source, std::string_view token, std::string_view replacement)
            {
                std::size_t pos = 0;
                while ((pos = source.find(token, pos)) != std::string::npos)
                {
                    source.replace(pos, token.length(), replacement);
                    pos += replacement.length();
                }
            };

            std::string normalized(path);
            if (const auto appData = readEnvVar("APPDATA"); appData.has_value())
            {
                replaceAll(normalized, "%APPDATA%", appData.value());
            }
            if (const auto localAppData = readEnvVar("LOCALAPPDATA"); localAppData.has_value())
            {
                replaceAll(normalized, "%LOCALAPPDATA%", localAppData.value());
            }

            if (!normalized.empty() && normalized.front() == '.' &&
                (normalized.size() == 1 || normalized[1] == '/' || normalized[1] == '\\'))
            {
                const DWORD cwdSize = GetCurrentDirectoryA(0, nullptr);
                if (cwdSize != 0)
                {
                    std::string cwd(cwdSize - 1, '\0');
                    GetCurrentDirectoryA(cwdSize, cwd.data());
                    const std::string cwdExposed = ToExposedPath(cwd);

                    if (normalized.size() == 1)
                    {
                        return cwdExposed;
                    }

                    const std::string remainder = normalized.substr(2);
                    return PathUtility::join(cwdExposed, ToExposedPath(remainder));
                }
            }

            return ToExposedPath(normalized);
        }

        bool exists(std::string_view path) const override
        {
            if (path.empty())
            {
                return false;
            }

            return ReadHostMetadata(toHostPath(path)).exists;
        }

        bool mkdir(std::string_view path) override
        {
            if (path.empty())
            {
                return false;
            }

            return CreateHostDirectory(toHostPath(path));
        }

        FileHandle open(std::string_view path, FileOpenMode mode) override
        {
            if (path.empty())
            {
                return nullptr;
            }

            const std::string exposedPath = ToExposedPath(path);
            std::string hostPath = toHostPath(path);
            const FileMetadata metadata = ReadHostMetadata(hostPath);
            if (!metadata.exists && mode == FileOpenMode::Read)
            {
                return nullptr;
            }

            if (metadata.directory)
            {
                return std::make_unique<WindowsFile>(nullptr, std::move(hostPath), exposedPath, true,
                                                     std::nullopt, metadata.lastWrite, mode);
            }

            auto stream = std::make_unique<std::fstream>();
            stream->open(hostPath, toOpenMode(mode, metadata.exists));
            if (!stream->is_open())
            {
                return nullptr;
            }

            const FileMetadata refreshedMetadata = ReadHostMetadata(hostPath);
            return std::make_unique<WindowsFile>(std::move(stream), std::move(hostPath), exposedPath, false,
                                                 refreshedMetadata.size, refreshedMetadata.lastWrite, mode);
        }

        bool list(std::string_view directoryPath, const DirectoryEntryCallback &callback,
                  bool recursive = false) override
        {
            if (directoryPath.empty() || !callback)
            {
                return false;
            }

            return listResolved(toHostPath(directoryPath), ToExposedPath(directoryPath), callback, recursive);
        }

        bool remove(std::string_view path) override
        {
            if (path.empty())
            {
                return false;
            }

            const std::string hostPath = toHostPath(path);
            const FileMetadata metadata = ReadHostMetadata(hostPath);
            if (!metadata.exists)
            {
                return false;
            }

            return RemoveHostPath(hostPath, metadata.directory);
        }

        bool rename(std::string_view from, std::string_view to) override
        {
            if (from.empty() || to.empty())
            {
                return false;
            }

            return RenameHostPath(toHostPath(from), toHostPath(to));
        }

        std::unique_ptr<IFileSystem> getScoped(std::string_view rootPath) override
        {
            if (rootPath.empty())
            {
                return nullptr;
            }

            const FileMetadata metadata = ReadHostMetadata(toHostPath(rootPath));
            if (!metadata.exists || !metadata.directory)
            {
                return nullptr;
            }

            return std::make_unique<lumalink::platform::filesystem::ScopedFileSystem>(
                std::string(rootPath), *this);
        }

    private:
        std::string toHostPath(std::string_view exposedPath) const
        {
            return ToHostPath(exposedPath);
        }

        bool listResolved(std::string_view hostDirectoryPath, std::string_view exposedDirectoryPath,
                          const DirectoryEntryCallback &callback, const bool recursive) const
        {
            const FileMetadata metadata = ReadHostMetadata(hostDirectoryPath);
            if (!metadata.exists || !metadata.directory)
            {
                return false;
            }

            return EnumerateHostDirectory(
                hostDirectoryPath,
                [this, &hostDirectoryPath, &exposedDirectoryPath, &callback, recursive](std::string_view name,
                                                                                        const bool isDirectory)
                {
                    const std::string entryHostPath = JoinHostPath(hostDirectoryPath, name);
                    const std::string entryExposedPath = JoinExposedPath(exposedDirectoryPath, name);
                    const DirectoryEntry entry{std::string(name), entryExposedPath, isDirectory};
                    if (!callback(entry))
                    {
                        return false;
                    }

                    if (recursive && isDirectory)
                    {
                        return listResolved(entryHostPath, entryExposedPath, callback, true);
                    }

                    return true;
                });
        }

        static std::ios::openmode toOpenMode(FileOpenMode mode, bool pathExists)
        {
            switch (mode)
            {
            case FileOpenMode::Read:
                return std::ios::binary | std::ios::in;

            case FileOpenMode::Write:
                return std::ios::binary | std::ios::out | std::ios::trunc;

            case FileOpenMode::ReadWrite:
            default:
                return pathExists ? (std::ios::binary | std::ios::in | std::ios::out)
                                  : (std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);
            }
        }
    };

} // namespace lumalink::platform::windows
