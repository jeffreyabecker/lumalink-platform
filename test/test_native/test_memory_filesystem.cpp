#include <unity.h>

#include <lumalink/platform/memory/MemoryFileAdapter.h>
#include <lumalink/platform/filesystem/ScopedFileSystem.h>

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

using lumalink::platform::filesystem::DirectoryEntry;
using lumalink::platform::filesystem::FileHandle;
using lumalink::platform::filesystem::FileOpenMode;
using lumalink::platform::filesystem::IFileSystem;
using lumalink::platform::filesystem::ScopedFileSystem;
using lumalink::platform::memory::MemoryFileSystem;

namespace
{
    class NormalizingStubFileSystem final : public IFileSystem
    {
    public:
        std::string normalizePath(std::string_view path) const override
        {
            std::string normalized(path);
            for (char &ch : normalized)
            {
                if (ch == '\\')
                {
                    ch = '/';
                }
            }
            return normalized;
        }

        bool exists(std::string_view) const override
        {
            return false;
        }

        bool mkdir(std::string_view) override
        {
            return false;
        }

        FileHandle open(std::string_view, FileOpenMode) override
        {
            return nullptr;
        }

        bool remove(std::string_view) override
        {
            return false;
        }

        bool rename(std::string_view, std::string_view) override
        {
            return false;
        }

        bool list(std::string_view, const lumalink::platform::filesystem::DirectoryEntryCallback &, bool) override
        {
            return false;
        }

        std::unique_ptr<IFileSystem> getScoped(std::string_view) override
        {
            return nullptr;
        }
    };
}

static int ReadByte(lumalink::platform::filesystem::IFile &file)
{
    uint8_t byte = 0;
    return file.read(std::span<uint8_t>(&byte, 1)) == 1 ? static_cast<int>(byte) : -1;
}

void test_memory_file_can_write_and_read()
{
    std::unique_ptr<IFileSystem> fs = std::make_unique<MemoryFileSystem>();
    TEST_ASSERT_NOT_NULL(fs.get());

    const std::string path = "/test_memory.txt";
    FileHandle writable = fs->open(path, FileOpenMode::ReadWrite);
    TEST_ASSERT_NOT_NULL(writable.get());

    const std::vector<uint8_t> payload = {'h', 'i', '!'};
    TEST_ASSERT_EQUAL_UINT64(payload.size(),
        writable->write(std::span<const uint8_t>(payload.data(), payload.size())));
    writable->flush();
    writable->close();

    FileHandle readable = fs->open(path, FileOpenMode::Read);
    TEST_ASSERT_NOT_NULL(readable.get());
    TEST_ASSERT_TRUE(readable->size().has_value());
    TEST_ASSERT_EQUAL_UINT64(payload.size(), *readable->size());
    TEST_ASSERT_EQUAL_INT('h', ReadByte(*readable));
    TEST_ASSERT_EQUAL_INT('i', ReadByte(*readable));
    TEST_ASSERT_EQUAL_INT('!', ReadByte(*readable));
    readable->close();
}

void test_memory_directory_and_list()
{
    std::unique_ptr<IFileSystem> fs = std::make_unique<MemoryFileSystem>();
    TEST_ASSERT_NOT_NULL(fs.get());

    TEST_ASSERT_TRUE(fs->mkdir("/dir"));
    FileHandle f = fs->open("/dir/asset.txt", FileOpenMode::ReadWrite);
    TEST_ASSERT_NOT_NULL(f.get());
    const std::vector<uint8_t> payload = {'a'};
    TEST_ASSERT_EQUAL_UINT64(1,
        f->write(std::span<const uint8_t>(payload.data(), payload.size())));
    f->close();

    std::vector<DirectoryEntry> entries;
    TEST_ASSERT_TRUE(fs->list("/", [&entries](const DirectoryEntry &entry) {
        entries.push_back(entry);
        return true;
    }, true));

    bool foundDir = false;
    bool foundFile = false;
    for (const auto &e : entries)
    {
        if (e.isDirectory && e.path == "/dir")
        {
            foundDir = true;
        }
        if (!e.isDirectory && e.path == "/dir/asset.txt")
        {
            foundFile = true;
        }
    }

    TEST_ASSERT_TRUE(foundDir);
    TEST_ASSERT_TRUE(foundFile);

    TEST_ASSERT_TRUE(fs->remove("/dir/asset.txt"));
    TEST_ASSERT_TRUE(fs->remove("/dir"));
}

void test_memory_rename()
{
    std::unique_ptr<IFileSystem> fs = std::make_unique<MemoryFileSystem>();
    TEST_ASSERT_NOT_NULL(fs.get());

    FileHandle f = fs->open("/a.txt", FileOpenMode::ReadWrite);
    TEST_ASSERT_NOT_NULL(f.get());
    const std::vector<uint8_t> payload = {'x'};
    TEST_ASSERT_EQUAL_UINT64(1,
        f->write(std::span<const uint8_t>(payload.data(), payload.size())));
    f->close();

    TEST_ASSERT_TRUE(fs->exists("/a.txt"));
    TEST_ASSERT_TRUE(fs->rename("/a.txt", "/b.txt"));
    TEST_ASSERT_FALSE(fs->exists("/a.txt"));
    TEST_ASSERT_TRUE(fs->exists("/b.txt"));
}

void test_memory_filesystem_normalize_path_is_noop()
{
    std::unique_ptr<IFileSystem> fs = std::make_unique<MemoryFileSystem>();
    TEST_ASSERT_NOT_NULL(fs.get());

    TEST_ASSERT_EQUAL_STRING("\\dir\\file.txt", fs->normalizePath("\\dir\\file.txt").c_str());
    TEST_ASSERT_EQUAL_STRING("/dir/file.txt", fs->normalizePath("/dir/file.txt").c_str());
}

void test_memory_filesystem_ensure_directory_creates_nested_paths()
{
    std::unique_ptr<IFileSystem> fs = std::make_unique<MemoryFileSystem>();
    TEST_ASSERT_NOT_NULL(fs.get());

    TEST_ASSERT_TRUE(fs->ensureDirectory("/root/dir/sub"));
    TEST_ASSERT_TRUE(fs->exists("/root"));
    TEST_ASSERT_TRUE(fs->exists("/root/dir"));
    TEST_ASSERT_TRUE(fs->exists("/root/dir/sub"));
}

void test_memory_filesystem_ensure_directory_handles_existing_and_root()
{
    std::unique_ptr<IFileSystem> fs = std::make_unique<MemoryFileSystem>();
    TEST_ASSERT_NOT_NULL(fs.get());

    TEST_ASSERT_TRUE(fs->mkdir("/existing"));
    TEST_ASSERT_TRUE(fs->ensureDirectory("/existing"));
    TEST_ASSERT_TRUE(fs->ensureDirectory("/"));
}

void test_memory_filesystem_ensure_directory_rejects_empty_path()
{
    std::unique_ptr<IFileSystem> fs = std::make_unique<MemoryFileSystem>();
    TEST_ASSERT_NOT_NULL(fs.get());

    TEST_ASSERT_FALSE(fs->ensureDirectory(""));
}

void test_scoped_filesystem_scopes_paths_and_exposes_scoped_view()
{
    MemoryFileSystem inner;
    auto *innerPtr = &inner;

    std::unique_ptr<IFileSystem> fs = std::make_unique<ScopedFileSystem>("/root", inner);
    TEST_ASSERT_NOT_NULL(fs.get());

    TEST_ASSERT_TRUE(fs->mkdir("/dir"));

    FileHandle writable = fs->open("/dir/asset.txt", FileOpenMode::ReadWrite);
    TEST_ASSERT_NOT_NULL(writable.get());
    const std::vector<uint8_t> payload = {'s', 'c'};
    TEST_ASSERT_EQUAL_UINT64(payload.size(),
        writable->write(std::span<const uint8_t>(payload.data(), payload.size())));
    writable->close();

    TEST_ASSERT_TRUE(innerPtr->exists("/root/dir/asset.txt"));
    TEST_ASSERT_FALSE(innerPtr->exists("/dir/asset.txt"));

    FileHandle readable = fs->open("/dir/asset.txt", FileOpenMode::Read);
    TEST_ASSERT_NOT_NULL(readable.get());
    TEST_ASSERT_EQUAL_STRING("/dir/asset.txt", std::string(readable->path()).c_str());
    TEST_ASSERT_EQUAL_STRING("asset.txt", std::string(readable->name()).c_str());
    readable->close();

    std::vector<DirectoryEntry> entries;
    TEST_ASSERT_TRUE(fs->list("/", [&entries](const DirectoryEntry &entry) {
        entries.push_back(entry);
        return true;
    }, true));

    bool foundDir = false;
    bool foundFile = false;
    for (const auto &entry : entries)
    {
        if (entry.isDirectory && entry.path == "/dir")
        {
            foundDir = true;
        }

        if (!entry.isDirectory && entry.path == "/dir/asset.txt")
        {
            foundFile = true;
        }
    }

    TEST_ASSERT_TRUE(foundDir);
    TEST_ASSERT_TRUE(foundFile);

    TEST_ASSERT_TRUE(fs->rename("/dir/asset.txt", "/dir/renamed.txt"));
    TEST_ASSERT_TRUE(innerPtr->exists("/root/dir/renamed.txt"));
    TEST_ASSERT_FALSE(innerPtr->exists("/root/dir/asset.txt"));
}

void test_scoped_filesystem_root_scope_passthroughs_paths()
{
    MemoryFileSystem inner;
    auto *innerPtr = &inner;

    std::unique_ptr<IFileSystem> fs = std::make_unique<ScopedFileSystem>("/", inner);
    TEST_ASSERT_NOT_NULL(fs.get());

    FileHandle writable = fs->open("/top.txt", FileOpenMode::ReadWrite);
    TEST_ASSERT_NOT_NULL(writable.get());
    const std::vector<uint8_t> payload = {'r'};
    TEST_ASSERT_EQUAL_UINT64(payload.size(),
        writable->write(std::span<const uint8_t>(payload.data(), payload.size())));
    writable->close();

    TEST_ASSERT_TRUE(innerPtr->exists("/top.txt"));

    FileHandle readable = fs->open("/top.txt", FileOpenMode::Read);
    TEST_ASSERT_NOT_NULL(readable.get());
    TEST_ASSERT_EQUAL_STRING("/top.txt", std::string(readable->path()).c_str());
    TEST_ASSERT_EQUAL_STRING("top.txt", std::string(readable->name()).c_str());
    readable->close();
}

void test_scoped_filesystem_exposes_scoped_directory_handles_and_entries()
{
    MemoryFileSystem inner;
    auto *innerPtr = &inner;

    TEST_ASSERT_TRUE(innerPtr->mkdir("/root"));
    TEST_ASSERT_TRUE(innerPtr->mkdir("/root/nested"));
    FileHandle innerFile = innerPtr->open("/root/nested/file.txt", FileOpenMode::ReadWrite);
    TEST_ASSERT_NOT_NULL(innerFile.get());
    innerFile->close();

    std::unique_ptr<IFileSystem> fs = std::make_unique<ScopedFileSystem>("/root", inner);
    TEST_ASSERT_NOT_NULL(fs.get());

    TEST_ASSERT_TRUE(fs->exists("/nested"));
    TEST_ASSERT_TRUE(fs->exists("/nested/file.txt"));

    std::vector<DirectoryEntry> entries;
    TEST_ASSERT_TRUE(fs->list("/nested", [&entries](const DirectoryEntry &entry) {
        entries.push_back(entry);
        return true;
    }, false));

    TEST_ASSERT_EQUAL_UINT64(1, entries.size());
    TEST_ASSERT_EQUAL_STRING("file.txt", entries.front().name.c_str());
    TEST_ASSERT_EQUAL_STRING("/nested/file.txt", entries.front().path.c_str());
    TEST_ASSERT_FALSE(entries.front().isDirectory);
}

void test_scoped_filesystem_normalize_path_delegates_to_inner_filesystem()
{
    NormalizingStubFileSystem inner;
    std::unique_ptr<IFileSystem> fs = std::make_unique<ScopedFileSystem>("/root", inner);
    TEST_ASSERT_NOT_NULL(fs.get());

    TEST_ASSERT_EQUAL_STRING("/dir/file.txt", fs->normalizePath("\\dir\\file.txt").c_str());
    TEST_ASSERT_EQUAL_STRING("dir/file.txt", fs->normalizePath("dir\\file.txt").c_str());
}
