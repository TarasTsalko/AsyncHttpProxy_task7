#include "headers.h"

#include <cassert>
#include <ranges>
#include <stdexcept>
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

    // Извлекаем только часть с заголовками
    std::string_view headers_part = req.substr(0, end_headers);
    auto lines = headers_part | std::views::split('\r') |
                 std::views::transform([](auto &&part) { return part | std::views::split('\n'); }) | std::views::join |
                 std::views::filter([](auto &&line) { return !line.empty(); }) | std::views::drop(1);

    for (auto &&line_view : lines) {
        std::string_view line = std::string_view(line_view.begin(), line_view.size());

        assert(!line.empty());
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
    std::string host;
    std::string port;

    iterHeaders(req, [&](std::string_view name, std::string_view value) {
        if (name == "Host") {
            size_t colon_pos = value.find(':');
            if (colon_pos != std::string_view::npos) {
                host = std::string(value.substr(0, colon_pos));
                port = std::string(value.substr(colon_pos + 1));
            } else {
                host = std::string(value);
                port = "80";
            }
        }
    });

    if (host.empty())
        throw std::runtime_error("Missing Host header in HTTP request");

    return {host, port};
    // code here
}

std::optional<size_t> findContentLength(std::string_view rsp) {
    std::optional<size_t> content_length;

    iterHeaders(rsp, [&](std::string_view name, std::string_view value) {
        if (name == "Content-Length") {
            // Используем std::from_chars для безопасного преобразования
            std::from_chars_result result;

            // Создаем временную переменную для хранения результата
            size_t number;
            result = std::from_chars(value.data(), value.data() + value.size(), number);
            // Проверяем успешность преобразования
            if (result.ec == std::errc() && result.ptr == value.data() + value.size()) {
                content_length = number;
            } else {
                content_length = std::nullopt;
            }
        }
    });

    return content_length;
    // code here
}
