cmake_minimum_required(VERSION 3.14)
project(Aether LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Linux feature macros（确保eventfd/timerfd/SOCK_NONBLOCK等可用）
add_definitions(-D_GNU_SOURCE)

# 编译选项
if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
    set(CMAKE_CXX_FLAGS_DEBUG "/Od /Zi")
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /DNDEBUG")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -fsanitize=address")
    set(CMAKE_CXX_FLAGS_RELEASE "-O2 /DNDEBUG")
endif()

# 源文件
file(GLOB_RECURSE SOURCES "src/*.cpp")

# 主程序
add_executable(aether ${SOURCES})
target_include_directories(aether PRIVATE ${CMAKE_SOURCE_DIR}/src)

# 线程库
find_package(Threads REQUIRED)
target_link_libraries(aether PRIVATE Threads::Threads)

# 安装目标
install(TARGETS aether DESTINATION bin)

# 测试
option(BUILD_TESTS "Build tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()

# 安装静态文件到构建目录
file(COPY ${CMAKE_SOURCE_DIR}/www DESTINATION ${CMAKE_BINARY_DIR})
