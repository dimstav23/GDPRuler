# Build the test executable
add_executable(user_index_test
    test_index_manager.cpp
)

# Include project headers
target_include_directories(user_index_test PRIVATE
    ${CMAKE_SOURCE_DIR}/
)

# Register the test with CTest
add_test(
    NAME UserIndexTest
    COMMAND user_index_test
)
