cmake_minimum_required(VERSION 3.20)

project(lumalink-platform LANGUAGES CXX)

add_library(lumalink-platform INTERFACE)
target_compile_features(lumalink-platform INTERFACE cxx_std_17)
target_include_directories(lumalink-platform INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)
