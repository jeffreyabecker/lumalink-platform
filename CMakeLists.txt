cmake_minimum_required(VERSION 3.20)

project(lumalink-platform LANGUAGES CXX)

# ── Library target ────────────────────────────────────────────────────────────
add_library(lumalink-platform INTERFACE)
add_library(lumalink::platform ALIAS lumalink-platform)

target_compile_features(lumalink-platform INTERFACE cxx_std_17)

target_include_directories(lumalink-platform INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)

# ── Install rules ─────────────────────────────────────────────────────────────
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS lumalink-platform
    EXPORT lumalinkPlatformTargets
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT lumalinkPlatformTargets
    FILE      lumalinkPlatformTargets.cmake
    NAMESPACE lumalink::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/lumalinkPlatform
)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/lumalinkPlatformConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/lumalinkPlatformConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/lumalinkPlatform
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/lumalinkPlatformConfig.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/lumalinkPlatform
)

# ── Tests (top-level project only) ───────────────────────────────────────────
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    enable_testing()
    add_subdirectory(test/test_native)
endif()
