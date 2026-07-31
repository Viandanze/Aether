# Aether unit tests
# Build: cmake -DBUILD_TESTS=ON .. && make
# Or use Makefile for direct g++ compilation

enable_testing()

# Include source directory
include_directories(${CMAKE_SOURCE_DIR}/src)

# Test sources that need .cpp implementations
set(HTTP_REQUEST_SRC ${CMAKE_SOURCE_DIR}/src/http/HttpRequest.cpp)
set(HTTP_RESPONSE_SRC ${CMAKE_SOURCE_DIR}/src/http/HttpResponse.cpp)
set(INET_ADDRESS_SRC ${CMAKE_SOURCE_DIR}/src/net/InetAddress.cpp)

# Test targets
add_executable(test_buffer test_buffer.cpp)
target_link_libraries(test_buffer PRIVATE Threads::Threads)

add_executable(test_http_request test_http_request.cpp ${HTTP_REQUEST_SRC})
target_link_libraries(test_http_request PRIVATE Threads::Threads)

add_executable(test_http_response test_http_response.cpp ${HTTP_RESPONSE_SRC})
target_link_libraries(test_http_response PRIVATE Threads::Threads)

add_executable(test_inet_address test_inet_address.cpp ${INET_ADDRESS_SRC})
target_link_libraries(test_inet_address PRIVATE Threads::Threads)

add_executable(test_timestamp test_timestamp.cpp)
target_link_libraries(test_timestamp PRIVATE Threads::Threads)

# CTest registration
add_test(NAME buffer COMMAND test_buffer)
add_test(NAME http_request COMMAND test_http_request)
add_test(NAME http_response COMMAND test_http_response)
add_test(NAME inet_address COMMAND test_inet_address)
add_test(NAME timestamp COMMAND test_timestamp)
