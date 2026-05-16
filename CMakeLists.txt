cmake_minimum_required(VERSION 3.20)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "" FORCE)
project(bm25_tokenize_tests LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(unofficial-sqlite3 CONFIG QUIET)
if(unofficial-sqlite3_FOUND)
  set(BM25_SQLITE_TARGET unofficial::sqlite3::sqlite3)
else()
  find_package(SQLite3 REQUIRED)
  set(BM25_SQLITE_TARGET SQLite::SQLite3)
endif()

add_library(
  bm25lib
    STATIC
        src/bm25.cpp
        src/core/ingester.cpp
        src/core/scorer.cpp
        src/core/ranker.cpp
)
add_library(bm25::bm25 ALIAS bm25lib)
target_include_directories(
bm25lib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(
bm25lib
    PUBLIC
        ${BM25_SQLITE_TARGET}
)

include(CTest)
if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
