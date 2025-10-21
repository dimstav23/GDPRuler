set(BENCHMARK_LIBS
    PRIVATE
    gdpr_logging_lib
    OpenSSL::SSL
    OpenSSL::Crypto
    ZLIB::ZLIB
)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/benchmarks)

set(VALIDATION_BENCHMARKS
    batch_size
    concurrency
    scaling_concurrency
    encryption_compression_usage
    file_rotation
    queue_capacity
)

set(WORKLOAD_BENCHMARKS
    compression_ratio
    diverse_filepaths
    large_batches
    main
    multi_producer_small_batches
    single_entry_appends
    gdpruler_log_performance
    gdpruler_compression_rate
)

foreach(benchmark ${VALIDATION_BENCHMARKS})
    add_executable(${benchmark}_benchmark benchmarks/validation/${benchmark}.cpp)
    target_link_libraries(${benchmark}_benchmark ${BENCHMARK_LIBS})
endforeach()

foreach(benchmark ${WORKLOAD_BENCHMARKS})
    add_executable(${benchmark}_benchmark benchmarks/workloads/${benchmark}.cpp)
    target_link_libraries(${benchmark}_benchmark ${BENCHMARK_LIBS})
endforeach()