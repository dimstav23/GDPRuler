cmake_minimum_required(VERSION 3.28)
project(GDPR_Logging)

include(cmake/project_settings.cmake)
include(cmake/dependencies.cmake)
include(cmake/library.cmake)

# Only build the library by default - disable optional components
option(BUILD_TESTING "Build tests" OFF)
option(BUILD_BENCHMARKS "Build benchmarks" OFF)
option(BUILD_EXAMPLES "Build examples" OFF)

# Conditionally include additional components
if(BUILD_TESTING)
    include(cmake/testing.cmake)
endif()

if(BUILD_BENCHMARKS)
    include(cmake/benchmarks.cmake)
endif()

if(BUILD_EXAMPLES)
    include(cmake/examples.cmake)
endif()