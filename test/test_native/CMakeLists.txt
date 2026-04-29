include(FetchContent)

FetchContent_Declare(
    unity
    GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
    GIT_TAG        v2.6.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(unity)

add_executable(lumalink_platform_tests
    test_main.cpp
    test_filesystem_native.cpp
    test_memory_filesystem.cpp
    test_path_utility.cpp
    test_time.cpp
    test_transport_native.cpp
)

target_compile_features(lumalink_platform_tests PRIVATE cxx_std_23)

target_include_directories(lumalink_platform_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../support/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../support/native_arduino
)

target_compile_options(lumalink_platform_tests PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W3 /permissive->
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra>
)

target_link_libraries(lumalink_platform_tests PRIVATE
    lumalink::platform
    unity
    $<$<PLATFORM_ID:Windows>:ws2_32>
)

add_test(NAME lumalink_platform_tests COMMAND lumalink_platform_tests)
