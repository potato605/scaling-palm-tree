// Player.cpp —— 玩家成长：经验曲线 / 升级奖励 / 状态栏
#include "Player.h"

#include <algorithm>

int Player::expNeedFor(int level) {
    switch (level) {
        case 1: return 60;
        case 2: return 150;
        case 3: return 300;
        default: return 300 + 300 * (level - 3);
    }
}

int Player::addExp(GameState& st, int gain) {
    if (gain <= 0) return 0;
    st.exp += gain;
    int ups = 0;
    while (st.level < 20 && st.exp >= expNeedFor(st.level)) {
        st.exp -= expNeedFor(st.level);
        st.level += 1;
        st.maxHp += 12;
        st.attack += 2;
        st.defense += 1;
        // 升级只提升上限与攻防，当前 HP 完全保留，避免战斗结束隐性回血。
        ups += 1;
    }
    return ups;
}

void Player::heal(GameState& st, int amount) {
    st.hp = std::min(st.hp + amount, st.maxHp);
}

std::wstring Player::hudText(const GameState& st) {
    wchar_t buf[128];
    swprintf_s(buf, L"[角色状态]  HP %d/%d   攻击 %d   防御 %d   经验 %d",
               st.hp, st.maxHp, st.attack, st.defense, st.exp);
    return buf;
}
