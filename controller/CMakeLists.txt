cmake_minimum_required(VERSION 3.30)

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

# Set the default value for ENCRYPTION_ENABLED to ON
option(ENCRYPTION_ENABLED "Enable encryption" ON)

# Add a preprocessor definition based on the ENCRYPTION_ENABLED option
if(ENCRYPTION_ENABLED)
  add_definitions(-DENCRYPTION_ENABLED)
  message(STATUS "Encryption enabled: ON")
else()
  message(STATUS "Encryption enabled: OFF")
endif()

if(ASAN_ENABLED)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address  -fsanitize=leak -g")
  message(STATUS "ASan: ON")
else()
  message(STATUS "ASan: OFF")
endif()

if(TSAN_ENABLED)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread -g")
  message(STATUS "TSan: ON")
else()
  message(STATUS "TSan: OFF")
endif()


# ---- Declare library ----

add_library(
    gdpr_controller_lib SHARED
    source/default_policy.cpp
    source/query.cpp
    source/query_rewriter.cpp
    source/gdpr_filter.cpp
    source/gdpr_regulator.cpp
)

target_include_directories(
    gdpr_controller_lib ${warning_guard}
    PUBLIC
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/source>"
)

target_compile_features(gdpr_controller_lib PUBLIC cxx_std_20)

# ---- Add custom project dependencies ----

# ---- Add hiredis dependency ----
find_path(HIREDIS_HEADER hiredis)
find_library(HIREDIS_LIB hiredis)

# ---- Add redis-plus-plus dependency ----
# NOTE: this should be *sw* NOT *redis++*
find_path(REDIS_PLUS_PLUS_HEADER sw)
find_library(REDIS_PLUS_PLUS_LIB redis++)

# ---- Add rocksdb dependency ----
# find_path(ROCKSDB_HEADER rocksdb)
find_library(ROCKSDB_LIB rocksdb)

# ---- Add boost dependency ----
find_package(Boost CONFIG REQUIRED COMPONENTS system)

# ---- Add OpenSSL dependency ----
find_package(OpenSSL REQUIRED COMPONENTS Crypto)

# ---- Declare executables ----

# main
add_executable(gdpr_controller_exe source/controller.cpp)
add_executable(gdpr_controller::exe ALIAS gdpr_controller_exe)

set_property(TARGET gdpr_controller_exe PROPERTY OUTPUT_NAME gdpr_controller)

target_compile_features(gdpr_controller_exe PRIVATE cxx_std_20)

target_link_libraries(gdpr_controller_exe PRIVATE gdpr_controller_lib ${HIREDIS_LIB} ${REDIS_PLUS_PLUS_LIB} ${ROCKSDB_LIB} ${Boost_LIBRARIES} OpenSSL::Crypto)

# native controller
add_executable(native_controller_exe source/native_controller.cpp)
add_executable(native_controller::exe ALIAS native_controller_exe)

set_property(TARGET native_controller_exe PROPERTY OUTPUT_NAME native_controller)

target_compile_features(native_controller_exe PRIVATE cxx_std_20)

target_link_libraries(native_controller_exe PRIVATE gdpr_controller_lib ${HIREDIS_LIB} ${REDIS_PLUS_PLUS_LIB} ${ROCKSDB_LIB} ${Boost_LIBRARIES} OpenSSL::Crypto)

# redis interface
add_executable(test_kv_client_driver_exe source/test_kv_client_driver.cpp)
add_executable(test_kv_client_driver::exe ALIAS test_kv_client_driver_exe)

set_property(TARGET test_kv_client_driver_exe PROPERTY OUTPUT_NAME test_kv_client_driver)

target_compile_features(test_kv_client_driver_exe PRIVATE cxx_std_20)

target_link_libraries(test_kv_client_driver_exe PRIVATE gdpr_controller_lib ${HIREDIS_LIB} ${REDIS_PLUS_PLUS_LIB} OpenSSL::Crypto)
target_include_directories(test_kv_client_driver_exe SYSTEM PRIVATE ${HIREDIS_HEADER} ${REDIS_PLUS_PLUS_HEADER})

# rocksdb server
add_executable(rocksdb_server_exe source/rocksdb_server/server.cpp)
add_executable(rocksdb_server::exe ALIAS rocksdb_server_exe)

set_property(TARGET rocksdb_server_exe PROPERTY OUTPUT_NAME rocksdb_server)

target_compile_features(rocksdb_server_exe PRIVATE cxx_std_20)

target_link_libraries(rocksdb_server_exe PRIVATE ${Boost_LIBRARIES} ${ROCKSDB_LIB})

# ---- Install rules ----

if(NOT CMAKE_SKIP_INSTALL_RULES)
  include(cmake/install-rules.cmake)
endif()

# ---- Developer mode ----

if(NOT gdpr_controller_DEVELOPER_MODE)
  return()
elseif(NOT PROJECT_IS_TOP_LEVEL)
  message(
      AUTHOR_WARNING
      "Developer mode is intended for developers of gdpr_controller"
  )
endif()

include(cmake/dev-mode.cmake)
