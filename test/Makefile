# Aether unit tests — Makefile for direct g++ compilation (no cmake required)
# Usage: cd test && make && make test
SRC_DIR := ../src
CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -I$(SRC_DIR) -pthread

TESTS := test_buffer test_http_request test_http_response test_inet_address test_timestamp

.PHONY: all test clean

all: $(TESTS)

# Header-only tests
test_buffer: test_buffer.cpp test_framework.h
	$(CXX) $(CXXFLAGS) -o $@ $<

test_timestamp: test_timestamp.cpp test_framework.h
	$(CXX) $(CXXFLAGS) -o $@ $<

# Tests requiring .cpp implementations
test_http_request: test_http_request.cpp test_framework.h $(SRC_DIR)/http/HttpRequest.cpp
	$(CXX) $(CXXFLAGS) -o $@ test_http_request.cpp $(SRC_DIR)/http/HttpRequest.cpp

test_http_response: test_http_response.cpp test_framework.h $(SRC_DIR)/http/HttpResponse.cpp
	$(CXX) $(CXXFLAGS) -o $@ test_http_response.cpp $(SRC_DIR)/http/HttpResponse.cpp

test_inet_address: test_inet_address.cpp test_framework.h $(SRC_DIR)/net/InetAddress.cpp
	$(CXX) $(CXXFLAGS) -o $@ test_inet_address.cpp $(SRC_DIR)/net/InetAddress.cpp

test: $(TESTS)
	@echo ""
	@for t in $(TESTS); do \
		echo "── Running $$t ──"; \
		./$$t || exit 1; \
		echo ""; \
	done
	@echo "🎉 All test suites completed"

clean:
	rm -f $(TESTS)
