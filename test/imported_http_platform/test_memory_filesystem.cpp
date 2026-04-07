#include "../support/include/ConsolidatedNativeSuite.h"

#include "../../src/httpadv/v1/HttpServerAdvanced.h"

#include <unity.h>

#include "../../src/httpadv/v1/platform/memory/MemoryFileAdapter.h"

using namespace httpadv::v1::core;
using namespace httpadv::v1::handlers;
using namespace httpadv::v1::pipeline;
using namespace httpadv::v1::response;
using namespace httpadv::v1::routing;
using namespace httpadv::v1::server;
using namespace httpadv::v1::staticfiles;
using namespace httpadv::v1::transport;
using namespace httpadv::v1::util;
using namespace httpadv::v1::websocket;

using NativeFSImpl = httpadv::v1::platform::memory::MemoryFileSystem;

namespace
{
    void localSetUp()
    {
    }

    void localTearDown()
    {
    }

    void test_memory_file_can_write_and_read()
    {
        std::unique_ptr<IFileSystem> fs = std::make_unique<NativeFSImpl>();
        TEST_ASSERT_NOT_NULL(fs.get());

        const std::string path = "/test_memory.txt";
        FileHandle writable = fs->open(path, FileOpenMode::ReadWrite);
        TEST_ASSERT_NOT_NULL(writable.get());

        const std::vector<uint8_t> payload = {'h','i','!'};
        TEST_ASSERT_EQUAL_UINT64(payload.size(), writable->write(httpadv::v1::util::span<const uint8_t>(payload.data(), payload.size())));
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
        std::unique_ptr<IFileSystem> fs = std::make_unique<NativeFSImpl>();
        TEST_ASSERT_NOT_NULL(fs.get());

        // create directory and file
        TEST_ASSERT_TRUE(fs->mkdir("/dir"));
        FileHandle f = fs->open("/dir/asset.txt", FileOpenMode::ReadWrite);
        TEST_ASSERT_NOT_NULL(f.get());
        const std::vector<uint8_t> payload = {'a'};
        TEST_ASSERT_EQUAL_UINT64(1, f->write(httpadv::v1::util::span<const uint8_t>(payload.data(), payload.size())));
        f->close();

        std::vector<DirectoryEntry> entries;
        TEST_ASSERT_TRUE(fs->list("/", [&entries](const DirectoryEntry &entry)
        {
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

        // remove file and directory
        TEST_ASSERT_TRUE(fs->remove("/dir/asset.txt"));
        TEST_ASSERT_TRUE(fs->remove("/dir"));
    }

    void test_memory_rename()
    {
        std::unique_ptr<IFileSystem> fs = std::make_unique<NativeFSImpl>();
        TEST_ASSERT_NOT_NULL(fs.get());

        FileHandle f = fs->open("/a.txt", FileOpenMode::ReadWrite);
        TEST_ASSERT_NOT_NULL(f.get());
        const std::vector<uint8_t> payload = {'x'};
        TEST_ASSERT_EQUAL_UINT64(1, f->write(httpadv::v1::util::span<const uint8_t>(payload.data(), payload.size())));
        f->close();

        TEST_ASSERT_TRUE(fs->exists("/a.txt"));
        TEST_ASSERT_TRUE(fs->rename("/a.txt", "/b.txt"));
        TEST_ASSERT_FALSE(fs->exists("/a.txt"));
        TEST_ASSERT_TRUE(fs->exists("/b.txt"));
    }

    int runUnitySuite()
    {
        UNITY_BEGIN();
        RUN_TEST(test_memory_file_can_write_and_read);
        RUN_TEST(test_memory_directory_and_list);
        RUN_TEST(test_memory_rename);
        return UNITY_END();
    }
}

int run_test_memory_filesystem()
{
    return httpadv::v1::TestSupport::RunConsolidatedSuite(
        "memory filesystem native",
        runUnitySuite,
        localSetUp,
        localTearDown);
}
