// main.cpp —— 程序入口
#include <string>

#include "Game.h"

int main(int argc, char** argv) {
    bool selftest = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--selftest") selftest = true;
    }
    Game game(selftest);
    game.run();
    return 0;
}
