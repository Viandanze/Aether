// Unit tests for HttpResponse class
// Tests: status code, headers, body, serialize, quick builders
#include "test_framework.h"
#include "http/HttpResponse.h"

void test_response_construction() {
    TEST_SUITE("HttpResponse Construction");
    HttpResponse resp;
    ASSERT_EQ(resp.statusCode(), HttpResponse::kUnknown);
    ASSERT_FALSE(resp.closeConnection());

    HttpResponse resp_close(true);
    ASSERT_TRUE(resp_close.closeConnection());
}

void test_response_status() {
    TEST_SUITE("HttpResponse Status");
    HttpResponse resp;
    resp.setStatusCode(HttpResponse::k200Ok);
    ASSERT_EQ(resp.statusCode(), HttpResponse::k200Ok);
    resp.setStatusMessage("OK");
}

void test_response_headers() {
    TEST_SUITE("HttpResponse Headers");
    HttpResponse resp;
    resp.addHeader("X-Custom", "value1");
    ASSERT_TRUE(resp.hasHeader("X-Custom"));

    resp.setContentType("application/json");
    ASSERT_TRUE(resp.hasHeader("Content-Type"));

    // No header
    ASSERT_FALSE(resp.hasHeader("X-Nonexistent"));
}

void test_response_body() {
    TEST_SUITE("HttpResponse Body");
    HttpResponse resp;
    std::string body = "{\"status\":\"ok\"}";
    resp.setBody(body);
    // Body is set internally, verify via serialize
    std::string serialized = resp.serialize();
    ASSERT_CONTAINS(serialized, body);

    // Move semantics
    HttpResponse resp2;
    std::string big = "large response body";
    resp2.setBody(std::move(big));
    ASSERT_CONTAINS(resp2.serialize(), "large response body");
}

void test_response_serialize_format() {
    TEST_SUITE("HttpResponse Serialize Format");
    HttpResponse resp;
    resp.setStatusCode(HttpResponse::k200Ok);
    resp.setStatusMessage("OK");
    resp.setContentType("text/html");
    resp.setBody("<h1>Hello</h1>");

    std::string s = resp.serialize();
    // Status line
    ASSERT_CONTAINS(s, "HTTP/1.1 200 OK\r\n");
    // Content-Type header
    ASSERT_CONTAINS(s, "Content-Type: text/html\r\n");
    // Auto Content-Length
    ASSERT_CONTAINS(s, "Content-Length: 14\r\n");
    // Auto Server header
    ASSERT_CONTAINS(s, "Server: Aether\r\n");
    // Connection header (keep-alive by default)
    ASSERT_CONTAINS(s, "Connection: keep-alive\r\n");
    // Body
    ASSERT_CONTAINS(s, "<h1>Hello</h1>");
    // Header terminator
    ASSERT_CONTAINS(s, "\r\n\r\n");
}

void test_response_serialize_close_connection() {
    TEST_SUITE("HttpResponse Serialize Close Connection");
    HttpResponse resp(true);  // close connection
    resp.setStatusCode(HttpResponse::k404NotFound);
    resp.setStatusMessage("Not Found");

    std::string s = resp.serialize();
    ASSERT_CONTAINS(s, "Connection: close\r\n");
    ASSERT_CONTAINS(s, "HTTP/1.1 404 Not Found\r\n");
}

void test_response_quick_builder_ok() {
    TEST_SUITE("HttpResponse Quick Builder: ok()");
    HttpResponse resp = HttpResponse::ok("body content", "text/plain");
    ASSERT_EQ(resp.statusCode(), HttpResponse::k200Ok);
    ASSERT_FALSE(resp.closeConnection());
    std::string s = resp.serialize();
    ASSERT_CONTAINS(s, "200 OK");
    ASSERT_CONTAINS(s, "text/plain");
    ASSERT_CONTAINS(s, "body content");
}

void test_response_quick_builder_not_found() {
    TEST_SUITE("HttpResponse Quick Builder: notFound()");
    HttpResponse resp = HttpResponse::notFound();
    ASSERT_EQ(resp.statusCode(), HttpResponse::k404NotFound);
    ASSERT_TRUE(resp.closeConnection());
    std::string s = resp.serialize();
    ASSERT_CONTAINS(s, "404 Not Found");
    ASSERT_CONTAINS(s, "404 Not Found</h1>");
    ASSERT_CONTAINS(s, "Connection: close");

    // Custom body
    HttpResponse resp2 = HttpResponse::notFound("custom 404");
    ASSERT_CONTAINS(resp2.serialize(), "custom 404");
}

void test_response_quick_builder_bad_request() {
    TEST_SUITE("HttpResponse Quick Builder: badRequest()");
    HttpResponse resp = HttpResponse::badRequest();
    ASSERT_EQ(resp.statusCode(), HttpResponse::k400BadRequest);
    ASSERT_TRUE(resp.closeConnection());
    ASSERT_CONTAINS(resp.serialize(), "400 Bad Request");
}

void test_response_quick_builder_forbidden() {
    TEST_SUITE("HttpResponse Quick Builder: forbidden()");
    HttpResponse resp = HttpResponse::forbidden();
    ASSERT_EQ(resp.statusCode(), HttpResponse::k403Forbidden);
    ASSERT_TRUE(resp.closeConnection());
    ASSERT_CONTAINS(resp.serialize(), "403 Forbidden");
}

void test_response_quick_builder_server_error() {
    TEST_SUITE("HttpResponse Quick Builder: serverError()");
    HttpResponse resp = HttpResponse::serverError();
    ASSERT_EQ(resp.statusCode(), HttpResponse::k500InternalServerError);
    ASSERT_TRUE(resp.closeConnection());
    ASSERT_CONTAINS(resp.serialize(), "500 Internal Server Error");
}

void test_response_quick_builder_method_not_allowed() {
    TEST_SUITE("HttpResponse Quick Builder: methodNotAllowed()");
    HttpResponse resp = HttpResponse::methodNotAllowed();
    ASSERT_EQ(resp.statusCode(), HttpResponse::k405MethodNotAllowed);
    ASSERT_TRUE(resp.closeConnection());
    ASSERT_CONTAINS(resp.serialize(), "405 Method Not Allowed");
}

void test_response_serialize_custom_content_length() {
    TEST_SUITE("HttpResponse Custom Content-Length");
    HttpResponse resp;
    resp.setStatusCode(HttpResponse::k200Ok);
    resp.setStatusMessage("OK");
    resp.setBody("12345");
    resp.addHeader("Content-Length", "999");  // Override auto
    std::string s = resp.serialize();
    // Should NOT add a second Content-Length
    size_t first = s.find("Content-Length:");
    size_t second = s.find("Content-Length:", first + 1);
    ASSERT_TRUE(first != std::string::npos);
    ASSERT_TRUE(second == std::string::npos);  // Only one Content-Length
    ASSERT_CONTAINS(s, "Content-Length: 999\r\n");
}

int main() {
    test_response_construction();
    test_response_status();
    test_response_headers();
    test_response_body();
    test_response_serialize_format();
    test_response_serialize_close_connection();
    test_response_quick_builder_ok();
    test_response_quick_builder_not_found();
    test_response_quick_builder_bad_request();
    test_response_quick_builder_forbidden();
    test_response_quick_builder_server_error();
    test_response_quick_builder_method_not_allowed();
    test_response_serialize_custom_content_length();
    TEST_SUMMARY();
}
