cmake_minimum_required(VERSION 3.14)

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

# ---- Declare library ----

add_library(
    gdpr_controller_lib SHARED
    source/default_policy.cpp
)

target_include_directories(
    gdpr_controller_lib ${warning_guard}
    PUBLIC
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/source>"
)

target_compile_features(gdpr_controller_lib PUBLIC cxx_std_17)

# ---- Add custom project dependencies ----

# ---- Add hiredis dependency ----
find_path(HIREDIS_HEADER hiredis)
find_library(HIREDIS_LIB hiredis)

# ---- Add redis-plus-plus dependency ----
# NOTE: this should be *sw* NOT *redis++*
find_path(REDIS_PLUS_PLUS_HEADER sw)
find_library(REDIS_PLUS_PLUS_LIB redis++)

# ---- Declare executables ----

# main
add_executable(gdpr_controller_exe source/main.cpp)
add_executable(gdpr_controller::exe ALIAS gdpr_controller_exe)

set_property(TARGET gdpr_controller_exe PROPERTY OUTPUT_NAME gdpr_controller)

target_compile_features(gdpr_controller_exe PRIVATE cxx_std_17)

target_link_libraries(gdpr_controller_exe PRIVATE gdpr_controller_lib ${HIREDIS_LIB} ${REDIS_PLUS_PLUS_LIB})

# redis interface
add_executable(redis_interface_exe source/redis_interface.cpp)
add_executable(redis_interface::exe ALIAS redis_interface_exe)

set_property(TARGET redis_interface_exe PROPERTY OUTPUT_NAME redis_interface)

target_compile_features(redis_interface_exe PRIVATE cxx_std_17)

target_link_libraries(redis_interface_exe PRIVATE gdpr_controller_lib ${HIREDIS_LIB} ${REDIS_PLUS_PLUS_LIB})
target_include_directories(redis_interface_exe SYSTEM PRIVATE ${HIREDIS_HEADER} ${REDIS_PLUS_PLUS_HEADER})

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