# ---------------------------------------------------------------------------------
# Build Configuration Options
# ---------------------------------------------------------------------------------

option(DEBUG_FLAG "Debug flag" OFF)
option(INTERNAL_TIMING "Internal timing" OFF)
option(METADATA_CACHE "Global metadata cache" ON)
option(CACHE_STATS "Collect cache stats" OFF)
option(ENCRYPTION_ENABLED "Enable encryption" ON)
option(ASAN_ENABLED "Enable Address Sanitizer" OFF)
option(TSAN_ENABLED "Enable Thread Sanitizer" OFF)

set(LOGGER_COMPRESSION_LEVEL "3" CACHE STRING "Compression level (0-9)")
# Validate range (0-9)
if(LOGGER_COMPRESSION_LEVEL LESS 0 OR LOGGER_COMPRESSION_LEVEL GREATER 9)
    message(FATAL_ERROR "LOGGER_COMPRESSION_LEVEL must be in the range [0,9]. You set: ${LOGGER_COMPRESSION_LEVEL}")
endif()

# ---------------------------------------------------------------------------------
# Status Messages (preserved from your original)
# ---------------------------------------------------------------------------------

message(STATUS "Build Configuration:")
message(STATUS "  Debug flag enabled: ${DEBUG_FLAG}")
message(STATUS "  INTERNAL_TIMING flag enabled: ${INTERNAL_TIMING}")
message(STATUS "  Global metadata cache enabled: ${METADATA_CACHE}")
if(METADATA_CACHE)
    message(STATUS "  Global metadata cache stats enabled: ${CACHE_STATS}")
endif()
message(STATUS "  Encryption enabled: ${ENCRYPTION_ENABLED}")
message(STATUS "  Logger compression level: ${LOGGER_COMPRESSION_LEVEL}")
message(STATUS "  ASan: ${ASAN_ENABLED}")
message(STATUS "  TSan: ${TSAN_ENABLED}")

# ---------------------------------------------------------------------------------
# Apply Configuration
# ---------------------------------------------------------------------------------

if(DEBUG_FLAG)
    add_definitions(-DDEBUG)
endif()

if(INTERNAL_TIMING)
    add_definitions(-DINTERNAL_TIMING)
endif()

if(METADATA_CACHE)
    add_definitions(-DMETADATA_CACHE)
    if(CACHE_STATS)
        add_definitions(-DCACHE_STATS)
    endif()
endif()

if(ENCRYPTION_ENABLED)
    add_definitions(-DENCRYPTION_ENABLED)
endif()

add_compile_definitions(LOGGER_COMPRESSION_LEVEL=${LOGGER_COMPRESSION_LEVEL})

if(ASAN_ENABLED)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fsanitize=leak -g")
endif()

if(TSAN_ENABLED)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread -g")
endif()
