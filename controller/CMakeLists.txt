cmake_minimum_required(VERSION 3.28)

include(cmake/prelude.cmake)

project(
    gdpr_controller
    VERSION 0.1.0
    DESCRIPTION "Controller module of GDPRuler"
    HOMEPAGE_URL "https://example.com/"
    LANGUAGES CXX
)

include(cmake/project-is-top-level.cmake)
include(cmake/variables.cmake)

# ---------------------------------------------------------------------------------
# Load Configuration Options
# ---------------------------------------------------------------------------------
include(cmake/options.cmake)

# ---------------------------------------------------------------------------------
# External Dependencies
# ---------------------------------------------------------------------------------

# Add external GDPR logger
set(GDPR_LOGGER_SOURCE_DIR "${CMAKE_SOURCE_DIR}/../gdpr-logger")
set(GDPR_LOGGER_BUILD_DIR "${CMAKE_BINARY_DIR}/external/gdpr-logger")

if(NOT EXISTS "${GDPR_LOGGER_SOURCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "GDPR Logger not found at: ${GDPR_LOGGER_SOURCE_DIR}")
endif()

add_subdirectory("${GDPR_LOGGER_SOURCE_DIR}" "${GDPR_LOGGER_BUILD_DIR}")

# Find system packages
find_package(Boost CONFIG REQUIRED COMPONENTS system)
find_package(OpenSSL REQUIRED COMPONENTS Crypto SSL)
find_package(ZLIB REQUIRED)
# Link Abseil if indexes enabled
if(ENABLE_GDPR_INDEX)
    find_package(absl REQUIRED)
    # Roaring bitmap library
    include(FetchContent)
    FetchContent_Declare(
        roaring
        GIT_REPOSITORY https://github.com/RoaringBitmap/CRoaring.git
        GIT_TAG v2.0.4
    )
    FetchContent_MakeAvailable(roaring)
endif()

# Find manual libraries
find_path(HIREDIS_HEADER hiredis)
find_library(HIREDIS_LIB hiredis)
find_path(REDIS_PLUS_PLUS_HEADER sw)
find_library(REDIS_PLUS_PLUS_LIB redis++)
find_path(ROCKSDB_HEADER rocksdb)
find_library(ROCKSDB_LIB rocksdb)

# ---------------------------------------------------------------------------------
# Main Library: gdpr_controller_lib
# ---------------------------------------------------------------------------------

# Collect source files
set(GDPR_CONTROLLER_SOURCES
    source/default_policy.cpp
    source/query.cpp
    source/query_rewriter.cpp
    source/gdpr_filter.cpp
    source/gdpr_regulator.cpp
)

# Add index sources if enabled
if(ENABLE_GDPR_INDEX)
    list(APPEND GDPR_CONTROLLER_SOURCES
        source/gdpr_index/hashmap_index.cpp
        source/gdpr_index/btree_index.cpp
        source/gdpr_index/bit_inverted_index_hashset.cpp
        source/gdpr_index/bit_inverted_index_roaring.cpp
        source/gdpr_index/index_manager.cpp
    )
endif()

add_library(gdpr_controller_lib SHARED
    ${GDPR_CONTROLLER_SOURCES}
)

target_include_directories(gdpr_controller_lib
    PUBLIC
        "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/source>"
)

target_compile_features(gdpr_controller_lib PUBLIC cxx_std_20)

target_link_libraries(gdpr_controller_lib
    PUBLIC
        gdpr_logging_lib        # Logger propagates to consumers
        OpenSSL::Crypto
        ${REDIS_PLUS_PLUS_LIB}
        ${HIREDIS_LIB}
    PRIVATE
        # Private dependencies
        ${ROCKSDB_LIB}
        ${Boost_LIBRARIES}
        ZLIB::ZLIB
        OpenSSL::SSL
)

if(ENABLE_GDPR_INDEX)
    target_link_libraries(gdpr_controller_lib
        PUBLIC
            absl::btree
            absl::flat_hash_map
            absl::flat_hash_set
            roaring::roaring
    )
endif()

# ---------------------------------------------------------------------------------
# Helper Function for Executables
# ---------------------------------------------------------------------------------

function(add_gdpruler_executable name source_file output_name)
    add_executable(${name} ${source_file})
    set_property(TARGET ${name} PROPERTY OUTPUT_NAME ${output_name})
    target_compile_features(${name} PRIVATE cxx_std_20)
    target_link_libraries(${name} PRIVATE gdpr_controller_lib)
endfunction()

# ---------------------------------------------------------------------------------
# Executables
# ---------------------------------------------------------------------------------

add_gdpruler_executable(gdpr_controller_exe source/controller.cpp gdpr_controller)
add_gdpruler_executable(native_controller_exe source/native_controller.cpp native_controller)
add_gdpruler_executable(test_kv_client_driver_exe source/test_kv_client_driver.cpp test_kv_client_driver)
add_gdpruler_executable(direct_kv_client_exe source/direct_kv_client.cpp direct_kv_client)

# RocksDB server
add_executable(rocksdb_server_exe source/rocksdb_server/server.cpp)
set_property(TARGET rocksdb_server_exe PROPERTY OUTPUT_NAME rocksdb_server)
target_compile_features(rocksdb_server_exe PRIVATE cxx_std_20)
target_link_libraries(rocksdb_server_exe PRIVATE ${Boost_LIBRARIES} ${ROCKSDB_LIB})

# ---------------------------------------------------------------------------------
# Tests (Build with -DBUILD_TESTING=ON)
# ---------------------------------------------------------------------------------

include(CTest)
if(BUILD_TESTING)
    message(STATUS "Building tests")
    add_subdirectory(test)
endif()

# ---------------------------------------------------------------------------------
# Install and Dev Mode
# ---------------------------------------------------------------------------------

if(NOT CMAKE_SKIP_INSTALL_RULES)
    include(cmake/install-rules.cmake)
endif()

if(NOT gdpr_controller_DEVELOPER_MODE)
    return()
elseif(NOT PROJECT_IS_TOP_LEVEL)
    message(AUTHOR_WARNING "Developer mode is intended for developers of gdpr_controller")
endif()

include(cmake/dev-mode.cmake)
