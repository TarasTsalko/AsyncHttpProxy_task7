#include "headers.h"
#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <vector>

TEST(iterHeaders, Empty) {

    try {
        const std::string_view empty_request = "";
        iterHeaders(empty_request, [](std::string_view name, std::string_view value) {});
    } catch (const std::runtime_error &e) {
        ASSERT_EQ(std::string(e.what()), std::string("Invalid HTTP request: missing headers"));
    }
    // code here
}

TEST(iterHeaders, MissingRequestLineDelimiter) {

    const auto callback = [](std::string_view, std::string_view) {};

    // Случай 1: полностью отсутствует разделитель \r\n
    std::string_view invalid_request = "GET /index.html HTTP/1.1Host: example.com\r\n\r\n";
    {
        try {
            iterHeaders(invalid_request, callback);
            FAIL() << "Expected exception not thrown";
        } catch (const std::runtime_error &e) {
            EXPECT_STREQ("Invalid HTTP request: missing request line", e.what());
        }
    }

    // Случай 2: неправильный разделитель
    {
        invalid_request = "GET /index.html HTTP/1.1\nHost: example.com\r\n\r\n";
        try {
            iterHeaders(invalid_request, callback);
            FAIL() << "Expected exception not thrown";
        } catch (const std::runtime_error &e) {
            EXPECT_STREQ("Invalid HTTP request: missing request line", e.what());
        }
    }

    // Случай 3: request line отсутствует вообще
    {
        invalid_request = "Host: example.com\r\n\r\n";
        try {
            iterHeaders(invalid_request, callback);
            FAIL() << "Expected exception not thrown";
        } catch (const std::runtime_error &e) {
            EXPECT_STREQ("Invalid HTTP request: missing request line", e.what());
        }
    }
}

TEST(iterHeaders, SkipRequestLine) {

    const std::string_view request = "GET http://127.0.0.1:8000/ HTTP/1.1\r\n"  // Request line
                                     "Host: example.com\r\n"
                                     "User-Agent: Mozilla/5.0\r\n"
                                     "Accept: text/html\r\n\r\n";

    const std::vector<std::pair<std::string, std::string>> expected = {
        {"Host", "example.com"}, {"User-Agent", "Mozilla/5.0"}, {"Accept", "text/html"}};

    std::vector<std::pair<std::string, std::string>> collected;
    auto callback = [&collected](std::string_view name, std::string_view value) {
        collected.emplace_back(name, value);
    };
    iterHeaders(request, callback);
    EXPECT_EQ(collected, expected);
    // code here
}

TEST(iterHeaders, SingleHeader) {

    const std::string_view request = "GET http://127.0.0.1:8000/ HTTP/1.1\r\n"
                                     "User-Agent: Mozilla/5.0\r\n\r\n";
    const std::pair<std::string, std::string> expected = {"User-Agent", "Mozilla/5.0"};

    std::pair<std::string, std::string> collected;
    auto callback = [&collected](std::string_view name, std::string_view value) {
        collected.first = name, collected.second = value;
    };
    iterHeaders(request, callback);
    EXPECT_EQ(collected, expected);
    // code here
}

TEST(iterHeaders, MultipleHeaders) {
    const std::string_view request = "GET http://127.0.0.1:8000/ HTTP/1.1\r\n"
                                     "Host: example.com\r\n"
                                     "Connection: keep-alive\r\n"
                                     "Accept: text/html\r\n\r\n";

    const std::vector<std::pair<std::string, std::string>> expected = {
        {"Host", "example.com"}, {"Connection", "keep-alive"}, {"Accept", "text/html"}};

    std::vector<std::pair<std::string, std::string>> collected;
    auto callback = [&collected](std::string_view name, std::string_view value) {
        collected.emplace_back(name, value);
    };
    iterHeaders(request, callback);
    EXPECT_EQ(collected, expected);
    // code here
}

TEST(iterHeaders, MultipleSameHeaders) {

    const std::string_view request = "GET http://127.0.0.1:8000/ HTTP/1.1\r\n"
                                     "Cookie: sessionid=123\r\n"
                                     "Cookie: userid=456\r\n"
                                     "Cookie: token=abc\r\n\r\n";

    const std::vector<std::pair<std::string, std::string>> expected = {
        {"Cookie", "sessionid=123"}, {"Cookie", "userid=456"}, {"Cookie", "token=abc"}};

    std::vector<std::pair<std::string, std::string>> collected;
    auto callback = [&collected](std::string_view name, std::string_view value) {
        collected.emplace_back(name, value);
    };
    iterHeaders(request, callback);
    EXPECT_EQ(collected, expected);
    // code here
}

TEST(findHostPort, Simple) {
    std::string request;
    // Тест 1: простой случай с хостом и портом
    {
        request = "GET / HTTP/1.1\r\n"
                  "Host: example.com:8080\r\n"
                  "User-Agent: test\r\n"
                  "Connection: close\r\n"
                  "\r\n";

        const auto [host, port] = parseHostWithPort(request);
        EXPECT_EQ(host, "example.com");
        EXPECT_EQ(port, "8080");
    }

    // Тест 2: случай только с хостом (без порта)
    {
        request = "GET / HTTP/1.1\r\n"
                  "Host: example.com\r\n"
                  "User-Agent: test\r\n"
                  "Connection: close\r\n"
                  "\r\n";

        const auto [host, port] = parseHostWithPort(request);
        EXPECT_EQ(host, "example.com");
        EXPECT_EQ(port, "80");  // порт по умолчанию
    }

    // Тест 3: случай с IP-адресом и портом
    {
        request = "GET / HTTP/1.1\r\n"
                  "Host: 192.168.1.1:8000\r\n"
                  "User-Agent: test\r\n"
                  "Connection: close\r\n"
                  "\r\n";

        const auto [host, port] = parseHostWithPort(request);
        EXPECT_EQ(host, "192.168.1.1");
        EXPECT_EQ(port, "8000");
    }

    // Тест 4: случай с поддоменом
    {
        request = "GET / HTTP/1.1\r\n"
                  "Host: sub.example.com:443\r\n"
                  "User-Agent: test\r\n"
                  "Connection: close\r\n"
                  "\r\n";

        auto [host, port] = parseHostWithPort(request);
        EXPECT_EQ(host, "sub.example.com");
        EXPECT_EQ(port, "443");
    }
    // code here
}

TEST(findHostPort, NoHost) {
    try {
        const std::string_view invalid_request = "GET http://127.0.0.1:8000/ HTTP/1.1\r\n"  // Request line
                                                 "User-Agent: Mozilla/5.0\r\n"
                                                 "Accept: text/html\r\n\r\n";
        parseHostWithPort(invalid_request);
        FAIL() << "Expected exception not thrown";
    } catch (const std::runtime_error &e) {
        EXPECT_STREQ("Missing Host header in HTTP request", e.what());
    }
    // code here
}

TEST(findContentLength, Simple) {
    std::string response;
    // Тест 1: простой случай с корректным Content-Length
    {
        response = "GET / HTTP/1.1\r\n"
                   "Host: example.com:8080\r\n"
                   "Content-Type: text/html\r\n"
                   "Content-Length: 1234\r\n"
                   "\r\n"
                   "<html>...</html>";

        const auto result = findContentLength(response);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result.value(), 1234);
    }

    // Тест 2: Content-Length с нулем
    {
        response = "GET / HTTP/1.1\r\n"
                   "Host: example.com:8080\r\n"
                   "Content-Length: 0\r\n"
                   "\r\n";

        const auto result = findContentLength(response);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result.value(), 0);
    }
    // code here
}

TEST(findContentLength, NoContentLength) {
    const std::string response = "GET / HTTP/1.1\r\n"
                                 "Host: example.com:8080\r\n"
                                 "\r\n";
    const auto result = findContentLength(response);
    ASSERT_FALSE(result.has_value());
    // code here
}