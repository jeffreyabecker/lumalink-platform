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

// clock types
void test_unix_time_comparison_operators();
void test_monotonic_millis_arithmetic();

// manual clock
void test_manual_clock_starts_with_no_utc_time();
void test_manual_clock_set_and_get_utc_time();
void test_manual_clock_clear_utc_time();
void test_manual_clock_advance_millis_monotonic();
void test_manual_clock_advance_millis_with_utc();
void test_manual_clock_advance_without_utc_does_not_set_utc();
void test_manual_clock_implements_iclock();

// time source / synchronizer
void test_utc_synchronizer_sets_clock_from_source();
void test_utc_synchronizer_leaves_clock_unchanged_on_source_failure();
void test_utc_time_result_bool_conversion();

// native clock implementations
void test_native_utc_clock_returns_a_value();
void test_native_monotonic_clock_is_non_decreasing();
void test_native_monotonic_clock_advances_over_delay();
void test_native_combined_clock_returns_both_times();

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
    RUN_TEST(test_unix_time_comparison_operators);
    RUN_TEST(test_monotonic_millis_arithmetic);
    RUN_TEST(test_manual_clock_starts_with_no_utc_time);
    RUN_TEST(test_manual_clock_set_and_get_utc_time);
    RUN_TEST(test_manual_clock_clear_utc_time);
    RUN_TEST(test_manual_clock_advance_millis_monotonic);
    RUN_TEST(test_manual_clock_advance_millis_with_utc);
    RUN_TEST(test_manual_clock_advance_without_utc_does_not_set_utc);
    RUN_TEST(test_manual_clock_implements_iclock);
    RUN_TEST(test_utc_synchronizer_sets_clock_from_source);
    RUN_TEST(test_utc_synchronizer_leaves_clock_unchanged_on_source_failure);
    RUN_TEST(test_utc_time_result_bool_conversion);
    RUN_TEST(test_native_utc_clock_returns_a_value);
    RUN_TEST(test_native_monotonic_clock_is_non_decreasing);
    RUN_TEST(test_native_monotonic_clock_advances_over_delay);
    RUN_TEST(test_native_combined_clock_returns_both_times);
    return UNITY_END();
}
