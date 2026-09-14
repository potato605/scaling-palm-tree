// Command.h —— 命令模式（Command Pattern）
//
// 把"动词"封装为命令对象，由 CommandDispatcher 统一查表执行：
//   Receiver（Game）持有真正的业务逻辑，Command 持有对 Receiver 的调用；
//   CommandRegistry 以 unordered_map 维护 动词 -> 命令对象，命令对象由
//   unique_ptr 唯一拥有，析构自动释放。
//
// MethodCommand 模板把 Game 的成员函数适配为命令对象，避免为每个动词
// 手写一个派生类——这是命令模式 + 现代 C++ 模板的惯用组合。
#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class Game;

// 抽象命令接口：所有可执行动词的统一接口（Invoker 为 CommandDispatcher）
class Command {
public:
    virtual ~Command() = default;
    virtual void execute(Game& game, const std::vector<std::string>& args) const = 0;
};

// 把 Game 的成员函数适配为命令对象
template <auto Fn>
class MethodCommand : public Command {
public:
    void execute(Game& game, const std::vector<std::string>& args) const override {
        (game.*Fn)(args);
    }
};

// 便捷工厂：make_command<&Game::doLook>()
template <auto Fn>
std::unique_ptr<Command> make_command() {
    return std::make_unique<MethodCommand<Fn>>();
}

// 命令注册表：动词 -> 命令对象（unique_ptr 唯一拥有）
class CommandRegistry {
public:
    void add(const std::string& verb, std::unique_ptr<Command> cmd);
    const Command* find(const std::string& verb) const;

private:
    std::unordered_map<std::string, std::unique_ptr<Command>> commands_;
};
