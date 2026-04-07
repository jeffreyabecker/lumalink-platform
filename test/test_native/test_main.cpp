#include <unity.h>

#include <LumaLinkPlatform.h>

void test_project_name_is_exposed();
void test_contract_names_are_exposed();

// memory filesystem
void test_memory_file_can_write_and_read();
void test_memory_directory_and_list();
void test_memory_rename();

// native filesystem
void test_native_file_factory_can_open_and_read_temp_file();
void test_native_file_factory_supports_directory_handles();
void test_native_file_factory_can_write_and_reopen_file();

// native transport
void test_native_factory_wrapper_can_be_instantiated();
void test_native_factory_creates_tcp_server_and_client_loopback();
void test_native_factory_creates_udp_peers_for_loopback_packets();

void setUp()
{
}

void tearDown()
{
}

void test_project_name_is_exposed()
{
    TEST_ASSERT_EQUAL_STRING("lumalink-platform", lumalink::platform::ProjectName.data());
}

void test_contract_names_are_exposed()
{
    TEST_ASSERT_EQUAL_STRING("buffers", lumalink::platform::buffers::ContractName.data());
    TEST_ASSERT_EQUAL_STRING("transport", lumalink::platform::transport::ContractName.data());
    TEST_ASSERT_EQUAL_STRING("filesystem", lumalink::platform::filesystem::ContractName.data());
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_project_name_is_exposed);
    RUN_TEST(test_contract_names_are_exposed);
    RUN_TEST(test_memory_file_can_write_and_read);
    RUN_TEST(test_memory_directory_and_list);
    RUN_TEST(test_memory_rename);
    RUN_TEST(test_native_file_factory_can_open_and_read_temp_file);
    RUN_TEST(test_native_file_factory_supports_directory_handles);
    RUN_TEST(test_native_file_factory_can_write_and_reopen_file);
    RUN_TEST(test_native_factory_wrapper_can_be_instantiated);
    RUN_TEST(test_native_factory_creates_tcp_server_and_client_loopback);
    RUN_TEST(test_native_factory_creates_udp_peers_for_loopback_packets);
    return UNITY_END();
}
