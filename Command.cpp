// Command.cpp —— 命令注册表实现
#include "Command.h"

void CommandRegistry::add(const std::string& verb, std::unique_ptr<Command> cmd) {
    commands_[verb] = std::move(cmd);
}

const Command* CommandRegistry::find(const std::string& verb) const {
    auto it = commands_.find(verb);
    return it == commands_.end() ? nullptr : it->second.get();
}
