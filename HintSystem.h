// HintSystem.h —— hint 动态提示 + 自动三级提示（60秒/30次移动/3次无效/锁门15秒等）
#pragma once

#include <string>
#include <vector>
#include <cstdint>

class Game;
class World;
struct GameState;

class HintSystem {
public:
    explicit HintSystem(Game& g) : game_(g) {}

    // 自动检测，返回需要显示的提示（空=现在不用提示）
    std::wstring tick(uint64_t now, World& world);

    // 推进事件（房间切换/拿物品/处置/对话等）
    void noteProgress(uint64_t now);

    // 统计事件
    void noteMove(uint64_t now);
    void noteInvalid();
    void noteLook(uint64_t now);
    void noteCombatUnavailable();

    // 锁门计时维护（Game 的步进逻辑调用）
    void setNearLocked(bool nearLock, uint64_t now);

    // hint 命令全文
    std::wstring hintText(World& world) const;

private:
    Game& game_;
    GameState& st() const;

    uint64_t lastProgressMs_ = 0;
    uint64_t cooldownUntilMs_ = 0;
    uint64_t hintLevel_ = 1;           // 连续无进展自动升级
    int movesInRoom_ = 0;
    int invalidStreak_ = 0;
    std::vector<uint64_t> lookTimes_;
    uint64_t nearLockMs_ = 0;          // 靠近锁门累计时长
    bool nearLock_ = false;
    int combatBad_ = 0;

    std::wstring level1() const;
    std::wstring level2() const;
    std::wstring level3() const;
};
