#include <unity.h>

#include <LumaLinkPlatform.h>

// memory filesystem
void test_memory_file_can_write_and_read();
void test_memory_directory_and_list();
void test_memory_rename();
void test_scoped_filesystem_scopes_paths_and_exposes_scoped_view();
void test_scoped_filesystem_root_scope_passthroughs_paths();
void test_scoped_filesystem_exposes_scoped_directory_handles_and_entries();
void test_memory_filesystem_normalize_path_is_noop();
void test_scoped_filesystem_normalize_path_delegates_to_inner_filesystem();

// path utility
void test_path_utility_get_file_name();
void test_path_utility_get_dir_name();
void test_path_utility_join_relative_segments();
void test_path_utility_join_preserves_absolute_first_segment();
void test_path_utility_get_extension();
void test_path_utility_remove_extension();
void test_path_utility_make_relative();

// native filesystem
void test_native_filesystem_normalize_path();
#if !defined(_WIN32)
void test_posix_normalize_path_expands_home();
#endif
void test_native_file_factory_can_open_and_read_temp_file();
void test_native_file_factory_supports_directory_handles();
void test_native_file_factory_can_write_and_reopen_file();
#if defined(_WIN32)
void test_windows_forward_slash_in_open_and_exists();
void test_windows_list_returns_forward_slash_paths();
void test_windows_backslash_paths_supported();
void test_windows_normalize_path_expands_known_env_vars();
void test_windows_normalize_path_resolves_dot_to_cwd();
#endif

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
    RUN_TEST(test_scoped_filesystem_scopes_paths_and_exposes_scoped_view);
    RUN_TEST(test_scoped_filesystem_root_scope_passthroughs_paths);
    RUN_TEST(test_scoped_filesystem_exposes_scoped_directory_handles_and_entries);
    RUN_TEST(test_memory_filesystem_normalize_path_is_noop);
    RUN_TEST(test_scoped_filesystem_normalize_path_delegates_to_inner_filesystem);
    RUN_TEST(test_path_utility_get_file_name);
    RUN_TEST(test_path_utility_get_dir_name);
    RUN_TEST(test_path_utility_join_relative_segments);
    RUN_TEST(test_path_utility_join_preserves_absolute_first_segment);
    RUN_TEST(test_path_utility_get_extension);
    RUN_TEST(test_path_utility_remove_extension);
    RUN_TEST(test_path_utility_make_relative);
    RUN_TEST(test_native_filesystem_normalize_path);
#if !defined(_WIN32)
    RUN_TEST(test_posix_normalize_path_expands_home);
#endif
    RUN_TEST(test_native_file_factory_can_open_and_read_temp_file);
    RUN_TEST(test_native_file_factory_supports_directory_handles);
    RUN_TEST(test_native_file_factory_can_write_and_reopen_file);
#if defined(_WIN32)
    RUN_TEST(test_windows_forward_slash_in_open_and_exists);
    RUN_TEST(test_windows_list_returns_forward_slash_paths);
    RUN_TEST(test_windows_backslash_paths_supported);
    RUN_TEST(test_windows_normalize_path_expands_known_env_vars);
    RUN_TEST(test_windows_normalize_path_resolves_dot_to_cwd);
#endif
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
