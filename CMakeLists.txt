cmake_minimum_required(VERSION 3.14)
project(Aether LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Linux feature macros (ensure eventfd/timerfd/SOCK_NONBLOCK availability)
add_definitions(-D_GNU_SOURCE)

# AddressSanitizer (optional, for debug builds)
option(ENABLE_ASAN "Enable AddressSanitizer in debug builds" OFF)

# Compile options
if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
    set(CMAKE_CXX_FLAGS_DEBUG "/Od /Zi")
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /DNDEBUG")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
    if(ENABLE_ASAN)
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address")
    endif()
    set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")
endif()

# Source files
file(GLOB_RECURSE SOURCES "src/*.cpp")

# Main executable
add_executable(aether ${SOURCES})
target_include_directories(aether PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Threads library
find_package(Threads REQUIRED)
target_link_libraries(aether PRIVATE Threads::Threads)

# Install target
install(TARGETS aether DESTINATION bin)

# Tests
option(BUILD_TESTS "Build tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()

# Install static files to build directory
file(COPY ${CMAKE_SOURCE_DIR}/www DESTINATION ${CMAKE_BINARY_DIR})
