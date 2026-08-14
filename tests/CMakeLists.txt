add_executable(bb_test test_main.cpp ${CMAKE_SOURCE_DIR}/src/bb_machine.cpp)
add_test(NAME bb_test COMMAND bb_test)

add_test(NAME bb_search_2states COMMAND bb_search 2 1000)
set_tests_properties(bb_search_2states PROPERTIES
    PASS_REGULAR_EXPRESSION ": 6 steps"
    TIMEOUT 60
)

add_test(NAME bb_search_3states COMMAND bb_search 3 1000)
set_tests_properties(bb_search_3states PROPERTIES
    PASS_REGULAR_EXPRESSION ": 21 steps"
    TIMEOUT 120
)

add_test(NAME bb_search_4states COMMAND bb_search 4 1000)
set_tests_properties(bb_search_4states PROPERTIES
    PASS_REGULAR_EXPRESSION ": 107 steps"
    TIMEOUT 3600
    LABELS "slow"
)
