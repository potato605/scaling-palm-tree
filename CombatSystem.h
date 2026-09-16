// CombatSystem.h —— 回合制战斗：肃清/净化/控制、BOSS 三阶段、低血提示
#pragma once

#include <string>
#include <vector>
#include <utility>
#include <cstdint>

class Game;
struct Monster;

class CombatSystem {
public:
    explicit CombatSystem(Game& g) : game_(g) {}

    // 开战（当前房间内的怪物）；tutorial 控制首战教学
    void start(const std::string& monsterId, bool tutorial = false);
    bool active() const { return active_; }
    Monster* target() const;

    // 统一业务入口（attack / purify / control / use / guard）
    void handle(const std::string& verb);

    // 使用道具后的战斗回合：恢复生命后由敌人立即行动。
    void itemTurn();

    // 探索状态下的直接处置（条件满足时一步完成，与战斗内逻辑共用）
    void quickPurify(const std::string& monsterId);
    void quickControl(const std::string& monsterId);

    // 渲染数据
    const std::vector<std::pair<std::wstring, int>>& log() const { return log_; }

    // 结束战斗回到探索
    void finish();

private:
    Game& game_;
    std::string targetId_;
    bool active_ = false;
    bool guarded_ = false;
    int unavailableStreak_ = 0;
    std::vector<std::pair<std::wstring, int>> log_;

    void monsterActs(Monster& m);
    void checkDisposition(Monster& m);       // 血量触发的状态检查
    void applyEliminate(Monster& m);
    void applyPurify(Monster& m);
    void applyControl(Monster& m);
    void playerDeath(const std::wstring& cause);
    void pushLog(const std::wstring& text, int color = 7);
};
