// HintSystem.cpp —— hint 动态提示 + 自动三级提示
#include "HintSystem.h"
#include "Game.h"
#include "QuestSystem.h"
#include "InteractionSystem.h"
#include "Item.h"
#include "World.h"

GameState& HintSystem::st() const { return game_.state(); }

void HintSystem::noteProgress(uint64_t now) {
    lastProgressMs_ = now;
    hintLevel_ = 1;
    movesInRoom_ = 0;
    invalidStreak_ = 0;
    lookTimes_.clear();
    nearLockMs_ = 0;
    combatBad_ = 0;
}

void HintSystem::noteMove(uint64_t) { ++movesInRoom_; }
void HintSystem::noteInvalid() { ++invalidStreak_; }
void HintSystem::noteLook(uint64_t now) {
    lookTimes_.push_back(now);
    // 只保留最近 10 条
    if (lookTimes_.size() > 10) lookTimes_.erase(lookTimes_.begin());
}
void HintSystem::noteCombatUnavailable() { combatBad_ += 2; }

std::wstring HintSystem::level1() const {
    return L"【提示】按 H 或输入 hint 查看任务提示；走到目标旁边按数字交互。";
}

std::wstring HintSystem::level2() const {
    const GameState& s = game_.state();
    std::wstring out = L"【提示】";
    out += L"当前目标：" + game_.quests().stage().next;
    if (s.hp * 100 < s.maxHp * 30) {
        out += L"；生命低于 30%，先使用生物营养块恢复（输 use）";
    }
    return out;
}

std::wstring HintSystem::level3() const {
    return L"【提示】" + game_.quests().stage().walkthrough;
}

// ------- 每帧自动检测 -------
std::wstring HintSystem::tick(uint64_t now, World& world) {
    GameState& s = game_.state();
    if (s.mode != GameMode::Exploration) return L"";
    if (cooldownUntilMs_ > now) return L"";

    Room* room = world.room(s.roomId);
    if (!room) return L"";

    std::wstring msg;
    uint64_t sinceProgress = lastProgressMs_ == 0 ? 0 : now - lastProgressMs_;

    // 触发条件组：60秒未推进 / 同房间移动30次 / 连续3次无效 / 连续 look / 锁门15秒
    if (sinceProgress > 60'000) {
        msg = level1();
    } else if (movesInRoom_ >= 30) {
        msg = level1();
    } else if (invalidStreak_ >= 3) {
        msg = L"【提示】输入好像不太对：命令均为英文（如 talk、attack、look），数字也可交互。";
    } else if (lookTimes_.size() >= 2 &&
               now - lookTimes_[lookTimes_.size() - 2] < 5000) {
        msg = L"【提示】你已经看了一圈（look）—— 按 mission 查看任务，或输入 hint。";
        lookTimes_.clear();
    } else if (nearLock_) {
        if (now - nearLockMs_ > 15'000) {
            msg = L"【提示】这道门需要对应授权。查看背包（inventory），找齐关键权限再回来。";
        }
    }

    if (msg.empty()) return L"";

    // 三级升级：无进展时连续触发逐级加深
    if (lastProgressMs_ != 0 && now - lastProgressMs_ > 45'000 && hintLevel_ < 3) {
        ++hintLevel_;
    }
    if (hintLevel_ >= 3) msg = level3();
    else if (hintLevel_ == 2) msg = level2();

    cooldownUntilMs_ = now + 20'000;   // 提示冷却 20 秒
    return msg;
}

// ------- 锁门计时（在尝试步进门时由 Game 调用维护状态；此处仅判断） -------
// 简化：近锁计时在 game 的 step 中处理，见 HintSystem::nearLock 由 Game 直接更新

void HintSystem::setNearLocked(bool nearLock, uint64_t now) {
    if (nearLock && !nearLock_) { nearLockMs_ = now; }
    if (!nearLock && nearLock_) { nearLockMs_ = 0; }
    nearLock_ = nearLock;
}

// ------- hint 命令全文 -------
std::wstring HintSystem::hintText(World& world) const {
    const GameState& s = game_.state();
    Room* room = world.room(s.roomId);
    std::wstring out;
    out += L"我在哪：" + (room ? room->name : L"?") + L"\n";
    out += L"做什么：" + game_.quests().stage().next + L"\n";
    out += L"去哪：" + game_.quests().directionText() + L"\n";
    out += L"找什么符号：当前目标 ";
    const std::wstring& sym = game_.quests().stage().targetSymbol;
    const std::wstring& cmd = game_.quests().stage().keyCmd;
    out += sym;
    out += L"\n按什么：靠近后按数字，或输入 " + cmd + L"\n";
    // 缺什么
    std::vector<std::wstring> missing;
    if (!s.hasKey("neural_protocol")) missing.push_back(L"神经控制协议（控制怪物/厄生需要，从赵诚处取得）");
    if (!s.hasKey("retrovirus")) missing.push_back(L"逆转录净化剂（净化厄生需要，从陈砚处取得）");
    if (!missing.empty()) {
        out += L"缺什么：";
        for (size_t i = 0; i < missing.size(); ++i) {
            if (i) out += L"；";
            out += missing[i];
        }
        out += L"\n";
    } else {
        out += L"缺什么：关键物品已齐，走主线即可。\n";
    }
    // 位置与方向的辅助
    if (room) {
        const auto& nearAct = InteractionSystem::build(
            const_cast<GameState&>(s), world, game_.quests().objectiveEntity());
        if (!nearAct.empty()) {
            out += L"附近对象：" + nearAct[0].chineseLabel + L"\n";
        }
    }
    return out;
}
