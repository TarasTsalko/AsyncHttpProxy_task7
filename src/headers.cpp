#include "headers.h"

#include <ranges>
#include <string>
#include <string_view>

using namespace std::string_view_literals;

using Callback = std::function<void(std::string_view, std::string_view)>;

void iterHeaders(std::string_view req, Callback &&callback) {
    const size_t end_headers = req.find(delimiter);
    if (end_headers == std::string_view::npos) {
        throw std::runtime_error("Invalid HTTP request: missing headers");
    }

    // Находим первую пустую строку (\r\n), которая отделяет request line от заголовков
    const size_t first_empty_line = req.find("\r\n");
    if (first_empty_line == end_headers) {
        throw std::runtime_error("Invalid HTTP request: missing request line");
    }

    // Получаем только заголовки, пропуская request line
    std::string_view headers = req.substr(first_empty_line + 2, end_headers - (first_empty_line + 2));
    size_t start = 0;
    while (start < headers.size()) {
        size_t end = headers.find("\r\n", start);
        if (end == std::string::npos)
            end = headers.length();
        const std::string_view line = headers.substr(start, end - start);
        start = end + 2;
        if (line.empty())
            continue;

        const size_t colon = line.find(':');
        if (colon == std::string_view::npos) {
            throw std::runtime_error("Invalid header format");
        }

        std::string_view name = line.substr(0, colon);
        std::string_view value = line.substr(colon + 1);
        value.remove_prefix(value.find_first_not_of(' '));

        callback(name, value);
    }
    // code here
}

std::pair<std::string, std::string> findHostPort(std::string_view req) {
    return {};
    // code here
}

std::optional<size_t> findContentLength(std::string_view rsp) {
    return std::nullopt;
    // code here
}
