# Parent project does not export its library target, so this CML implicitly
# depends on being added from it, i.e. the testing is done only from the build
# tree and is not feasible from an install location

project(gdpr_controllerTests LANGUAGES CXX)

# ---- Tests ----

add_executable(gdpr_controller_test source/gdpr_controller_test.cpp)
target_link_libraries(gdpr_controller_test PRIVATE gdpr_controller_lib)
target_compile_features(gdpr_controller_test PRIVATE cxx_std_17)

add_test(NAME gdpr_controller_test COMMAND gdpr_controller_test)

# ---- End-of-file commands ----

add_folders(Test)
