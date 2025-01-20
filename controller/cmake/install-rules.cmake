# install(
#     TARGETS gdpr_controller_exe
#     RUNTIME COMPONENT gdpr_controller_Runtime
# )
install(
    TARGETS native_controller_exe
    RUNTIME COMPONENT native_controller_Runtime
)


if(PROJECT_IS_TOP_LEVEL)
  include(CPack)
endif()
