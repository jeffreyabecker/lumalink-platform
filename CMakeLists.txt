cmake_minimum_required(VERSION 3.30)

project(lumalink-platform LANGUAGES CXX)

option(LUMALINK_ENABLE_MODULES "Enable exported C++20 module support" OFF)

# ── Library target ────────────────────────────────────────────────────────────
add_library(lumalink-platform)
add_library(lumalink::platform ALIAS lumalink-platform)

target_compile_features(lumalink-platform PUBLIC cxx_std_23)

target_sources(lumalink-platform
    PRIVATE
        src/lumalink/platform_target.cpp
)

target_include_directories(lumalink-platform PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)

if(LUMALINK_ENABLE_MODULES)
    target_sources(lumalink-platform
        PUBLIC
            FILE_SET lumalink_platform_modules TYPE CXX_MODULES FILES
                src/lumalink/platform/lumalink.platform.cppm
    )

    if(WIN32)
        target_sources(lumalink-platform
            PUBLIC
                FILE_SET lumalink_platform_modules TYPE CXX_MODULES FILES
                    src/windows/lumalink.platform.windows.cppm
        )
    elseif(UNIX)
        target_sources(lumalink-platform
            PUBLIC
                FILE_SET lumalink_platform_modules TYPE CXX_MODULES FILES
                    src/posix/lumalink.platform.posix.cppm
        )
    endif()

    target_compile_definitions(lumalink-platform PUBLIC LUMALINK_ENABLE_MODULES=1)
endif()

# ── Install rules ─────────────────────────────────────────────────────────────
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

if(LUMALINK_ENABLE_MODULES)
    install(TARGETS lumalink-platform
        EXPORT lumalinkPlatformTargets
        FILE_SET lumalink_platform_modules DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/lumalink/platform
    )
else()
    install(TARGETS lumalink-platform
        EXPORT lumalinkPlatformTargets
    )
endif()

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
