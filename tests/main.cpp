#include <gtest/gtest.h>

#include <cstdlib>  // system
#include <string>

// нелбходимо завершить процесс AsyncHttpProxy после всех тестов, чтобы освободился порт для следуюшего запуска
void stop_process(std::string_view process_name) {
    const std::string command =
        std::format("ps aux | grep \"{}\" | grep -v grep | awk '{{print $2}}' | xargs kill -9", process_name);
    system(command.c_str());
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    const int ret = RUN_ALL_TESTS();
    stop_process("AsyncHttpProxy");
    return ret;
}
