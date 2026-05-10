cmake_minimum_required(VERSION 3.20)

project(bm25_tokenize_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

add_library(bm25_tokenize INTERFACE)
target_include_directories(
    bm25_tokenize
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
