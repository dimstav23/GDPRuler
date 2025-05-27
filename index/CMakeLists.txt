cmake_minimum_required(VERSION 3.20)

project(
    index_demo
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)

file(GLOB INDEX_SRC CONFIGURE_DEPENDS "*.cpp")
add_executable(index_demo 
    ${INDEX_SRC}
)

target_include_directories(index_demo PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
)

enable_testing()
add_subdirectory(tests)
