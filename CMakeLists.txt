cmake_minimum_required(VERSION 3.20)
project(busy_beaver CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_program(CLANG_TIDY_EXE NAMES clang-tidy)
if(CLANG_TIDY_EXE)
    set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE};--config-file=${CMAKE_SOURCE_DIR}/.clang-tidy")
endif()

add_executable(bb src/main.cpp)

enable_testing()

add_executable(bb_test tests/test_main.cpp)
add_test(NAME bb_test COMMAND bb_test)
