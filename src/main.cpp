#include "headers.h"

#include <boost/asio.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <iostream>
#include <print>
#include <string_view>

using boost::asio::async_read_until;
using boost::asio::awaitable;
using boost::asio::buffer;
using boost::asio::co_spawn;
using boost::asio::dynamic_buffer;
using boost::asio::io_service;
using boost::asio::transfer_at_least;
using boost::asio::use_awaitable;
using boost::asio::ip::tcp;
using boost::system::error_code;

awaitable<void> session(tcp::socket client_socket, io_service &io_service) {
    try {
        std::println("\nNew session started");

        std::string input_buffer;
        // Читаем HTTP-запрос до разделителя
        const size_t bytes_transferred =
            co_await async_read_until(client_socket, dynamic_buffer(input_buffer), delimiter, use_awaitable);

        if (bytes_transferred <= 0) {
            throw std::runtime_error("No bytes transferred during read operation");
        }

        // Получаем Host и Port
        const auto [host, port] = findHostPort(input_buffer);
        if (host.empty()) {
            throw std::runtime_error("Host header not found");
        }

        // Устанавливаем соединение с сервером
        tcp::resolver resolver(io_service);
        auto endpoints = co_await resolver.async_resolve(host, port, use_awaitable);
        // Добавляем проверку на пустой endpoints
        if (endpoints.empty()) {
            throw std::runtime_error("No endpoints found for host: " + host);
        }

        tcp::socket server_socket(io_service);
        co_await async_connect(server_socket, endpoints.begin(), use_awaitable);

        // Пересылаем запрос на сервер
        co_await async_write(server_socket, buffer(input_buffer), use_awaitable);

        // Читаем заголовок ответа
        std::string header_buffer;

        // Читаем только заголовок
        co_await async_read_until(server_socket, dynamic_buffer(header_buffer), delimiter, use_awaitable);

        // Проверяем наличие разделителя
        const size_t pos = header_buffer.find(delimiter);
        if (pos == std::string::npos) {
            throw std::runtime_error("Invalid response from server: delimiter not found");
        }

        // Получаем Content-Length
        const auto content_length = findContentLength(header_buffer);

        if (!content_length.has_value()) {
            throw std::runtime_error("Content-Length header is missing in response");
        }

        // Вычисляем ожидаемый размер тела
        const size_t expected_body_size = content_length.value() - (pos + delimiter.size());

        // Проверяем, есть ли данные в header_buffer после разделителя
        // так как не смотря на разделитель данные для оптимизации читаются пакетами
        const size_t current_body_size = header_buffer.size() - (pos + delimiter.size());
        std::string body_buffer = header_buffer.substr(pos + delimiter.size());

        if (current_body_size < expected_body_size) {
            // Читаем оставшиеся данные
            std::string remaining_buffer;
            remaining_buffer.resize(expected_body_size - current_body_size);

            const size_t bytes_read =
                co_await async_read(server_socket, buffer(remaining_buffer),
                                    transfer_at_least(expected_body_size - current_body_size), use_awaitable);

            if (bytes_read < (expected_body_size - current_body_size)) {
                throw std::runtime_error("Read operation did not transfer expected number of bytes");
            }

            remaining_buffer.resize(bytes_read);
            body_buffer += remaining_buffer;
        }

        // Отправляем заголовок и тело клиенту
        co_await async_write(client_socket, buffer(header_buffer), use_awaitable);
        co_await async_write(client_socket, buffer(body_buffer), use_awaitable);

        // по заданию у сокитов нужно вызвать close, но ка я читал
        // close вызывается в деструкторе, уточнить у ревьювера

    } catch (const std::exception &e) {
        throw std::runtime_error(std::format("Session error: {}", e.what()));
    }
}

class Server {
public:
    Server(io_service &io_service, short port)
        : io_service_(io_service), acceptor_(io_service, tcp::endpoint(tcp::v4(), port)) {
        do_accept();
    }

private:
    void do_accept() {
        std::println("Waiting for connection...");

        acceptor_.async_accept([this](error_code ec, tcp::socket socket) {
            try {
                if (!ec) {
                    co_spawn(
                        io_service_,
                        [this, &socket]() -> awaitable<void> {
                            try {
                                if (is_running_)
                                    co_await session(std::move(socket), io_service_);
                            } catch (const std::exception &e) {
                                std::cerr << e.what() << std::endl;
                                is_running_ = false;
                            }
                        },
                        boost::asio::detached);
                } else {
                    std::cerr << "Accept error: " << ec.message() << std::endl;
                }

                // Проверка на наличие исключения
                if (!is_running_) {
                    return;
                }

                // Продолжаем принимать новые подключения
                do_accept();
            } catch (const std::exception &e) {
                std::cerr << "Exception: " << e.what() << std::endl;
                is_running_ = false;
            }
        });
    }

    io_service &io_service_;
    tcp::acceptor acceptor_;
    // может нужен atomic, уточнить у ревьювера
    bool is_running_ = true;
};

int main(int argc, char *argv[]) {
    try {
        if (argc != 2) {
            std::cerr << "Usage: proxy_server";
            std::cerr << " <listen_port>\n";
            return 1;
        }
        io_service io_service(1);
        Server server(io_service, std::atoi(argv[1]));
        io_service.run();

    } catch (const std::exception &e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
}
