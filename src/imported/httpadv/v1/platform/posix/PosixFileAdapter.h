#pragma once

#include "../../transport/IFileSystem.h"

#include "../PathMapper.h"

#include <dirent.h>
#include <sys/stat.h>

#include <algorithm>
#include <cerrno>
#include <fstream>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace httpadv::v1::platform::posix
{
            struct FileMetadata
            {
                bool exists = false;
                bool directory = false;
                std::optional<std::size_t> size;
                std::optional<uint32_t> lastWrite;
            };

            inline FileMetadata ReadMetadata(std::string_view path)
            {
                FileMetadata metadata;
                const std::string ownedPath(path);

                struct stat fileStatus;
                if (stat(ownedPath.c_str(), &fileStatus) != 0)
                {
                    return metadata;
                }

                metadata.exists = true;
                metadata.directory = S_ISDIR(fileStatus.st_mode) != 0;
                if (!metadata.directory)
                {
                    metadata.size = static_cast<std::size_t>(fileStatus.st_size);
                }

                metadata.lastWrite = static_cast<uint32_t>(fileStatus.st_mtime);
                return metadata;
            }

            inline bool CreateHostDirectory(std::string_view path)
            {
                const std::string ownedPath(path);
                if (::mkdir(ownedPath.c_str(), 0777) == 0)
                {
                    return true;
                }

                if (errno == EEXIST)
                {
                    const FileMetadata metadata = ReadMetadata(ownedPath);
                    return metadata.exists && metadata.directory;
                }

                return false;
            }

            inline bool RemoveHostPath(std::string_view path, bool /*isDirectory*/)
            {
                const std::string ownedPath(path);
                return ::remove(ownedPath.c_str()) == 0;
            }

            inline bool RenameHostPath(std::string_view from, std::string_view to)
            {
                const std::string ownedFrom(from);
                const std::string ownedTo(to);
                return ::rename(ownedFrom.c_str(), ownedTo.c_str()) == 0;
            }

            template <typename Callback>
            inline bool EnumerateHostDirectory(std::string_view directoryPath, Callback&& callback)
            {
                const std::string ownedDirectory(directoryPath);
                DIR* directory = opendir(ownedDirectory.c_str());
                if (directory == nullptr)
                {
                    return false;
                }

                while (dirent* current = readdir(directory))
                {
                    const std::string_view name(current->d_name);
                    if (name == "." || name == "..")
                    {
                        continue;
                    }

                    const std::string entryHostPath =
                        httpadv::v1::platform::PosixPathMapper::JoinScopedPath(ownedDirectory, name);
                    bool isDirectory = false;
#if defined(DT_DIR) && defined(DT_UNKNOWN)
                    if (current->d_type == DT_DIR)
                    {
                        isDirectory = true;
                    }
                    else if (current->d_type == DT_UNKNOWN)
                    {
                        const FileMetadata entryMetadata = ReadMetadata(entryHostPath);
                        isDirectory = entryMetadata.exists && entryMetadata.directory;
                    }
#else
                    const FileMetadata entryMetadata = ReadMetadata(entryHostPath);
                    isDirectory = entryMetadata.exists && entryMetadata.directory;
#endif

                    if (!callback(name, isDirectory))
                    {
                        closedir(directory);
                        return true;
                    }
                }

                closedir(directory);
                return true;
            }

            class PosixFile : public IFile
            {
              public:
                PosixFile(std::unique_ptr<std::fstream> stream, std::string hostPath, std::string path, bool directory,
                           std::optional<std::size_t> size, std::optional<uint32_t> lastWrite, FileOpenMode mode)
                    : stream_(std::move(stream)), hostPath_(std::move(hostPath)), path_(std::move(path)),
                      directory_(directory), size_(size), lastWrite_(lastWrite), mode_(mode)
                {
                }

                ~PosixFile() override
                {
                    close();
                }

                AvailableResult available() override
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

                size_t read(httpadv::v1::util::span<uint8_t> buffer) override
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
                    stream_->read(reinterpret_cast<char*>(buffer.data()), chunkSize);
                    const std::streamsize bytesRead = stream_->gcount();
                    if (bytesRead <= 0)
                    {
                        return 0;
                    }

                    position_ += static_cast<std::size_t>(bytesRead);
                    return static_cast<std::size_t>(bytesRead);
                }

                size_t peek(httpadv::v1::util::span<uint8_t> buffer) override
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
                    stream_->read(reinterpret_cast<char*>(buffer.data()), chunkSize);
                    const std::streamsize bytesRead = stream_->gcount();
                    if (bytesRead <= 0)
                    {
                        return 0;
                    }

                    return static_cast<std::size_t>(bytesRead);
                }

                std::size_t write(httpadv::v1::util::span<const uint8_t> buffer) override
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
                    stream_->write(reinterpret_cast<const char*>(buffer.data()), chunkSize);
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
                    return httpadv::v1::platform::PosixPathMapper::BasenameView(path_);
                }

                std::string_view path() const override
                {
                    return path_;
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

                    const FileMetadata metadata = ReadMetadata(hostPath_);
                    if (metadata.exists)
                    {
                        size_ = metadata.size;
                        lastWrite_ = metadata.lastWrite;
                    }
                }

                std::unique_ptr<std::fstream> stream_;
                std::string hostPath_;
                std::string path_;
                bool directory_ = false;
                std::optional<std::size_t> size_;
                std::optional<uint32_t> lastWrite_;
                FileOpenMode mode_ = FileOpenMode::Read;
                std::size_t position_ = 0;
            };

            class PosixFS : public IFileSystem
            {
              public:
                PosixFS() = default;

                explicit PosixFS(std::string_view rootPath)
                    : mapper_(rootPath)
                {
                }

                bool exists(std::string_view path) const override
                {
                    if (path.empty())
                    {
                        return false;
                    }

                    return ReadMetadata(resolveHostPath(path)).exists;
                }

                bool mkdir(std::string_view path) override
                {
                    if (path.empty())
                    {
                        return false;
                    }

                    return CreateHostDirectory(resolveHostPath(path));
                }

                FileHandle open(std::string_view path, FileOpenMode mode) override
                {
                    if (path.empty())
                    {
                        return nullptr;
                    }

                    const std::string virtualPath = exposePath(path);
                    std::string ownedPath = resolveHostPath(path);
                    const FileMetadata metadata = ReadMetadata(ownedPath);
                    if (!metadata.exists && mode == FileOpenMode::Read)
                    {
                        return nullptr;
                    }

                    if (metadata.directory)
                    {
                        return std::make_unique<PosixFile>(nullptr, std::move(ownedPath), virtualPath, true,
                                                            std::nullopt, metadata.lastWrite, mode);
                    }

                    auto stream = std::make_unique<std::fstream>();
                    stream->open(ownedPath, toOpenMode(mode, metadata.exists));
                    if (!stream->is_open())
                    {
                        return nullptr;
                    }

                    const FileMetadata refreshedMetadata = ReadMetadata(ownedPath);
                    return std::make_unique<PosixFile>(std::move(stream), std::move(ownedPath), virtualPath, false,
                                                        refreshedMetadata.size, refreshedMetadata.lastWrite, mode);
                }

                bool list(std::string_view directoryPath, const DirectoryEntryCallback& callback,
                          bool recursive = false) override
                {
                    if (directoryPath.empty() || !callback)
                    {
                        return false;
                    }

                    return listResolved(resolveHostPath(directoryPath), exposePath(directoryPath), callback, recursive);
                }

                bool remove(std::string_view path) override
                {
                    if (path.empty())
                    {
                        return false;
                    }

                    const std::string ownedPath = resolveHostPath(path);
                    const FileMetadata metadata = ReadMetadata(ownedPath);
                    if (!metadata.exists)
                    {
                        return false;
                    }

                    return RemoveHostPath(ownedPath, metadata.directory);
                }

                bool rename(std::string_view from, std::string_view to) override
                {
                    if (from.empty() || to.empty())
                    {
                        return false;
                    }

                    return RenameHostPath(resolveHostPath(from), resolveHostPath(to));
                }

              private:
                std::string resolveHostPath(std::string_view path) const
                {
                    return mapper_.resolveScopedPath(path);
                }

                std::string exposePath(std::string_view path) const
                {
                    return mapper_.exposePath(path);
                }

                bool listResolved(std::string_view hostDirectoryPath, std::string_view virtualDirectoryPath,
                                  const DirectoryEntryCallback& callback, const bool recursive) const
                {
                    const FileMetadata metadata = ReadMetadata(hostDirectoryPath);
                    if (!metadata.exists || !metadata.directory)
                    {
                        return false;
                    }

                    return EnumerateHostDirectory(
                        hostDirectoryPath,
                        [this, &hostDirectoryPath, &virtualDirectoryPath, &callback, recursive](std::string_view name,
                                                                                                const bool isDirectory)
                        {
                            const std::string entryHostPath = httpadv::v1::platform::PosixPathMapper::JoinScopedPath(hostDirectoryPath, name);
                            const std::string entryVirtualPath = httpadv::v1::platform::PosixPathMapper::Join(virtualDirectoryPath, name);
                            const DirectoryEntry entry{std::string(name), entryVirtualPath, isDirectory};
                            if (!callback(entry))
                            {
                                return false;
                            }

                            if (recursive && isDirectory)
                            {
                                return listResolved(entryHostPath, entryVirtualPath, callback, true);
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

                httpadv::v1::platform::PosixPathMapper mapper_{};
            };
            
} // namespace httpadv::v1::platform::posix
