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
using lumalink::platform::util::span;

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
    return file.read(span<uint8_t>(&byte, 1)) == 1 ? static_cast<int>(byte) : -1;
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
    TEST_ASSERT_EQUAL_UINT64(0, file->read(span<uint8_t>()));
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
        writable->write(span<const uint8_t>(payload.data(), payload.size())));
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
