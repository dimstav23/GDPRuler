# Parent project does not export its library target, so this CML implicitly
# depends on being added from it, i.e. the testing is done only from the build
# tree and is not feasible from an install location

project(gdpr_controllerTests LANGUAGES CXX)

# ---- Tests ----

add_executable(gdpr_controller_test source/gdpr_controller_test.cpp)
target_link_libraries(gdpr_controller_test PRIVATE gdpr_controller_lib OpenSSL::Crypto)
target_compile_features(gdpr_controller_test PRIVATE cxx_std_20)

add_test(NAME gdpr_controller_test COMMAND gdpr_controller_test)

add_executable(gdpr_logger_perf_test source/gdpr_logger_perf_test.cpp)
target_link_libraries(gdpr_logger_perf_test PRIVATE gdpr_controller_lib ${CMAKE_DL_LIBS} OpenSSL::Crypto)
target_compile_features(gdpr_logger_perf_test PRIVATE cxx_std_20)

add_test(NAME gdpr_logger_perf_test COMMAND gdpr_logger_perf_test)

add_executable(encryption_test source/encryption_test.cpp)
target_link_libraries(encryption_test PRIVATE gdpr_controller_lib ${CMAKE_DL_LIBS} OpenSSL::Crypto)
target_compile_features(encryption_test PRIVATE cxx_std_20)

add_test(NAME encryption_test COMMAND encryption_test)

if(ENABLE_GDPR_INDEX)
    # Index tests
    add_executable(index_test source/index_test.cpp)
    target_link_libraries(index_test PRIVATE gdpr_controller_lib)
    target_compile_features(index_test PRIVATE cxx_std_20)
    add_test(NAME IndexTests COMMAND index_test)
    
    # Index benchmark
    add_executable(index_benchmark source/index_benchmark.cpp)
    target_link_libraries(index_benchmark PRIVATE gdpr_controller_lib)
    target_compile_features(index_benchmark PRIVATE cxx_std_20)

    # Comparison benchmark
    add_executable(index_benchmark_comparison source/index_benchmark_comparison.cpp)
    target_link_libraries(index_benchmark_comparison PRIVATE gdpr_controller_lib)
    target_compile_features(index_benchmark_comparison PRIVATE cxx_std_20)
endif()

# ---- End-of-file commands ----

# Only call add_folders if it exists (when included from dev-mode)
if(COMMAND add_folders)
    add_folders(Test)
endif()
