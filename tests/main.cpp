#include <gtest/gtest.h>

#include <cstdlib>  // system

void stop_process(const std::string &process_name) {
    std::string command = "ps aux | grep \"" + process_name + "\" | grep -v grep | awk '{print $2}' | xargs kill -9";
    system(command.c_str());
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    auto ret = RUN_ALL_TESTS();
    stop_process("AsyncHttpProxy");
    return ret;
}
