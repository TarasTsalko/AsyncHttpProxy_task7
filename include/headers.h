#pragma once

#include <functional>
#include <optional>
#include <string>

constexpr std::string_view delimiter = "\r\n\r\n";

using Callback = std::function<void(std::string_view, std::string_view)>;

struct HostPort {
    std::string host;
    std::string port;
};

void iterHeaders(std::string_view req, Callback &&callback);

HostPort parseHostWithPort(std::string_view req);

std::optional<size_t> findContentLength(std::string_view rsp);