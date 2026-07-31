// Unit tests for HttpRequest class
// Tests: method, version, path, query, headers (case-insensitive), body, contentLength, isKeepAlive, dump
#include "test_framework.h"
#include "http/HttpRequest.h"

void test_request_method() {
    TEST_SUITE("HttpRequest Method");
    HttpRequest req;
    ASSERT_EQ(req.method(), HttpRequest::kInvalid);
    ASSERT_STREQ(req.methodString(), "INVALID");

    req.setMethod(HttpRequest::kGet);
    ASSERT_EQ(req.method(), HttpRequest::kGet);
    ASSERT_STREQ(req.methodString(), "GET");

    req.setMethod(HttpRequest::kPost);
    ASSERT_STREQ(req.methodString(), "POST");

    req.setMethod(HttpRequest::kHead);
    ASSERT_STREQ(req.methodString(), "HEAD");

    req.setMethod(HttpRequest::kPut);
    ASSERT_STREQ(req.methodString(), "PUT");

    req.setMethod(HttpRequest::kDelete);
    ASSERT_STREQ(req.methodString(), "DELETE");
}

void test_request_version() {
    TEST_SUITE("HttpRequest Version");
    HttpRequest req;
    ASSERT_EQ(req.version(), HttpRequest::kUnknown);
    req.setVersion(HttpRequest::kHttp10);
    ASSERT_EQ(req.version(), HttpRequest::kHttp10);
    req.setVersion(HttpRequest::kHttp11);
    ASSERT_EQ(req.version(), HttpRequest::kHttp11);
}

void test_request_path_query() {
    TEST_SUITE("HttpRequest Path & Query");
    HttpRequest req;
    req.setPath("/api/status");
    ASSERT_STREQ(req.path(), "/api/status");

    req.setQuery("page=1&limit=10");
    ASSERT_STREQ(req.query(), "page=1&limit=10");
}

void test_request_headers_case_insensitive() {
    TEST_SUITE("HttpRequest Headers (Case-Insensitive)");
    HttpRequest req;
    req.addHeader("Content-Type", "application/json");
    ASSERT_STREQ(req.getHeader("content-type"), "application/json");
    ASSERT_STREQ(req.getHeader("CONTENT-TYPE"), "application/json");
    ASSERT_STREQ(req.getHeader("Content-Type"), "application/json");

    // Overwrite existing
    req.addHeader("CONTENT-TYPE", "text/html");
    ASSERT_STREQ(req.getHeader("Content-Type"), "text/html");

    // Non-existent header returns empty string
    ASSERT_STREQ(req.getHeader("X-Custom"), "");
}

void test_request_body() {
    TEST_SUITE("HttpRequest Body");
    HttpRequest req;
    req.setBody("hello world");
    ASSERT_STREQ(req.body(), "hello world");

    req.appendBody("!!!");
    ASSERT_STREQ(req.body(), "hello world!!!");

    // Move semantics
    std::string big = "large body data";
    req.setBody(std::move(big));
    ASSERT_STREQ(req.body(), "large body data");
}

void test_request_content_length() {
    TEST_SUITE("HttpRequest Content-Length");
    HttpRequest req;
    ASSERT_EQ(req.contentLength(), 0u);

    req.addHeader("Content-Length", "1024");
    ASSERT_EQ(req.contentLength(), 1024u);

    // Invalid content-length returns 0
    req.addHeader("Content-Length", "abc");
    ASSERT_EQ(req.contentLength(), 0u);

    // No content-length header
    HttpRequest req2;
    req2.addHeader("Content-Type", "text/plain");
    ASSERT_EQ(req2.contentLength(), 0u);
}

void test_request_keep_alive() {
    TEST_SUITE("HttpRequest Keep-Alive");
    // HTTP/1.1 default keep-alive
    HttpRequest req11;
    req11.setVersion(HttpRequest::kHttp11);
    ASSERT_TRUE(req11.isKeepAlive());

    // HTTP/1.1 with Connection: close
    HttpRequest req11_close;
    req11_close.setVersion(HttpRequest::kHttp11);
    req11_close.addHeader("Connection", "close");
    ASSERT_FALSE(req11_close.isKeepAlive());

    // HTTP/1.0 default not keep-alive
    HttpRequest req10;
    req10.setVersion(HttpRequest::kHttp10);
    ASSERT_FALSE(req10.isKeepAlive());

    // HTTP/1.0 with Connection: keep-alive
    HttpRequest req10_ka;
    req10_ka.setVersion(HttpRequest::kHttp10);
    req10_ka.addHeader("Connection", "keep-alive");
    ASSERT_TRUE(req10_ka.isKeepAlive());
}

void test_request_has_body_expected() {
    TEST_SUITE("HttpRequest hasBodyExpected");
    HttpRequest get_req;
    get_req.setMethod(HttpRequest::kGet);
    ASSERT_FALSE(get_req.hasBodyExpected());

    HttpRequest post_req;
    post_req.setMethod(HttpRequest::kPost);
    ASSERT_TRUE(post_req.hasBodyExpected());

    HttpRequest put_req;
    put_req.setMethod(HttpRequest::kPut);
    ASSERT_TRUE(put_req.hasBodyExpected());

    HttpRequest head_req;
    head_req.setMethod(HttpRequest::kHead);
    ASSERT_FALSE(head_req.hasBodyExpected());
}

void test_request_dump() {
    TEST_SUITE("HttpRequest Dump");
    HttpRequest req;
    req.setMethod(HttpRequest::kGet);
    req.setVersion(HttpRequest::kHttp11);
    req.setPath("/api/status");
    req.addHeader("Host", "localhost:8080");

    std::string dumped = req.dump();
    ASSERT_CONTAINS(dumped, "GET /api/status HTTP/1.1");
    ASSERT_CONTAINS(dumped, "host: localhost:8080");
    ASSERT_CONTAINS(dumped, "\r\n\r\n");

    // With query and body
    HttpRequest req2;
    req2.setMethod(HttpRequest::kPost);
    req2.setVersion(HttpRequest::kHttp11);
    req2.setPath("/api/echo");
    req2.setQuery("verbose=1");
    req2.setBody("test body");
    std::string dumped2 = req2.dump();
    ASSERT_CONTAINS(dumped2, "POST /api/echo?verbose=1 HTTP/1.1");
    ASSERT_CONTAINS(dumped2, "test body");
}

int main() {
    test_request_method();
    test_request_version();
    test_request_path_query();
    test_request_headers_case_insensitive();
    test_request_body();
    test_request_content_length();
    test_request_keep_alive();
    test_request_has_body_expected();
    test_request_dump();
    TEST_SUMMARY();
}
