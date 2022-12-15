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
    gdpr_controller_lib OBJECT
    source/lib.cpp
)

target_include_directories(
    gdpr_controller_lib ${warning_guard}
    PUBLIC
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/source>"
)

target_compile_features(gdpr_controller_lib PUBLIC cxx_std_17)

# ---- Declare executable ----

add_executable(gdpr_controller_exe source/main.cpp)
add_executable(gdpr_controller::exe ALIAS gdpr_controller_exe)

set_property(TARGET gdpr_controller_exe PROPERTY OUTPUT_NAME gdpr_controller)

target_compile_features(gdpr_controller_exe PRIVATE cxx_std_17)

target_link_libraries(gdpr_controller_exe PRIVATE gdpr_controller_lib)

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
