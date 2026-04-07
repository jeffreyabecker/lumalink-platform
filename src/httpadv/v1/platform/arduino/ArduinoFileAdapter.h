#pragma once

#include "../../transport/IFileSystem.h"

#include <memory>
#include <string>
#include <string_view>

#if defined(ARDUINO) && __has_include(<FS.h>)
#include <FS.h>
#include "../PathMapper.h"

#include <optional>
#include <utility>

namespace httpadv::v1::platform::arduino
{
            inline std::optional<std::size_t> FileSize(File& file, const bool isDirectory)
            {
                if (!file || isDirectory)
                {
                    return std::nullopt;
                }

                return static_cast<std::size_t>(file.size());
            }

            inline std::optional<uint32_t> LastWriteSeconds(File& file)
            {
                if (!file)
                {
                    return std::nullopt;
                }

                const time_t lastWrite = file.getLastWrite();
                if (lastWrite < 0)
                {
                    return std::nullopt;
                }

                return static_cast<uint32_t>(lastWrite);
            }

            inline std::string CopyCString(const char* value)
            {
                return value != nullptr ? std::string(value) : std::string();
            }

            class ArduinoFile : public IFile
            {
              public:
                ArduinoFile(FS& fileSystem, File file, std::string fsPath, std::string path, bool directory,
                            std::optional<std::size_t> size, std::optional<uint32_t> lastWrite, FileOpenMode mode)
                    : fileSystem_(fileSystem), file_(std::move(file)), fsPath_(std::move(fsPath)), path_(std::move(path)),
                      name_(std::string(httpadv::v1::platform::PosixPathMapper::BasenameView(path_))), directory_(directory), size_(size),
                      lastWrite_(lastWrite), mode_(mode)
                {
                }

                ~ArduinoFile() override
                {
                    close();
                }

                AvailableResult available() override
                {
                    if (directory_ || !file_ || !isReadable())
                    {
                        return ExhaustedResult();
                    }

                    const int availableBytes = file_.available();
                    if (availableBytes > 0)
                    {
                        return AvailableBytes(static_cast<std::size_t>(availableBytes));
                    }

                    if (availableBytes < 0)
                    {
                        return ErrorResult(availableBytes);
                    }

                    return atEndOfFile() ? ExhaustedResult() : TemporarilyUnavailableResult();
                }

                size_t read(httpadv::v1::util::span<uint8_t> buffer) override
                {
                    if (directory_ || !file_ || !isReadable() || buffer.empty())
                    {
                        return 0;
                    }

                    const int bytesRead = file_.read(buffer.data(), buffer.size());
                    if (bytesRead <= 0)
                    {
                        return 0;
                    }

                    return static_cast<std::size_t>(bytesRead);
                }

                size_t peek(httpadv::v1::util::span<uint8_t> buffer) override
                {
                    if (directory_ || !file_ || !isReadable() || buffer.empty())
                    {
                        return 0;
                    }

                    const std::size_t originalPosition = file_.position();
                    const int bytesRead = file_.read(buffer.data(), buffer.size());
                    if (!file_.seek(static_cast<uint32_t>(originalPosition), SeekSet) || bytesRead <= 0)
                    {
                        return 0;
                    }

                    return static_cast<std::size_t>(bytesRead);
                }

                std::size_t write(httpadv::v1::util::span<const uint8_t> buffer) override
                {
                    if (directory_ || !file_ || !isWritable() || buffer.empty())
                    {
                        return 0;
                    }

                    const std::size_t bytesWritten = file_.write(buffer.data(), buffer.size());
                    if (bytesWritten == 0)
                    {
                        return 0;
                    }

                    size_ = static_cast<std::size_t>(file_.size());
                    return bytesWritten;
                }

                void flush() override
                {
                    if (!directory_ && file_ && isWritable())
                    {
                        file_.flush();
                        refreshMetadata();
                    }
                }

                bool isDirectory() const override
                {
                    return directory_;
                }

                void close() override
                {
                    if (file_)
                    {
                        if (isWritable())
                        {
                            file_.flush();
                        }

                        refreshMetadata();
                        file_.close();
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

                bool atEndOfFile() const
                {
                    return size_.has_value() && file_.position() >= *size_;
                }

                void refreshMetadata()
                {
                    if (directory_)
                    {
                        return;
                    }

                    File metadataFile = fileSystem_.open(fsPath_.c_str(), "r");
                    if (!metadataFile)
                    {
                        return;
                    }

                    size_ = FileSize(metadataFile, false);
                    lastWrite_ = LastWriteSeconds(metadataFile);
                }

                FS& fileSystem_;
                File file_;
                std::string fsPath_;
                std::string path_;
                std::string name_;
                bool directory_ = false;
                std::optional<std::size_t> size_;
                std::optional<uint32_t> lastWrite_;
                FileOpenMode mode_ = FileOpenMode::Read;
            };

            class ArduinoFS : public IFileSystem
            {
              public:
                explicit ArduinoFS(FS& fileSystem)
                    : fileSystem_(fileSystem), mapper_(httpadv::v1::platform::PosixPathMapper::NormalizeRootPath("/"))
                {
                }

                ArduinoFS(FS& fileSystem, std::string_view rootPath)
                    : fileSystem_(fileSystem), mapper_(httpadv::v1::platform::PosixPathMapper::NormalizeRootPath(rootPath))
                {
                }

                bool exists(std::string_view path) const override
                {
                    if (path.empty())
                    {
                        return false;
                    }

                    const std::string fsPath = resolveFsPath(path);
                    return fileSystem_.exists(fsPath.c_str());
                }

                bool mkdir(std::string_view path) override
                {
                    if (path.empty())
                    {
                        return false;
                    }

                    const std::string fsPath = resolveFsPath(path);
                    return fileSystem_.mkdir(fsPath.c_str());
                }

                FileHandle open(std::string_view path, FileOpenMode mode) override
                {
                    if (path.empty())
                    {
                        return nullptr;
                    }

                    const std::string fsPath = resolveFsPath(path);
                    const std::string exposedPath = exposePath(path);
                    File metadataFile = fileSystem_.open(fsPath.c_str(), "r");
                    const bool existed = static_cast<bool>(metadataFile);
                    const bool isDirectory = metadataFile && metadataFile.isDirectory();
                    const std::optional<std::size_t> size = FileSize(metadataFile, isDirectory);
                    const std::optional<uint32_t> lastWrite = LastWriteSeconds(metadataFile);

                    if (!existed && mode == FileOpenMode::Read)
                    {
                        return nullptr;
                    }

                    if (isDirectory)
                    {
                        return std::make_unique<ArduinoFile>(fileSystem_, std::move(metadataFile), fsPath, exposedPath,
                                                             true, std::nullopt, lastWrite, mode);
                    }

                    metadataFile.close();
                    File file = fileSystem_.open(fsPath.c_str(), openMode(mode, existed));
                    if (!file)
                    {
                        return nullptr;
                    }

                    const bool openedDirectory = file.isDirectory();
                    return std::make_unique<ArduinoFile>(fileSystem_, std::move(file), fsPath, exposedPath,
                                                         openedDirectory, openedDirectory ? std::nullopt : size,
                                                         lastWrite, mode);
                }

                bool remove(std::string_view path) override
                {
                    if (path.empty())
                    {
                        return false;
                    }

                    const std::string fsPath = resolveFsPath(path);
                    File file = fileSystem_.open(fsPath.c_str(), "r");
                    if (!file)
                    {
                        return false;
                    }

                    const bool isDirectory = file.isDirectory();
                    file.close();
                    return isDirectory ? fileSystem_.rmdir(fsPath.c_str()) : fileSystem_.remove(fsPath.c_str());
                }

                bool rename(std::string_view from, std::string_view to) override
                {
                    if (from.empty() || to.empty())
                    {
                        return false;
                    }

                    const std::string fromPath = resolveFsPath(from);
                    const std::string toPath = resolveFsPath(to);
                    return fileSystem_.rename(fromPath.c_str(), toPath.c_str());
                }

                bool list(std::string_view directoryPath, const DirectoryEntryCallback& callback,
                          bool recursive = false) override
                {
                    if (directoryPath.empty() || !callback)
                    {
                        return false;
                    }

                    return listResolved(resolveFsPath(directoryPath), exposePath(directoryPath), callback, recursive);
                }

              private:
                std::string resolveFsPath(std::string_view path) const
                {
                    return mapper_.resolveScopedPath(path);
                }

                std::string exposePath(std::string_view path) const
                {
                    return mapper_.exposePath(path);
                }

                bool listResolved(std::string_view fsDirectoryPath, std::string_view virtualDirectoryPath,
                                  const DirectoryEntryCallback& callback, const bool recursive)
                {
                    File directory = fileSystem_.open(std::string(fsDirectoryPath).c_str(), "r");
                    if (!directory || !directory.isDirectory())
                    {
                        return false;
                    }

                    directory.rewindDirectory();
                    while (true)
                    {
                        File entry = directory.openNextFile();
                        if (!entry)
                        {
                            break;
                        }

                        std::string entryFsPath = CopyCString(entry.fullName());
                        if (entryFsPath.empty())
                        {
                            entryFsPath = httpadv::v1::platform::PosixPathMapper::JoinScopedPath(fsDirectoryPath, CopyCString(entry.name()));
                        }

                        const std::string entryVirtualPath = httpadv::v1::platform::PosixPathMapper::Join(virtualDirectoryPath, CopyCString(entry.name()));
                        const DirectoryEntry directoryEntry{std::string(httpadv::v1::platform::PosixPathMapper::BasenameView(entryVirtualPath)),
                                                            entryVirtualPath, entry.isDirectory()};
                        if (!callback(directoryEntry))
                        {
                            entry.close();
                            directory.close();
                            return true;
                        }

                        if (recursive && entry.isDirectory() && !listResolved(entryFsPath, entryVirtualPath, callback, true))
                        {
                            entry.close();
                            directory.close();
                            return false;
                        }

                        entry.close();
                    }

                    directory.close();
                    return true;
                }

                static const char* openMode(FileOpenMode mode, const bool pathExists)
                {
                    switch (mode)
                    {
                        case FileOpenMode::Read:
                            return "r";

                        case FileOpenMode::Write:
                            return "w";

                        case FileOpenMode::ReadWrite:
                        default:
                            return pathExists ? "r+" : "w+";
                    }
                }

                FS& fileSystem_;
                httpadv::v1::platform::PosixPathMapper mapper_{};
            };
} // namespace httpadv::v1::platform::arduino

#else

namespace httpadv::v1::platform::arduino
{
    class FS;

} // namespace httpadv::v1::platform::arduino

#endif