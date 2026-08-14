cmake_minimum_required(VERSION 3.20)
project(busy_beaver CXX)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(argparse
    GIT_REPOSITORY https://github.com/p-ranav/argparse.git
    GIT_TAG v3.1
)
FetchContent_MakeAvailable(argparse)

find_program(CLANG_TIDY_EXE NAMES clang-tidy)
if(CLANG_TIDY_EXE)
    set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE};--config-file=${CMAKE_SOURCE_DIR}/.clang-tidy;--header-filter=${CMAKE_SOURCE_DIR}/src/.*")
endif()

add_executable(bb src/main.cpp src/bb_machine.cpp)
add_executable(bb_search src/bb_search.cpp src/bb_machine.cpp)
target_link_libraries(bb_search PRIVATE argparse)

enable_testing()
add_subdirectory(tests)
