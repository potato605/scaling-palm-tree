// SaveSystem.h —— 版本化文本存档：字段校验、坐标校验、一致性校验、损坏保护
#pragma once

#include <string>

class World;
struct GameState;

class SaveSystem {
public:
    static constexpr const char* SAVE_VERSION = "1";

    // 默认存档路径（相对工作目录 saves/slot1.txt；运行时自动创建目录）
    static std::string defaultPath();
    static bool exists(const std::string& path);

    // 写档；失败时返回错误说明，成功返回空
    static std::string save(const GameState& st, const World& world, const std::string& path);
    // 读档；成功后状态与怪物处置状态已应用
    // 失败（文件缺失/损坏/校验失败）返回错误说明，状态保持原样
    static std::string load(GameState& st, World& world, const std::string& path);

private:
    static const char* parseLine(const std::string& line, std::string& key, std::string& value);
    static std::string validate(const GameState& st, const World& world);
};
