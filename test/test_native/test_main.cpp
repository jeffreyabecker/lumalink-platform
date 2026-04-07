#include <unity.h>

#include <LumaLinkPlatform.h>

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

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
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
