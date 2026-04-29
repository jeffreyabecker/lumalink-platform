#include <unity.h>

#if defined(_WIN32)
#include <lumalink/platform/windows/WindowsFileAdapter.h>
#include <windows.h>
#else
#include <lumalink/platform/posix/PosixFileAdapter.h>
#endif

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include <lumalink/platform/filesystem/ScopedFileSystem.h>

#if defined(_WIN32)
using NativeFSImpl = lumalink::platform::windows::WindowsFs;
#else
using NativeFSImpl = lumalink::platform::posix::PosixFS;
#endif

using lumalink::platform::filesystem::FileHandle;
using lumalink::platform::filesystem::FileOpenMode;
using lumalink::platform::filesystem::IFileSystem;
using lumalink::platform::filesystem::DirectoryEntry;
using lumalink::platform::filesystem::ScopedFileSystem;

static unsigned g_tempCounter = 0;

static std::filesystem::path MakeTempDirectoryPath(const char *label)
{
    const std::filesystem::path base = std::filesystem::temp_directory_path();
    const std::string dirName = std::string("lumalink_platform_") + label + "_" +
                               std::to_string(static_cast<unsigned long long>(std::time(nullptr))) + "_" +
                               std::to_string(g_tempCounter++);
    return base / dirName;
}

struct ScopedTempFs
{
    explicit ScopedTempFs(const char *label)
        : tempDir(MakeTempDirectoryPath(label))
    {
        std::filesystem::create_directories(tempDir);
        inner = std::make_unique<NativeFSImpl>();
        root = inner->normalizePath(tempDir.string());
        fs = std::make_unique<ScopedFileSystem>(root, *inner);
    }

    ~ScopedTempFs()
    {
        std::error_code ec;
        std::filesystem::remove_all(tempDir, ec);
    }

    std::filesystem::path tempDir;
    std::string root;
    std::unique_ptr<NativeFSImpl> inner;
    std::unique_ptr<IFileSystem> fs;
};

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
    const DWORD cwdSize = GetCurrentDirectoryA(0, nullptr);
    TEST_ASSERT_TRUE(cwdSize > 0);
    std::string cwd(cwdSize - 1, '\0');
    GetCurrentDirectoryA(cwdSize, cwd.data());
    for (char &ch : cwd)
    {
        if (ch == '\\')
        {
            ch = '/';
        }
    }

    TEST_ASSERT_EQUAL_STRING((cwd + "/dir/file.txt").c_str(), fs->normalizePath(".\\dir\\file.txt").c_str());
    TEST_ASSERT_EQUAL_STRING((cwd + "/dir/file.txt").c_str(), fs->normalizePath("./dir/file.txt").c_str());
#else
    TEST_ASSERT_EQUAL_STRING(".\\dir\\file.txt", fs->normalizePath(".\\dir\\file.txt").c_str());
    TEST_ASSERT_EQUAL_STRING("./dir/file.txt", fs->normalizePath("./dir/file.txt").c_str());
#endif
}

#if !defined(_WIN32)
void test_posix_normalize_path_expands_home()
{
    std::unique_ptr<IFileSystem> fs = std::make_unique<NativeFSImpl>();
    TEST_ASSERT_NOT_NULL(fs.get());

    const char *originalHome = std::getenv("HOME");
    TEST_ASSERT_TRUE(setenv("HOME", "/tmp/lumalink_home", 1) == 0);

    TEST_ASSERT_EQUAL_STRING("/tmp/lumalink_home",
        fs->normalizePath("~").c_str());
    TEST_ASSERT_EQUAL_STRING("/tmp/lumalink_home/config.json",
        fs->normalizePath("~/config.json").c_str());

    if (originalHome != nullptr)
    {
        setenv("HOME", originalHome, 1);
    }
    else
    {
        unsetenv("HOME");
    }
}
#endif

#if defined(_WIN32)
namespace
{
    struct EnvVarGuard
    {
        explicit EnvVarGuard(const char *name)
            : name_(name), hadValue_(false)
        {
            const DWORD required = GetEnvironmentVariableA(name_, nullptr, 0);
            if (required > 0)
            {
                std::string value(required - 1, '\0');
                GetEnvironmentVariableA(name_, value.data(), required);
                original_ = std::move(value);
                hadValue_ = true;
            }
        }

        ~EnvVarGuard()
        {
            if (hadValue_)
            {
                SetEnvironmentVariableA(name_, original_.c_str());
            }
            else
            {
                SetEnvironmentVariableA(name_, nullptr);
            }
        }

        const char *name_;
        std::string original_;
        bool hadValue_;
    };
}

void test_windows_normalize_path_expands_known_env_vars()
{
    std::unique_ptr<IFileSystem> fs = std::make_unique<NativeFSImpl>();
    TEST_ASSERT_NOT_NULL(fs.get());

    EnvVarGuard appData("APPDATA");
    EnvVarGuard localAppData("LOCALAPPDATA");

    TEST_ASSERT_TRUE(SetEnvironmentVariableA("APPDATA", "C:\\Users\\Test\\AppData\\Roaming"));
    TEST_ASSERT_TRUE(SetEnvironmentVariableA("LOCALAPPDATA", "C:\\Users\\Test\\AppData\\Local"));

    TEST_ASSERT_EQUAL_STRING("C:/Users/Test/AppData/Roaming/config.json",
        fs->normalizePath("%APPDATA%/config.json").c_str());
    TEST_ASSERT_EQUAL_STRING("C:/Users/Test/AppData/Local/cache",
        fs->normalizePath("%LOCALAPPDATA%\\cache").c_str());
}

void test_windows_normalize_path_resolves_dot_to_cwd()
{
    std::unique_ptr<IFileSystem> fs = std::make_unique<NativeFSImpl>();
    TEST_ASSERT_NOT_NULL(fs.get());

    const DWORD cwdSize = GetCurrentDirectoryA(0, nullptr);
    TEST_ASSERT_TRUE(cwdSize > 0);
    std::string cwd(cwdSize - 1, '\0');
    GetCurrentDirectoryA(cwdSize, cwd.data());
    std::string cwdExposed(cwd);
    for (char &ch : cwdExposed)
    {
        if (ch == '\\')
        {
            ch = '/';
        }
    }

    TEST_ASSERT_EQUAL_STRING(cwdExposed.c_str(), fs->normalizePath(".").c_str());
    TEST_ASSERT_EQUAL_STRING((cwdExposed + "/file.txt").c_str(),
        fs->normalizePath("./file.txt").c_str());
    TEST_ASSERT_EQUAL_STRING((cwdExposed + "/nested/file.txt").c_str(),
        fs->normalizePath(".\\nested\\file.txt").c_str());
}
#endif

void test_native_file_factory_can_open_and_read_temp_file()
{
    ScopedTempFs fixture("file_read");
    TEST_ASSERT_NOT_NULL(fixture.fs.get());

    const char *content = "hello-native";
    FileHandle writable = fixture.fs->open("/file_read.tmp", FileOpenMode::ReadWrite);
    TEST_ASSERT_NOT_NULL(writable.get());
    const std::vector<uint8_t> payload(content, content + std::strlen(content));
    TEST_ASSERT_EQUAL_UINT64(payload.size(),
        writable->write(std::span<const uint8_t>(payload.data(), payload.size())));
    writable->close();

    FileHandle file = fixture.fs->open("/file_read.tmp", FileOpenMode::Read);
    TEST_ASSERT_NOT_NULL(file.get());
    TEST_ASSERT_TRUE(file->size().has_value());
    TEST_ASSERT_EQUAL_UINT64(std::strlen(content), *file->size());
    TEST_ASSERT_EQUAL_INT('h', ReadByte(*file));

    file->close();
    TEST_ASSERT_TRUE(fixture.fs->remove("/file_read.tmp"));
}

void test_native_file_factory_supports_directory_handles()
{
    ScopedTempFs fixture("dir_open");
    TEST_ASSERT_NOT_NULL(fixture.fs.get());

    FileHandle file = fixture.fs->open("/", FileOpenMode::Read);
    TEST_ASSERT_NOT_NULL(file.get());
    TEST_ASSERT_TRUE(file->isDirectory());
    TEST_ASSERT_FALSE(file->size().has_value());
    TEST_ASSERT_EQUAL_UINT64(0, file->read(std::span<uint8_t>()));
}

void test_native_file_factory_can_write_and_reopen_file()
{
    ScopedTempFs fixture("file_write");
    TEST_ASSERT_NOT_NULL(fixture.fs.get());

    FileHandle writable = fixture.fs->open("/file_write.tmp", FileOpenMode::ReadWrite);
    TEST_ASSERT_NOT_NULL(writable.get());

    const std::vector<uint8_t> payload = {'o', 'k'};
    TEST_ASSERT_EQUAL_UINT64(payload.size(),
        writable->write(std::span<const uint8_t>(payload.data(), payload.size())));
    writable->flush();
    writable->close();

    FileHandle readable = fixture.fs->open("/file_write.tmp", FileOpenMode::Read);
    TEST_ASSERT_NOT_NULL(readable.get());
    TEST_ASSERT_TRUE(readable->size().has_value());
    TEST_ASSERT_EQUAL_UINT64(payload.size(), *readable->size());
    TEST_ASSERT_EQUAL_INT('o', ReadByte(*readable));
    TEST_ASSERT_EQUAL_INT('k', ReadByte(*readable));
    readable->close();

    TEST_ASSERT_TRUE(fixture.fs->remove("/file_write.tmp"));
}

#if defined(_WIN32)
void test_windows_forward_slash_in_open_and_exists()
{
    ScopedTempFs fixture("fs_slash");
    TEST_ASSERT_NOT_NULL(fixture.fs.get());

    FileHandle writable = fixture.fs->open("/slash.txt", FileOpenMode::ReadWrite);
    TEST_ASSERT_NOT_NULL(writable.get());
    const std::vector<uint8_t> payload = {'w'};
    TEST_ASSERT_EQUAL_UINT64(1,
        writable->write(std::span<const uint8_t>(payload.data(), payload.size())));
    writable->close();

    const std::string pathWithDot = std::string("/") + "slash.txt"; // forward-slash path
    FileHandle file = fixture.fs->open(pathWithDot, FileOpenMode::Read);
    TEST_ASSERT_NOT_NULL(file.get());
    TEST_ASSERT_TRUE(fixture.fs->exists(pathWithDot));
    TEST_ASSERT_EQUAL_INT('w', ReadByte(*file));
    file->close();

    TEST_ASSERT_TRUE(fixture.fs->remove(pathWithDot));
}

void test_windows_list_returns_forward_slash_paths()
{
    ScopedTempFs fixture("list_ws");
    TEST_ASSERT_NOT_NULL(fixture.fs.get());

    TEST_ASSERT_TRUE(fixture.fs->mkdir("/dir_ws"));
    FileHandle f = fixture.fs->open("/dir_ws/asset.txt", FileOpenMode::ReadWrite);
    TEST_ASSERT_NOT_NULL(f.get());
    const std::vector<uint8_t> payload = {'a'};
    TEST_ASSERT_EQUAL_UINT64(1,
        f->write(std::span<const uint8_t>(payload.data(), payload.size())));
    f->close();

    std::vector<DirectoryEntry> entries;
    TEST_ASSERT_TRUE(fixture.fs->exists("/dir_ws"));
    TEST_ASSERT_TRUE(fixture.fs->list("/dir_ws", [&entries](const DirectoryEntry &entry) {
        entries.push_back(entry);
        return true;
    }, false));

    bool foundFile = false;
    for (const auto &e : entries)
    {
        if (!e.isDirectory &&
            (e.path == "/dir_ws/asset.txt" || e.path == "dir_ws/asset.txt") &&
            e.path.find('\\') == std::string::npos)
        {
            foundFile = true;
        }
    }

    TEST_ASSERT_TRUE(foundFile);

    TEST_ASSERT_TRUE(fixture.fs->remove("/dir_ws/asset.txt"));
    TEST_ASSERT_TRUE(fixture.fs->remove("/dir_ws"));
}
#endif


#if defined(_WIN32)
void test_windows_backslash_paths_supported()
{
    ScopedTempFs fixture("fs_backslash");
    TEST_ASSERT_NOT_NULL(fixture.fs.get());

    FileHandle writable = fixture.fs->open("/back.txt", FileOpenMode::ReadWrite);
    TEST_ASSERT_NOT_NULL(writable.get());
    const std::vector<uint8_t> payload = {'w'};
    TEST_ASSERT_EQUAL_UINT64(1,
        writable->write(std::span<const uint8_t>(payload.data(), payload.size())));
    writable->close();

    const std::string pathWithBack = std::string("\\back.txt"); // backslash path
    FileHandle file = fixture.fs->open(pathWithBack, FileOpenMode::Read);
    TEST_ASSERT_NOT_NULL(file.get());
    TEST_ASSERT_TRUE(fixture.fs->exists(pathWithBack));
    TEST_ASSERT_EQUAL_INT('w', ReadByte(*file));
    file->close();

    TEST_ASSERT_TRUE(fixture.fs->remove(pathWithBack));
}
#endif
