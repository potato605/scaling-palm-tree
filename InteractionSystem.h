// InteractionSystem.h —— 附近交互菜单（ContextAction）
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "GameState.h"

class World;

// 数字快捷交互项：数字 -> commandText -> CommandParser -> CommandDispatcher
struct ContextAction {
    int number = 0;
    int priority = 0;
    std::wstring chineseLabel;
    std::wstring englishLabel;
    std::wstring effectDescription;
    std::string commandText;        // 标准英文命令（如 talk linyue）
    bool enabled = true;
    std::wstring unavailableReason;
};

class InteractionSystem {
public:
    // 收集当前房间内曼哈顿距离 1 的目标，按优先级排序并编号
    // 排序：主线目标 > 关键NPC > 门 > 敌人 > 关键道具 > 终端/档案 > 普通容器 > 已完成对象
    static std::vector<ContextAction> build(GameState& st, World& world,
                                            const std::optional<std::string>& objectiveId);
};
