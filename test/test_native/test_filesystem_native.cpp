#include <unity.h>

#if defined(_WIN32)
#include <lumalink/platform/windows/WindowsFileAdapter.h>
#else
#include <lumalink/platform/posix/PosixFileAdapter.h>
#endif

#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <memory>
#include <span>
#include <string>
#include <vector>

#if defined(_WIN32)
using NativeFSImpl = lumalink::platform::windows::WindowsFs;
#else
using NativeFSImpl = lumalink::platform::posix::PosixFS;
#endif

using lumalink::platform::filesystem::FileHandle;
using lumalink::platform::filesystem::FileOpenMode;
using lumalink::platform::filesystem::IFileSystem;
using lumalink::platform::filesystem::DirectoryEntry;

static unsigned g_tempCounter = 0;

static std::string MakeTempPath(const char *label)
{
    return std::string("test_temp_") + label + "_" +
           std::to_string(static_cast<unsigned long long>(std::time(nullptr))) + "_" +
           std::to_string(g_tempCounter++) + ".tmp";
}

static bool CreateBinaryFile(const std::string &hostPath, const char *content)
{
    std::ofstream fh(hostPath, std::ios::binary | std::ios::trunc);
    if (!fh.is_open())
    {
        return false;
    }
    fh.write(content, static_cast<std::streamsize>(std::strlen(content)));
    return fh.good();
}

static int ReadByte(lumalink::platform::filesystem::IFile &file)
{
    uint8_t byte = 0;
    return file.read(std::span<uint8_t>(&byte, 1)) == 1 ? static_cast<int>(byte) : -1;
}

void test_native_filesystem_normalize_path()
{
    std::unique_ptr<IFileSystem> fs = std::make_unique<NativeFSImpl>();
    TEST_ASSERT_NOT_NULL(fs.get());

#if defined(_WIN32)
    TEST_ASSERT_EQUAL_STRING("./dir/file.txt", fs->normalizePath(".\\dir\\file.txt").c_str());
#else
    TEST_ASSERT_EQUAL_STRING(".\\dir\\file.txt", fs->normalizePath(".\\dir\\file.txt").c_str());
#endif

    TEST_ASSERT_EQUAL_STRING("./dir/file.txt", fs->normalizePath("./dir/file.txt").c_str());
}

void test_native_file_factory_can_open_and_read_temp_file()
{
    const std::string tmpPath = MakeTempPath("file_read");
    const char *content = "hello-native";
    TEST_ASSERT_TRUE(CreateBinaryFile(tmpPath, content));

    std::unique_ptr<IFileSystem> fs = std::make_unique<NativeFSImpl>();
    TEST_ASSERT_NOT_NULL(fs.get());

    FileHandle file = fs->open(tmpPath, FileOpenMode::Read);
    TEST_ASSERT_NOT_NULL(file.get());
    TEST_ASSERT_TRUE(file->size().has_value());
    TEST_ASSERT_EQUAL_UINT64(std::strlen(content), *file->size());
    TEST_ASSERT_EQUAL_INT('h', ReadByte(*file));

    file->close();
    fs->remove(tmpPath);
}

void test_native_file_factory_supports_directory_handles()
{
    std::unique_ptr<IFileSystem> fs = std::make_unique<NativeFSImpl>();
    TEST_ASSERT_NOT_NULL(fs.get());

    FileHandle file = fs->open(".", FileOpenMode::Read);
    TEST_ASSERT_NOT_NULL(file.get());
    TEST_ASSERT_TRUE(file->isDirectory());
    TEST_ASSERT_FALSE(file->size().has_value());
    TEST_ASSERT_EQUAL_UINT64(0, file->read(std::span<uint8_t>()));
}

void test_native_file_factory_can_write_and_reopen_file()
{
    const std::string tmpPath = MakeTempPath("file_write");

    std::unique_ptr<IFileSystem> fs = std::make_unique<NativeFSImpl>();
    TEST_ASSERT_NOT_NULL(fs.get());

    FileHandle writable = fs->open(tmpPath, FileOpenMode::ReadWrite);
    TEST_ASSERT_NOT_NULL(writable.get());

    const std::vector<uint8_t> payload = {'o', 'k'};
    TEST_ASSERT_EQUAL_UINT64(payload.size(),
        writable->write(std::span<const uint8_t>(payload.data(), payload.size())));
    writable->flush();
    writable->close();

    FileHandle readable = fs->open(tmpPath, FileOpenMode::Read);
    TEST_ASSERT_NOT_NULL(readable.get());
    TEST_ASSERT_TRUE(readable->size().has_value());
    TEST_ASSERT_EQUAL_UINT64(payload.size(), *readable->size());
    TEST_ASSERT_EQUAL_INT('o', ReadByte(*readable));
    TEST_ASSERT_EQUAL_INT('k', ReadByte(*readable));
    readable->close();

    fs->remove(tmpPath);
}

#if defined(_WIN32)
void test_windows_forward_slash_in_open_and_exists()
{
    const std::string tmpPath = MakeTempPath("fs_slash");
    const char *content = "win-slash";
    TEST_ASSERT_TRUE(CreateBinaryFile(tmpPath, content));

    std::unique_ptr<IFileSystem> fs = std::make_unique<NativeFSImpl>();
    TEST_ASSERT_NOT_NULL(fs.get());

    const std::string pathWithDot = std::string("./") + tmpPath; // forward-slash path
    FileHandle file = fs->open(pathWithDot, FileOpenMode::Read);
    TEST_ASSERT_NOT_NULL(file.get());
    TEST_ASSERT_TRUE(fs->exists(pathWithDot));
    TEST_ASSERT_EQUAL_INT('w', ReadByte(*file));
    file->close();

    TEST_ASSERT_TRUE(fs->remove(pathWithDot));
}

void test_windows_list_returns_forward_slash_paths()
{
    std::unique_ptr<IFileSystem> fs = std::make_unique<NativeFSImpl>();
    TEST_ASSERT_NOT_NULL(fs.get());

    TEST_ASSERT_TRUE(fs->mkdir("dir_ws"));
    FileHandle f = fs->open("dir_ws/asset.txt", FileOpenMode::ReadWrite);
    TEST_ASSERT_NOT_NULL(f.get());
    const std::vector<uint8_t> payload = {'a'};
    TEST_ASSERT_EQUAL_UINT64(1,
        f->write(std::span<const uint8_t>(payload.data(), payload.size())));
    f->close();

    std::vector<DirectoryEntry> entries;
    // verify directory exists before listing
    const bool dirExists = fs->exists("dir_ws");
    printf("DEBUG: fs->exists(\"dir_ws\") = %d\n", dirExists ? 1 : 0);
    TEST_ASSERT_TRUE(dirExists);
    // list the created directory to reliably observe its contents
    const bool listResult = fs->list("/dir_ws", [&entries](const DirectoryEntry &entry) {
        entries.push_back(entry);
        return true;
    }, false);
    printf("DEBUG: fs->list('/dir_ws') = %d, entries=%llu\n", listResult ? 1 : 0, static_cast<unsigned long long>(entries.size()));
    TEST_ASSERT_TRUE(listResult);

    bool foundFile = false;
    for (const auto &e : entries)
    {
        if (!e.isDirectory && e.path == "/dir_ws/asset.txt")
        {
            foundFile = true;
        }
    }

    TEST_ASSERT_TRUE(foundFile);

    TEST_ASSERT_TRUE(fs->remove("dir_ws/asset.txt"));
    TEST_ASSERT_TRUE(fs->remove("dir_ws"));
}
#endif


#if defined(_WIN32)
void test_windows_backslash_paths_supported()
{
    const std::string tmpPath = MakeTempPath("fs_backslash");
    const char *content = "win-back";
    TEST_ASSERT_TRUE(CreateBinaryFile(tmpPath, content));

    std::unique_ptr<IFileSystem> fs = std::make_unique<NativeFSImpl>();
    TEST_ASSERT_NOT_NULL(fs.get());

    const std::string pathWithBack = std::string(".\\") + tmpPath; // backslash path
    FileHandle file = fs->open(pathWithBack, FileOpenMode::Read);
    TEST_ASSERT_NOT_NULL(file.get());
    TEST_ASSERT_TRUE(fs->exists(pathWithBack));
    TEST_ASSERT_EQUAL_INT('w', ReadByte(*file));
    file->close();

    TEST_ASSERT_TRUE(fs->remove(pathWithBack));
}
#endif
