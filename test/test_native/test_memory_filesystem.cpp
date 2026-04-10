#include <unity.h>

#include <lumalink/filesystem/MemoryFileAdapter.h>

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

using lumalink::platform::filesystem::DirectoryEntry;
using lumalink::platform::filesystem::FileHandle;
using lumalink::platform::filesystem::FileOpenMode;
using lumalink::platform::filesystem::IFileSystem;
using lumalink::platform::memory::MemoryFileSystem;

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
