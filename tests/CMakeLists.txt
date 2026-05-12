find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

add_executable(
    string_utils_test
    string_utils_test.cpp
    lib_tests.cpp
    core_tests.cpp
    sqlite_store_tests.cpp
    adapter_contract_tests.cpp
)

target_link_libraries(
    string_utils_test
    PRIVATE
        bm25_tokenize
        GTest::gtest_main
)

if(MSVC)
    target_compile_options(string_utils_test PRIVATE /W4 /permissive-)
else()
    target_compile_options(string_utils_test PRIVATE -Wall -Wextra -Wpedantic)
endif()

gtest_discover_tests(string_utils_test)

add_executable(
    perf_smoke_test
    perf_smoke_test.cpp
)

target_link_libraries(
    perf_smoke_test
    PRIVATE
        bm25_tokenize
        GTest::gtest_main
)

if(MSVC)
    target_compile_options(perf_smoke_test PRIVATE /W4 /permissive-)
else()
    target_compile_options(perf_smoke_test PRIVATE -Wall -Wextra -Wpedantic)
endif()

gtest_discover_tests(perf_smoke_test)
