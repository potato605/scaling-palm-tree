// CommandDispatcher.h —— 数字快捷与英文命令的唯一业务入口
#pragma once

#include <string>

class Game;

class CommandDispatcher {
public:
    explicit CommandDispatcher(Game& g) : game_(g) {}

    // 入口：中文命令拒绝、模式检查、Parser 解析、路由到 Game 的统一业务
    void dispatch(const std::string& rawLine);

private:
    Game& game_;
};
