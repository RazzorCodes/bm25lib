cmake_minimum_required(VERSION 3.20)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "" FORCE)
project(bm25_tokenize_tests LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(
    bm25_tokenize
    STATIC
        src/lib.cpp
        src/bm25.cpp
        src/core/ingester.cpp
        src/core/scorer.cpp
        src/core/ranker.cpp
)
target_include_directories(
    bm25_tokenize
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
