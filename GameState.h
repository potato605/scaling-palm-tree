// GameState.h —— 全局游戏状态与模式定义
#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <utility>
#include <unordered_set>
#include <unordered_map>

// 游戏模式（严格隔离，各模式只接受允许的输入）
enum class GameMode {
    Title,          // 标题页：新游戏 / 读档 / 退出
    Narrative,      // 剧情逐字播放：只接受 Enter 补全当前行
    Exploration,    // 自由探索：WASD / 数字 / 命令行 / H / F5 / Q
    Selection,      // 数字选项菜单：1~9，可取消菜单允许 0
    Combat,         // 战斗：数字快捷 + 攻击/净化/控制/使用
    CommandInput,   // Command > 命令行输入
    PauseOrConfirm, // 二次确认：1 确认 / 2 取消
    Ending,         // 结局
    GameOver        // 死亡结算
};

// 全局游戏状态（SaveSystem 持久化字段全集）
struct GameState {
    // ---- 模式与位置 ----
    GameMode mode = GameMode::Title;
    int roomId = 0;
    int px = 1, py = 1;

    // ---- 玩家属性 ----
    int hp = 120, maxHp = 120;
    int attack = 18, defense = 6;
    int level = 1, exp = 0;

    // ---- 主线与进度 ----
    int questStage = 1;              // 1~12，见 QuestSystem
    int checkpointRoom = 0, checkpointX = 1, checkpointY = 1;
    int elapsedSeconds = 0;          // 总用时（正常时间）
    int deaths = 0;

    // ---- 结局统计 ----
    int eliminateCount = 0;          // 肃清数
    int purifyCount = 0;             // 净化数
    int controlCount = 0;            // 控制数
    int correctAnswers = 0;          // 生态题正确数
    int archivesRead = 0;            // 已阅读档案数
    std::wstring finalDisposition;   // 最终处置（肃清/净化/控制）

    // ---- 物品 ----
    std::unordered_set<std::string> keyItems;                 // 关键物品（不可丢弃/消耗/死亡丢失）
    std::unordered_map<std::string, int> items;               // 普通消耗品 id -> 数量（数量不小于 0）

    // ---- 标志（对话完成 / 教学 / 检查点等）----
    std::unordered_set<std::string> flags;
    std::unordered_set<std::string> quizDone;                 // 已完成的生态题（NPC id）
    std::unordered_set<std::string> archivesDone;             // 已阅读的档案 id
    std::unordered_map<std::string, std::string> monsterStates; // "怪物id" -> "alive/weakened/purified/controlled/eliminated"

    // ---- 消息队列（最近 4 条，附颜色）----
    std::vector<std::pair<std::wstring, int>> messages;

    // ---------- 便捷方法 ----------
    bool hasKey(const std::string& id) const { return keyItems.count(id) > 0; }
    void addKey(const std::string& id) { keyItems.insert(id); }

    bool hasFlag(const std::string& name) const { return flags.count(name) > 0; }
    void setFlag(const std::string& name) { flags.insert(name); }

    int itemCount(const std::string& id) const {
        auto it = items.find(id);
        return it == items.end() ? 0 : it->second;
    }

    // 添加消息，保留最近 4 条
    void addMessage(const std::wstring& text, int color = 7) {
        messages.push_back({ text, color });
        if (messages.size() > 4) messages.erase(messages.begin());
    }

    // 死亡后恢复可行动生命值（返回检查点处使用）
    void restoreVitals() {
        hp = maxHp * 3 / 5;
        if (hp < 1) hp = 1;
    }
};
