#pragma once

#include "FileSystem.h"
#include "PathUtility.h"

#include <memory>
#include <string>
#include <string_view>

namespace lumalink::platform::filesystem
{
    namespace detail
    {
        class ScopedFile final : public IFile
        {
        public:
            ScopedFile(FileHandle innerFile, std::string exposedPath)
                : innerFile_(std::move(innerFile)), exposedPath_(std::move(exposedPath))
            {
            }

            bool isDirectory() const override
            {
                return innerFile_->isDirectory();
            }

            void close() override
            {
                innerFile_->close();
            }

            std::optional<std::size_t> size() const override
            {
                return innerFile_->size();
            }

            std::string_view name() const override
            {
                return innerFile_->name();
            }

            std::string_view path() const override
            {
                return exposedPath_;
            }

            std::optional<uint32_t> lastWriteEpochSeconds() const override
            {
                return innerFile_->lastWriteEpochSeconds();
            }

            buffers::ByteAvailability available() override
            {
                return innerFile_->available();
            }

            std::size_t read(std::span<uint8_t> buffer) override
            {
                return innerFile_->read(buffer);
            }

            std::size_t peek(std::span<uint8_t> buffer) override
            {
                return innerFile_->peek(buffer);
            }

            std::size_t write(std::span<const uint8_t> buffer) override
            {
                return innerFile_->write(buffer);
            }

            void flush() override
            {
                innerFile_->flush();
            }

        private:
            FileHandle innerFile_;
            std::string exposedPath_;
        };
    }

    class ScopedFileSystem final : public IFileSystem
    {
    public:
        ScopedFileSystem(std::string_view rootPath, std::unique_ptr<IFileSystem> innerFileSystem)
            : rootPath_(rootPath), innerFileSystem_(std::move(innerFileSystem))
        {
        }

        std::string normalizePath(std::string_view path) const override
        {
            return innerFileSystem_ != nullptr ? innerFileSystem_->normalizePath(path) : std::string(path);
        }

        bool exists(std::string_view path) const override
        {
            return innerFileSystem_ != nullptr && !path.empty() && innerFileSystem_->exists(scopePath(path));
        }

        bool mkdir(std::string_view path) override
        {
            return innerFileSystem_ != nullptr && !path.empty() && innerFileSystem_->mkdir(scopePath(path));
        }

        FileHandle open(std::string_view path, FileOpenMode mode) override
        {
            if (innerFileSystem_ == nullptr || path.empty())
            {
                return nullptr;
            }

            FileHandle innerFile = innerFileSystem_->open(scopePath(path), mode);
            if (!innerFile)
            {
                return nullptr;
            }

            return std::make_unique<detail::ScopedFile>(std::move(innerFile), exposePath(innerFile->path()));
        }

        bool remove(std::string_view path) override
        {
            return innerFileSystem_ != nullptr && !path.empty() && innerFileSystem_->remove(scopePath(path));
        }

        bool rename(std::string_view from, std::string_view to) override
        {
            return innerFileSystem_ != nullptr && !from.empty() && !to.empty() &&
                   innerFileSystem_->rename(scopePath(from), scopePath(to));
        }

        bool list(std::string_view directoryPath,
                  const DirectoryEntryCallback &callback,
                  bool recursive = false) override
        {
            if (innerFileSystem_ == nullptr || directoryPath.empty() || !callback)
            {
                return false;
            }

            return innerFileSystem_->list(
                scopePath(directoryPath),
                [this, &callback](const DirectoryEntry &entry)
                {
                    const std::string exposedPath = exposePath(entry.path);
                    const std::string_view nameView = exposedPath == "/"
                        ? std::string_view(exposedPath)
                        : PathUtility::getFileName(exposedPath);
                    const DirectoryEntry scopedEntry{std::string(nameView), exposedPath, entry.isDirectory};
                    return callback(scopedEntry);
                },
                recursive);
        }

    private:
        std::string scopePath(std::string_view path) const
        {
            std::string relativePath = std::string(PathUtility::makeRelative(path, "/"));
            while (!relativePath.empty() && relativePath.front() == '/')
            {
                relativePath = std::string(PathUtility::makeRelative(relativePath, "/"));
            }

            if (relativePath.empty())
            {
                return rootPath_;
            }

            if (rootPath_ == "/")
            {
                return PathUtility::join("/", relativePath);
            }

            return PathUtility::join(rootPath_, relativePath);
        }

        std::string exposePath(std::string_view path) const
        {
            const std::string exposedInnerPath(path);
            if (rootPath_ == "/")
            {
                return exposedInnerPath;
            }

            if (exposedInnerPath == rootPath_)
            {
                return "/";
            }

            const std::string_view relative =
                PathUtility::makeRelative(exposedInnerPath, rootPath_, true);
            if (relative != exposedInnerPath)
            {
                return std::string(relative);
            }

            return exposedInnerPath;
        }

        std::string rootPath_;
        std::unique_ptr<IFileSystem> innerFileSystem_;
    };
}
