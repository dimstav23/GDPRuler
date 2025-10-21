set(LIBRARY_SOURCES
    src/LogEntry.cpp
    src/Logger.cpp
    src/BufferQueue.cpp
    src/Compression.cpp
    src/Crypto.cpp
    src/Writer.cpp
    src/SegmentedStorage.cpp
    src/LoggingManager.cpp
    src/LogExporter.cpp
    src/TrustedCounter.cpp
    benchmarks/BenchmarkUtils.cpp
)

add_library(gdpr_logging_lib ${LIBRARY_SOURCES})
set_target_properties(gdpr_logging_lib PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Use absolute paths with PUBLIC visibility
target_include_directories(gdpr_logging_lib
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/concurrentqueue
)

target_link_libraries(gdpr_logging_lib
    PUBLIC 
    OpenSSL::SSL 
    OpenSSL::Crypto
    ZLIB::ZLIB
)