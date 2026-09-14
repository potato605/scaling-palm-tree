// CommandDispatcher.h —— 命令的唯一业务入口（命令模式 + 模式隔离）
#pragma once

#include <string>

#include "Command.h"

class Game;

class CommandDispatcher {
public:
    explicit CommandDispatcher(Game& game);

    // 入口：中文命令拒绝、模式检查、Parser 解析、命令查表执行
    void dispatch(const std::string& rawLine);

private:
    Game& game_;
    CommandRegistry registry_;
};
