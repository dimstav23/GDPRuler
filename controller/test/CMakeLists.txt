# Parent project does not export its library target, so this CML implicitly
# depends on being added from it, i.e. the testing is done only from the build
# tree and is not feasible from an install location

project(gdpr_controllerTests LANGUAGES CXX)

# ---- Tests ----

add_executable(gdpr_controller_test source/gdpr_controller_test.cpp)
target_link_libraries(gdpr_controller_test PRIVATE gdpr_controller_lib)
target_compile_features(gdpr_controller_test PRIVATE cxx_std_20)

add_test(NAME gdpr_controller_test COMMAND gdpr_controller_test)

add_executable(gdpr_logger_perf_test source/gdpr_logger_perf_test.cpp)
target_link_libraries(gdpr_logger_perf_test PRIVATE gdpr_controller_lib ${CMAKE_DL_LIBS})
target_compile_features(gdpr_logger_perf_test PRIVATE cxx_std_20)

add_test(NAME gdpr_logger_perf_test COMMAND gdpr_logger_perf_test)

add_executable(encryption_test source/encryption_test.cpp)
target_link_libraries(encryption_test PRIVATE gdpr_controller_lib ${CMAKE_DL_LIBS} OpenSSL::Crypto)
target_compile_features(encryption_test PRIVATE cxx_std_20)

add_test(NAME encryption_test COMMAND encryption_test)

# ---- End-of-file commands ----

add_folders(Test)
