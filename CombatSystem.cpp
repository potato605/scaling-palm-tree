// CombatSystem.cpp —— 回合制战斗实现
#include "CombatSystem.h"
#include "Game.h"
#include "Monster.h"
#include "Item.h"
#include "World.h"
#include "Player.h"

#include <chrono>
#include <random>

namespace {
    std::mt19937& rng() {
        static std::mt19937 gen((unsigned)std::chrono::steady_clock::now().time_since_epoch().count());
        return gen;
    }
    int smallOffset() { return (int)(rng()() % 5) - 2; }  // -2..2 小随机偏移
    int rollChance(int pct) { return (int)(rng()() % 100) < pct; }
}

Monster* CombatSystem::target() const {
    World* w = game_.world();
    Room* room = w ? w->room(game_.state().roomId) : nullptr;
    if (!room) return nullptr;
    for (auto& m : room->monsters) {
        if (m->id == targetId_) return m.get();
    }
    return nullptr;
}

void CombatSystem::pushLog(const std::wstring& text, int color) {
    log_.push_back({ text, color });
    while (log_.size() > 6) log_.erase(log_.begin());
}

void CombatSystem::start(const std::string& monsterId, bool tutorial) {
    if (active_) return;
    targetId_ = monsterId;
    Monster* m = target();
    if (!m || !m->alive()) return;

    // 首战教学：先由 Game 播放教学剧情（见 Game::startCombat），这里只初始化
    targetId_ = monsterId;
    active_ = true;
    unavailableStreak_ = 0;
    log_.clear();
    if (tutorial) {
        pushLog(L"（基础教学）" + m->name + L"出现：先攻击削弱，直到它进入【基因衰弱】（HP 低于 35% 会自动提示），再净化或控制；也可以直接肃清。", 11);
    }
    pushLog(L"你遭遇了 " + m->name + L"！", 12);
    game_.state().mode = GameMode::Combat;
    game_.reqRender();
}

std::vector<std::wstring> CombatSystem::menuText() const {
    return { L"[1] 攻击", L"[2] 净化", L"[3] 控制", L"[4] 使用道具" };
}

void CombatSystem::handle(const std::string& verb) {
    if (!active_) return;
    Monster* m = target();
    if (!m || !m->alive()) { finish(); return; }
    GameState& st = game_.state();

    if (verb == "attack") {
        int dmg = st.attack - m->defense + smallOffset();
        if (dmg < 1) dmg = 1;
        m->hp -= dmg;
        pushLog(L"你的攻击命中：" + m->name + L" 损失 " + std::to_wstring(dmg) + L" 点生命（剩余 " + std::to_wstring(m->hp < 0 ? 0 : m->hp) + L"/" + std::to_wstring(m->maxHp) + L"）。", 15);

        if (m->boss) {
            // 三阶段：1 外壳防御 → 2 核心暴露 → 3 意识崩解
            if (m->hp <= 0) {
                if (m->bossPhase == 1) {
                    m->bossPhase = 2;
                    m->maxHp = 70;
                    m->hp = 70;
                    pushLog(L"骨甲碎裂！【核心暴露 / Core Exposed】—— 逆转录净化剂已满足最终净化条件之一。", 11);
                } else if (m->bossPhase == 2) {
                    m->bossPhase = 3;
                    m->maxHp = 80;
                    m->hp = 80;
                    pushLog(L"幼体意识崩塌！【意识崩解 / Mind Collapse】—— 神经控制协议已满足最终控制条件之一。", 11);
                } else {
                    active_ = false;
                    game_.openFinalDisposal();   // 三种处置菜单
                    return;
                }
                game_.reqRender();
                return;   // 阶段切换不反击
            }
        } else {
            // 击杀 → 肃清；未死且低血 → 基因衰弱
            if (m->hp <= 0) {
                applyEliminate(*m);
                return;   // 本轮结束
            }
            checkDisposition(*m);
            if (!m->alive()) return;   // 已进入衰弱（等待净化/控制），怪物本轮不再反击
        }

        if (m->alive()) monsterActs(*m);
        game_.reqRender();
    }
    else if (verb == "purify") {
        // 条件不足不消耗回合/道具（直接说明）
        if (!m->weakened()) {
            pushLog(L"它尚不具备净化条件：需要先攻击至【基因衰弱】（HP 低于 35%，会自动提示）。", 8);
            ++unavailableStreak_;
            if (unavailableStreak_ >= 2) pushLog(L"提示：continue 攻击怪物，观察它的 HP 条，低于 35% 后按 2 净化。", 14);
            game_.reqRender();
            return;
        }
        applyPurify(*m);
    }
    else if (verb == "control") {
        if (!st.hasKey("neural_protocol")) {
            pushLog(L"缺少【神经控制协议】：去安保值班室找赵诚交谈获得。", 8);
            ++unavailableStreak_;
            game_.reqRender();
            return;
        }
        if (!m->weakened()) {
            pushLog(L"目标尚不稳定：需先进入【基因衰弱】（HP<35%）后才能植入协议。", 8);
            ++unavailableStreak_;
            game_.reqRender();
            return;
        }
        applyControl(*m);
    }
    else if (verb == "use") {
        game_.doUse({});   // 打开道具子菜单（不离开战斗模式）
    }
}

void CombatSystem::quickPurify(const std::string& monsterId) {
    Monster* m = nullptr;
    Room* room = game_.world()->room(game_.state().roomId);
    if (room) {
        for (auto& mm : room->monsters) if (mm->id == monsterId) m = mm.get();
    }
    if (!m || !m->alive() || m->boss) return;
    if (!m->weakened()) {
        game_.post(L"净化条件：目标 HP 低于 35%（基因衰弱）。先攻击削弱它。", 12);
        return;
    }
    applyPurify(*m);
}

void CombatSystem::quickControl(const std::string& monsterId) {
    Monster* m = nullptr;
    Room* room = game_.world()->room(game_.state().roomId);
    if (room) {
        for (auto& mm : room->monsters) if (mm->id == monsterId) m = mm.get();
    }
    if (!m || !m->alive() || m->boss) return;
    if (!m->weakened()) {
        game_.post(L"控制条件：目标基因衰弱（HP<35%）且需要神经控制协议。", 12);
        return;
    }
    applyControl(*m);
}

void CombatSystem::checkDisposition(Monster& m) {
    if (m.boss || m.disp != Monster::Disposition::Alive) return;
    if (m.hpLow() && m.hp > 0) {
        m.disp = Monster::Disposition::Weakened;
        pushLog(m.weakMsg, 11);
    }
}

void CombatSystem::applyEliminate(Monster& m) {
    GameState& st = game_.state();
    m.hp = 0;
    m.disp = Monster::Disposition::Eliminated;
    ++st.eliminateCount;
    pushLog(m.name + L" 已被肃清。污染通道暂时切断。", 12);
    int ups = Player::addExp(st, m.exp);
    if (ups > 0) pushLog(L"等级提升！Lv." + std::to_wstring(st.level) + L" —— 生命、攻击、防御提升并完全恢复。", 11);
    game_.quests().checkAndAdvance();
    finish();
}

void CombatSystem::applyPurify(Monster& m) {
    GameState& st = game_.state();
    m.disp = Monster::Disposition::Purified;
    st.purifyCount++;
    pushLog(L"净化完成：" + m.name + L" 停止了攻击，地图标记为 +。", 10);
    pushLog(L"它恢复了平静，但长期修复之路才刚开始。", 13);
    int ups = Player::addExp(st, m.exp);
    if (ups > 0) pushLog(L"等级提升！Lv." + std::to_wstring(st.level), 11);
    game_.quests().checkAndAdvance();
    finish();
}

void CombatSystem::applyControl(Monster& m) {
    GameState& st = game_.state();
    m.disp = Monster::Disposition::Controlled;
    st.controlCount++;
    pushLog(L"控制成功：" + m.name + L" 停止了攻击，地图标记为 R。", 11);
    pushLog(L"注意：控制不等于治愈。它仍然被污染驱动着。", 12);
    int ups = Player::addExp(st, m.exp);
    if (ups > 0) pushLog(L"等级提升！Lv." + std::to_wstring(st.level), 11);
    game_.quests().checkAndAdvance();
    finish();
}

void CombatSystem::monsterActs(Monster& m) {
    GameState& st = game_.state();
    int dmg = m.attack - st.defense + smallOffset();
    if (dmg < 1) dmg = 1;

    std::wstring note;
    if (m.chargable && rollChance(25)) {          // 冲撞
        dmg *= 2;
        note = L"冲撞！";
        // 玩家邻近大型障碍可削弱冲撞伤害
        Room* room = game_.world()->room(st.roomId);
        if (room) {
            auto nearObstacle = [&]() {
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx) {
                        int x = st.px + dx, y = st.py + dy;
                        if (room->inBounds(x, y) && room->tileAt(x, y).obstacleLarge) return true;
                    }
                return false;
            };
            if (nearObstacle()) {
                dmg = dmg / 2;
                note = L"冲撞被大型障碍拦下，伤害减半！";
            }
        } else note = L"冲撞！";
    }

    st.hp -= dmg;
    pushLog(m.name + L" 发动攻击：" + note + L" 你损失 " + std::to_wstring(dmg) + L" 点生命。", 12);
    if (st.hp * 100 < st.maxHp * 30) {
        pushLog(L"你的生命低于 30%！使用营养块恢复（[4] 或输入 use）。", 14);
    }
    if (st.hp <= 0) {
        st.hp = 0;
        playerDeath(m.name + (m.boss ? L" 完成了它的残酷表演" : L""));
    }
}

void CombatSystem::playerDeath(const std::wstring& cause) {
    GameState& st = game_.state();
    active_ = false;
    ++st.deaths;
    std::vector<std::wstring> notes;
    notes.push_back(L"你在战斗中倒下（" + cause + L"）。");
    notes.push_back(L"死亡不会清空任务与关键物品——已回到最近检查点（生命恢复 60%）。");
    notes.push_back(L"建议：先清理同区域的其他怪物，再用生物营养块保持生命高于 60% 再战。");
    game_.dieNow(notes);
}

void CombatSystem::finish() {
    active_ = false;
    log_.clear();
    GameState& st = game_.state();
    st.mode = GameMode::Exploration;
    game_.reqRender();
}

void CombatSystem::tick(uint64_t) {
    // 战斗内的自动提示直接在 handle 中触发；此处预留（保持每帧接口）
}
