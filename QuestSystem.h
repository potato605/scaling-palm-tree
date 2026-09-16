// QuestSystem.h —— 主线阶段、mission、防卡关、任务/关键物品一致性
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "GameState.h"
#include "World.h"

class Game;

class QuestSystem {
public:
    struct StageInfo {
        std::wstring name;        // 阶段名
        std::wstring next;        // 下一步描述
        int targetRoom;           // 目标房间
        std::wstring keyCmd;      // 推荐命令
        std::wstring targetSymbol;// 地图符号建议（'N'/'S'/'M'…）
        std::wstring walkthrough; // 手把手路线（hint 三级）
    };

    QuestSystem(Game& g) : game_(g) {}

    // 检查当前阶段是否完成；是则推进（可能连续推进）
    void checkAndAdvance();
    // 当前主线阶段信息
    const StageInfo& stage() const;
    int stageNum() const;
    static const StageInfo& stageInfo(int n);

    // mission 全文
    std::wstring missionText() const;
    // 当前目标实体 id（用于地图 '!' 标记）
    std::optional<std::string> objectiveEntity() const;
    // 目标房间与当前房间的方向文字
    std::wstring directionText() const;

    // 加载存档后校验任务/门禁/关键物品一致性（返回错误信息，无错为空）
    std::wstring verifyConsistency(const GameState& st) const;

private:
    Game& game_;
    GameState& st() const;
};
