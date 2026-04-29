#include <unity.h>

#include <lumalink/platform/filesystem/PathUtility.h>

#include <string>

using lumalink::platform::filesystem::PathUtility;

void test_path_utility_get_file_name()
{
    TEST_ASSERT_EQUAL_STRING("file.txt", std::string(PathUtility::getFileName("/root/dir/file.txt")).c_str());
    TEST_ASSERT_EQUAL_STRING("plain.txt", std::string(PathUtility::getFileName("plain.txt")).c_str());
}

void test_path_utility_get_dir_name()
{
    TEST_ASSERT_EQUAL_STRING("/root/dir", std::string(PathUtility::getDirName("/root/dir/file.txt")).c_str());
    TEST_ASSERT_TRUE(PathUtility::getDirName("plain.txt").empty());
}

void test_path_utility_is_root()
{
    TEST_ASSERT_TRUE(PathUtility::isRoot("/"));
    TEST_ASSERT_TRUE(PathUtility::isRoot("."));
    TEST_ASSERT_TRUE(PathUtility::isRoot("C:"));
    TEST_ASSERT_TRUE(PathUtility::isRoot("C:/"));
    TEST_ASSERT_TRUE(PathUtility::isRoot("C:\\"));
    TEST_ASSERT_FALSE(PathUtility::isRoot("/root"));
    TEST_ASSERT_FALSE(PathUtility::isRoot("C:/root"));
}

void test_path_utility_join_relative_segments()
{
    TEST_ASSERT_EQUAL_STRING("dir/file.txt", PathUtility::join("dir", "file.txt").c_str());
    TEST_ASSERT_EQUAL_STRING("dir/file.txt", PathUtility::join("dir/", "/file.txt").c_str());
}

void test_path_utility_join_preserves_absolute_first_segment()
{
    TEST_ASSERT_EQUAL_STRING("/root/file.txt", PathUtility::join("/root", "file.txt").c_str());
    TEST_ASSERT_EQUAL_STRING("/root/file.txt", PathUtility::join("/root", "/file.txt").c_str());
}

void test_path_utility_get_extension()
{
    TEST_ASSERT_EQUAL_STRING("txt", std::string(PathUtility::getExtension("/root/dir/file.txt")).c_str());
    TEST_ASSERT_EQUAL_STRING("gz", std::string(PathUtility::getExtension("archive.tar.gz")).c_str());
    TEST_ASSERT_TRUE(PathUtility::getExtension("/root/dir/file").empty());
    TEST_ASSERT_TRUE(PathUtility::getExtension("/root.dir/file").empty());
}

void test_path_utility_remove_extension()
{
    TEST_ASSERT_EQUAL_STRING("/root/dir/file", std::string(PathUtility::removeExtension("/root/dir/file.txt")).c_str());
    TEST_ASSERT_EQUAL_STRING("archive.tar", std::string(PathUtility::removeExtension("archive.tar.gz")).c_str());
    TEST_ASSERT_EQUAL_STRING("/root.dir/file", std::string(PathUtility::removeExtension("/root.dir/file")).c_str());
    TEST_ASSERT_EQUAL_STRING("file", std::string(PathUtility::removeExtension("file")).c_str());
}

void test_path_utility_add_extension()
{
    TEST_ASSERT_EQUAL_STRING("/root/dir/file.txt", PathUtility::addExtension("/root/dir/file", "txt").c_str());
    TEST_ASSERT_EQUAL_STRING("archive.tar.gz", PathUtility::addExtension("archive.tar", "gz").c_str());
    TEST_ASSERT_EQUAL_STRING("/root/dir/file.txt", PathUtility::addExtension("/root/dir/file", ".txt").c_str());
    TEST_ASSERT_EQUAL_STRING("file", PathUtility::addExtension("file", "").c_str());
}

void test_path_utility_make_relative()
{
    TEST_ASSERT_EQUAL_STRING("dir/file.txt",
        std::string(PathUtility::makeRelative("/root/dir/file.txt", "/root")).c_str());
    TEST_ASSERT_EQUAL_STRING("dir/file.txt",
        std::string(PathUtility::makeRelative("/root/dir/file.txt", "/root/")).c_str());
    TEST_ASSERT_EQUAL_STRING("/dir/file.txt",
        std::string(PathUtility::makeRelative("/root/dir/file.txt", "/root", true)).c_str());
    TEST_ASSERT_EQUAL_STRING("/other/file.txt",
        std::string(PathUtility::makeRelative("/other/file.txt", "/root")).c_str());
}