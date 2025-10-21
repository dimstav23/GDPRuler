find_package(OpenSSL REQUIRED)
find_package(GTest REQUIRED)
find_package(ZLIB REQUIRED)

message(STATUS "Using OpenSSL version: ${OPENSSL_VERSION}")
message(STATUS "Using GTest version: ${GTEST_VERSION}")
message(STATUS "Using ZLIB version: ${ZLIB_VERSION_STRING}")

include_directories(include)

add_subdirectory(external/concurrentqueue EXCLUDE_FROM_ALL)
include_directories(external/concurrentqueue)