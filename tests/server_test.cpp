#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>  // system
#include <format>
#include <memory>
#include <string>
#include <thread>

// Функция для запуска процесса через систему
bool run_process(std::string_view command) { return system(command.data()) == 0; }

std::string readFromPipe(std::string_view command) {
    // Создаем пайпы для stdout и stderr
    FILE *pipeOut = popen(command.data(), "r");

    if (!pipeOut) {
        throw std::runtime_error("Ошибка при выполнении popen");
    }

    using Deleter = void (*)(FILE *);
    std::unique_ptr<FILE, Deleter> out_handle(pipeOut, [](FILE *f) { pclose(f); });

    std::array<char, 1024> buffer;
    std::stringstream outputStream;

    // Читаем stdout
    size_t bytesRead;
    const size_t bytes_per_element = 1;
    while ((bytesRead = fread(buffer.data(), bytes_per_element, buffer.size(), out_handle.get())) > 0) {
        outputStream.write(buffer.data(), bytesRead);
    }

    return outputStream.str();
}

TEST(AsyncHttpProxyTest, BasicFunctionalityTest) {
    const int proxy_port = 5555;
    const int target_port = 8000;

    // Запускаем прокси-сервер в фоновом режиме
    ASSERT_TRUE(run_process(std::string("./AsyncHttpProxy ") + std::to_string(proxy_port) + " &"));

    // Даём время на запуск сервера
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Запускаем тестовый HTTP-сервер
    std::string server_command =
        std::format("python3 -c 'print(\"HTTP/1.1 200 OK\\r\\nContent-Type: text/html\\r\\nContent-Length: "
                    "8192\\r\\n\\r\\n\" + \"B\"*8192, end=\"\")' | nc -l 127.0.0.1 -p {} &",
                    target_port);

    ASSERT_TRUE(run_process(server_command));

    // Даём время на запуск тестового сервера
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Выполняем запрос через прокси
    std::string curl_command =
        std::format("curl -s -x http://127.0.0.1:{} http://127.0.0.1:{}", proxy_port, target_port);

    std::string response = readFromPipe(curl_command);
    // Проверяем, что получили ожидаемый ответ
    EXPECT_EQ(response.size(), 8192);
    EXPECT_EQ(std::ranges::count(response, 'B'), 8192);

    server_command =
        std::format("python3 -c 'print(\"HTTP/1.1 200 OK\\r\\nContent-Type: text/html\\r\\nContent-Length: "
                    "4096\\r\\n\\r\\n\" + \"C\"*4096, end=\"\")' | nc -l 127.0.0.1 -p {} &",
                    target_port);
    ASSERT_TRUE(run_process(server_command));

    response = readFromPipe(curl_command);
    // Проверяем, что получили ожидаемый ответ
    EXPECT_EQ(response.size(), 4096);
    EXPECT_EQ(std::ranges::count(response, 'C'), 4096);
}

TEST(AsyncHttpProxyTest, ZeroPortTest) {
    const std::string invalid_port = "0";
    const std::string command = std::format("./AsyncHttpProxy {} 2>&1 >/dev/null", invalid_port);

    // Читаем из stderr, передавая true как второй параметр
    const std::string error = readFromPipe(command);

    const std::string expected_error =
        std::format("Error: invalid port number. Port must be an integer between 1 and 65535\n"
                    "Invalid input: '{}'\n",
                    invalid_port);

    EXPECT_EQ(error, expected_error);
}