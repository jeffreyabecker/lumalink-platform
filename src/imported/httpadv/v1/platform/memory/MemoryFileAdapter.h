#pragma once

#include "../../transport/IFileSystem.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace httpadv::v1::platform::memory
{
    using httpadv::v1::transport::AvailableBytes;
    using httpadv::v1::transport::AvailableResult;
    using httpadv::v1::transport::DirectoryEntry;
    using httpadv::v1::transport::DirectoryEntryCallback;
    using httpadv::v1::transport::ExhaustedResult;
    using httpadv::v1::transport::FileHandle;
    using httpadv::v1::transport::IFile;
    using httpadv::v1::transport::FileOpenMode;
    using httpadv::v1::transport::IFileSystem;

    class MemoryFile ;
    class MemoryFileSystem : public IFileSystem
    {
    public:
        MemoryFileSystem()
        {
            root_ = std::make_shared<Node>();
            root_->name = "/";
            root_->path = "/";
            root_->isDirectory = true;
            root_->lastWrite = static_cast<uint32_t>(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
        }

        ~MemoryFileSystem() override = default;

        bool exists(std::string_view path) const override
        {
            return !!findNode(path);
        }

        bool mkdir(std::string_view path) override
        {
            createPath(path, true);
            return true;
        }

        FileHandle open(std::string_view path, FileOpenMode mode) override;

        bool remove(std::string_view path) override
        {
            if (path.empty() || path == "/")
            {
                return false;
            }

            std::string p(path);
            if (p.front() == '/')
            {
                p.erase(0, 1);
            }

            std::vector<std::string> parts;
            size_t pos = 0;
            while (pos < p.size())
            {
                auto next = p.find('/', pos);
                if (next == std::string::npos)
                {
                    parts.push_back(p.substr(pos));
                    break;
                }
                parts.push_back(p.substr(pos, next - pos));
                pos = next + 1;
            }

            auto parent = root_;
            for (size_t i = 0; i + 1 < parts.size(); ++i)
            {
                auto it = parent->children.find(parts[i]);
                if (it == parent->children.end())
                {
                    return false;
                }
                parent = it->second;
            }

            const std::string &target = parts.back();
            auto it = parent->children.find(target);
            if (it == parent->children.end())
            {
                return false;
            }

            parent->children.erase(it);
            parent->lastWrite = static_cast<uint32_t>(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
            return true;
        }

        bool rename(std::string_view from, std::string_view to) override
        {
            auto src = findNode(from);
            if (!src)
            {
                return false;
            }

            // Remove from old parent
            std::string sf(from);
            if (!sf.empty() && sf.front() == '/')
                sf.erase(0, 1);
            size_t pos = sf.find_last_of('/');
            std::string parentPath = pos == std::string::npos ? std::string("/") : std::string("/" + sf.substr(0, pos + 1));

            // Create target parent
            std::string tf(to);
            if (!tf.empty() && tf.front() == '/')
                tf.erase(0, 1);
            size_t tpos = tf.find_last_of('/');
            std::string targetParentPath = tpos == std::string::npos ? std::string("/") : std::string("/" + tf.substr(0, tpos + 1));

            auto sourceParent = findNode(parentPath);
            if (!sourceParent)
            {
                return false;
            }

            std::string srcName = sf.substr(pos == std::string::npos ? 0 : pos + 1);
            auto it = sourceParent->children.find(srcName);
            if (it == sourceParent->children.end())
            {
                return false;
            }

            auto targetParent = createPath(targetParentPath, true);
            std::string destName = tf.substr(tpos == std::string::npos ? 0 : tpos + 1);

            // move node
            auto nodePtr = it->second;
            sourceParent->children.erase(it);
            nodePtr->name = destName;
            // compute new path for node and its children
            std::function<void(const std::shared_ptr<Node> &, const std::string &)> updatePaths;
            updatePaths = [&](const std::shared_ptr<Node> &n, const std::string &parentPath)
            {
                n->path = parentPath + (parentPath.back() == '/' ? std::string() : std::string("/")) + n->name;
                for (auto &c : n->children)
                {
                    updatePaths(c.second, n->path + "/");
                }
            };

            targetParent->children[destName] = nodePtr;
            updatePaths(nodePtr, targetParent->path + (targetParent->path.back() == '/' ? std::string() : std::string("/")));
            targetParent->lastWrite = static_cast<uint32_t>(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
            return true;
        }

        bool list(std::string_view directoryPath,
                  const DirectoryEntryCallback &callback,
                  bool recursive = false) override
        {
            auto node = findNode(directoryPath);
            if (!node)
            {
                return false;
            }

            if (!node->isDirectory)
            {
                DirectoryEntry entry{node->name, node->path, false};
                return callback(entry);
            }

            std::function<bool(const std::shared_ptr<Node> &)> walk;
            walk = [&](const std::shared_ptr<Node> &n) -> bool
            {
                for (const auto &c : n->children)
                {
                    DirectoryEntry entry{c.second->name, c.second->path, c.second->isDirectory};
                    if (!callback(entry))
                    {
                        return false;
                    }
                    if (recursive && c.second->isDirectory)
                    {
                        if (!walk(c.second))
                        {
                            return false;
                        }
                    }
                }

                return true;
            };

            return walk(node);
        }

    private:
        struct Node
        {
            std::string name;
            std::string path;
            bool isDirectory = false;
            std::vector<uint8_t> data;
            std::map<std::string, std::shared_ptr<Node>> children;
            uint32_t lastWrite = 0;
        };

        std::shared_ptr<Node> root() const
        {
            return root_;
        }

        std::shared_ptr<Node> findNode(std::string_view path) const
        {
            if (path.empty() || path == "/")
            {
                return root_;
            }

            std::string p(path);
            if (!p.empty() && p.front() == '/')
            {
                p.erase(0, 1);
            }

            std::shared_ptr<Node> current = root_;
            size_t pos = 0;
            while (pos < p.size())
            {
                auto next = p.find('/', pos);
                std::string part;
                if (next == std::string::npos)
                {
                    part = p.substr(pos);
                    pos = p.size();
                }
                else
                {
                    part = p.substr(pos, next - pos);
                    pos = next + 1;
                }

                auto it = current->children.find(part);
                if (it == current->children.end())
                {
                    return nullptr;
                }

                current = it->second;
            }

            return current;
        }

        std::shared_ptr<Node> createPath(std::string_view path, bool asDirectory)
        {
            std::string p(path);
            if (!p.empty() && p.front() == '/')
            {
                p.erase(0, 1);
            }

            if (p.empty())
            {
                return root_;
            }

            std::shared_ptr<Node> current = root_;
            size_t pos = 0;
            while (pos < p.size())
            {
                auto next = p.find('/', pos);
                std::string part;
                bool last = false;
                if (next == std::string::npos)
                {
                    part = p.substr(pos);
                    last = true;
                    pos = p.size();
                }
                else
                {
                    part = p.substr(pos, next - pos);
                    pos = next + 1;
                }

                auto it = current->children.find(part);
                if (it == current->children.end())
                {
                    auto node = std::make_shared<Node>();
                    node->name = part;
                    node->isDirectory = !last ? true : asDirectory;
                    // set path
                    if (current->path == "/")
                    {
                        node->path = std::string("/") + node->name;
                    }
                    else
                    {
                        node->path = current->path + "/" + node->name;
                    }
                    node->lastWrite = static_cast<uint32_t>(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
                    current->children[part] = node;
                    current = node;
                }
                else
                {
                    current = it->second;
                    if (last && asDirectory)
                    {
                        current->isDirectory = true;
                    }
                }
            }

            return current;
        }

        std::shared_ptr<Node> root_;
        friend class MemoryFile;
    };

    // MemoryFile implementation

    class MemoryFile : public IFile
    {
    public:
        MemoryFile(std::shared_ptr<MemoryFileSystem::Node> node, std::string name, std::string path, FileOpenMode mode)
            : node_(std::move(node)), nameStorage_(std::move(name)), pathStorage_(std::move(path)), mode_(mode)
        {
        }

        bool isDirectory() const override
        {
            return node_->isDirectory;
        }

        void close() override
        {
            closed_ = true;
        }

        std::optional<std::size_t> size() const override
        {
            return node_->data.size();
        }

        std::string_view name() const override
        {
            return nameStorage_;
        }

        std::string_view path() const override
        {
            return pathStorage_;
        }

        std::optional<uint32_t> lastWriteEpochSeconds() const override
        {
            return node_->lastWrite;
        }

        AvailableResult available() override
        {
            if (position_ >= node_->data.size())
            {
                return ExhaustedResult();
            }

            return AvailableBytes(node_->data.size() - position_);
        }

        size_t read(httpadv::v1::util::span<uint8_t> buffer) override
        {

            if (position_ >= node_->data.size())
            {
                return 0;
            }

            const size_t toCopy = std::min(buffer.size(), node_->data.size() - position_);
            std::copy_n(node_->data.data() + position_, toCopy, buffer.data());
            position_ += toCopy;
            return toCopy;
        }

        size_t peek(httpadv::v1::util::span<uint8_t> buffer) override
        {

            if (position_ >= node_->data.size())
            {
                return 0;
            }

            const size_t toCopy = std::min(buffer.size(), node_->data.size() - position_);
            std::copy_n(node_->data.data() + position_, toCopy, buffer.data());
            return toCopy;
        }

        std::size_t write(httpadv::v1::util::span<const uint8_t> buffer) override
        {

            if (mode_ == FileOpenMode::Read)
            {
                return 0;
            }

            const size_t need = position_ + buffer.size();
            if (node_->data.size() < need)
            {
                node_->data.resize(need);
            }

            std::copy_n(buffer.data(), buffer.size(), node_->data.data() + position_);
            position_ += buffer.size();
            node_->lastWrite = static_cast<uint32_t>(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
            return buffer.size();
        }

        void flush() override
        {
            // no-op for memory
        }

    private:
        std::shared_ptr<MemoryFileSystem::Node> node_;
        std::string nameStorage_;
        std::string pathStorage_;
        size_t position_ = 0;
        FileOpenMode mode_;
        bool closed_ = false;
    };

} // namespace httpadv::v1::platform::memory

namespace httpadv::v1::platform::memory
{
    inline FileHandle MemoryFileSystem::open(std::string_view path, FileOpenMode mode)
    {
        auto node = findNode(path);
        if (!node)
        {
            // create file if opening for write
            if (mode == FileOpenMode::Read)
            {
                return nullptr;
            }

            node = createPath(path, false);
        }

        // If node exists and is directory, cannot open as file
        if (node->isDirectory)
        {
            return nullptr;
        }

        return FileHandle(new MemoryFile(node, std::string(node->name), std::string(path), mode));
    }

} // namespace httpadv::v1::platform::memory
