cmake_minimum_required(VERSION 3.20)
project(busy_beaver CXX)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# assert() を有効にするため Release フラグから -DNDEBUG を除去
string(REPLACE "-DNDEBUG" "" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")

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

option(BB_STAT "Enable per-phase timing statistics" ON)
option(BB_CHECKER_STAT "Enable detailed BbMachineUnstoppableChecker timing (use with --checker-stat)" ON)
option(BB_DEBUG_PERIODICITY "Enable debug output in check_periodicity" OFF)

set(BB_CORE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/bb_machine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/bb_macro.cpp
)

add_executable(bb src/main.cpp ${BB_CORE_SOURCES})
add_executable(bb_search src/bb_search.cpp ${BB_CORE_SOURCES})
target_link_libraries(bb PRIVATE argparse)
target_link_libraries(bb_search PRIVATE argparse)

if(BB_STAT)
    target_compile_definitions(bb_search PRIVATE BB_STAT)
endif()
if(BB_CHECKER_STAT)
    target_compile_definitions(bb_search PRIVATE BB_CHECKER_STAT)
    target_compile_definitions(bb_search PRIVATE BB_STAT)  # CheckerStat = Stat が必要
endif()

if(BB_DEBUG_PERIODICITY)
    target_compile_definitions(bb PRIVATE BB_DEBUG_PERIODICITY)
    target_compile_definitions(bb_search PRIVATE BB_DEBUG_PERIODICITY)
endif()

enable_testing()
add_subdirectory(tests)
