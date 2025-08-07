#include "headers.h"

#include <boost/asio.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <cassert>
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

constexpr std::string_view delimiter = "\r\n\r\n";

awaitable<void> session(tcp::socket client_socket, io_service &io_service) {
    try {
        std::cout << "New session started" << std::endl;

        // Буфер для чтения HTTP-запроса
        boost::asio::streambuf sb;

        // Читаем HTTP-запрос до разделителя
        size_t bytes_transferred =
            co_await boost::asio::async_read_until(client_socket, sb, delimiter, boost::asio::use_awaitable);

        assert(bytes_transferred > 0);

        // Преобразуем буфер в строку
        std::string request;
        std::istream is(&sb);
        std::getline(is, request);

        // Обрабатываем полученный запрос
        std::cout << "Received request:\n" << request << std::endl;

        // Формируем ответ
        std::string response = "HTTP/1.1 200 OK\r\n"
                               "Content-Type: text/plain\r\n"
                               "Connection: close\r\n\r\n"
                               "Hello, client!\n";

        // Отправляем ответ
        co_await boost::asio::async_write(client_socket, boost::asio::buffer(response), boost::asio::use_awaitable);

        // Закрываем сокет
        client_socket.close();
    } catch (const std::exception &e) {
        std::cerr << "Session error: " << e.what() << std::endl;
    }
}

class Server {
public:
    Server(io_service &io_service, short port)
        : io_service_(io_service), acceptor_(io_service, tcp::endpoint(tcp::v4(), port)), socket_(io_service) {
        do_accept();
    }

private:
    void do_accept() {
        std::cout << "Waiting for connection..." << std::endl;

        acceptor_.async_accept(socket_, [this](error_code ec) {
            if (!ec) {
                // Запускаем новую корутину для обработки сессии
                boost::asio::co_spawn(io_service_, session(std::move(socket_), io_service_), boost::asio::detached);
            } else {
                std::cerr << "Accept error: " << ec.message() << std::endl;
            }

            // Продолжаем принимать новые подключения
            do_accept();
        });
    }
    io_service &io_service_;
    tcp::acceptor acceptor_;
    tcp::socket socket_;
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
