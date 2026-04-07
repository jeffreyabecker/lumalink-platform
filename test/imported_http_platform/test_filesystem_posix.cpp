#include "../support/include/ConsolidatedNativeSuite.h"

#include <unity.h>

#if defined(_WIN32)
#include "../../src/httpadv/v1/platform/windows/WindowsFileAdapter.h"
#else
#include "../../src/httpadv/v1/platform/posix/PosixFileAdapter.h"
#endif

#if defined(_WIN32)
using NativeFSImpl = httpadv::v1::platform::windows::WindowsFs;
#else
using NativeFSImpl = httpadv::v1::platform::posix::PosixFS;
#endif
#include "../../src/httpadv/v1/core/HttpContentTypes.h"
#include "../../src/httpadv/v1/staticfiles/DefaultFileLocator.h"

#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

using namespace httpadv::v1::core;
using namespace httpadv::v1::staticfiles;
using namespace httpadv::v1::transport;
using namespace httpadv::v1::util;

namespace
{
    std::string MakeTempPath(const char *label)
    {
        static unsigned counter = 0;
        return std::string("test_temp_") + label + "_" +
               std::to_string(static_cast<unsigned long long>(std::time(nullptr))) + "_" +
               std::to_string(counter++) + ".tmp";
    }

    bool CreateBinaryFile(std::string_view hostPath, std::string_view content)
    {
        std::ofstream fileHandle(std::string(hostPath), std::ios::binary | std::ios::trunc);
        if (!fileHandle.is_open())
        {
            return false;
        }

        fileHandle.write(content.data(), static_cast<std::streamsize>(content.size()));
        return fileHandle.good();
    }

    void localSetUp()
    {
    }

    void localTearDown()
    {
    }

    void test_native_file_factory_can_open_and_read_temp_file()
    {
        const std::string tmpPath = MakeTempPath("file_read");
        const char *content = "hello-posix";
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
        TEST_ASSERT_EQUAL_UINT64(0, file->read(httpadv::v1::util::span<uint8_t>()));
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
                     writable->write(httpadv::v1::util::span<const uint8_t>(payload.data(), payload.size())));
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

    void test_content_types_accept_string_view_inputs()
    {
        HttpContentTypes contentTypes;

        TEST_ASSERT_EQUAL_STRING("text/html", contentTypes.getContentTypeByExtension(std::string_view("HTML")));
        TEST_ASSERT_EQUAL_STRING("application/javascript", contentTypes.getContentTypeFromPath(std::string_view("/www/app.Js")));

        contentTypes.set(std::string_view("cust"), "application/x-custom");
        TEST_ASSERT_EQUAL_STRING("application/x-custom", contentTypes.getContentTypeByExtension(std::string_view("CUST")));
    }

    void test_default_file_locator_accepts_string_view_configuration()
    {
        std::unique_ptr<IFileSystem> fs = std::make_unique<NativeFSImpl>();
        TEST_ASSERT_NOT_NULL(fs.get());
        DefaultFileLocator locator(fs.get());

        locator.setRequestPathPrefixes(std::string_view("/static"), std::string_view("/static/api"));

        TEST_ASSERT_TRUE(locator.canHandle(std::string_view("/static/app.js")));
        TEST_ASSERT_FALSE(locator.canHandle(std::string_view("/static/api/data.json")));

        locator.setRequestPathPrefixes(std::string_view("/assets"));
        TEST_ASSERT_TRUE(locator.canHandle(std::string_view("/assets/logo.svg")));
        TEST_ASSERT_FALSE(locator.canHandle(std::string_view("/static/app.js")));
    }

    int runUnitySuite()
    {
        UNITY_BEGIN();
        RUN_TEST(test_native_file_factory_can_open_and_read_temp_file);
        RUN_TEST(test_native_file_factory_supports_directory_handles);
        RUN_TEST(test_native_file_factory_can_write_and_reopen_file);
        RUN_TEST(test_content_types_accept_string_view_inputs);
        RUN_TEST(test_default_file_locator_accepts_string_view_configuration);
        return UNITY_END();
    }
}

int run_test_filesystem_posix()
{
    return httpadv::v1::TestSupport::RunConsolidatedSuite(
        "filesystem native",
        runUnitySuite,
        localSetUp,
        localTearDown);
}
