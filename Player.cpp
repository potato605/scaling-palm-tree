// Player.cpp —— 玩家成长：经验曲线 / 升级奖励 / 状态栏
#include "Player.h"

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
        st.maxHp += 20;
        st.attack += 4;
        st.defense += 2;
        st.hp = st.maxHp;   // 升级完全恢复
        ups += 1;
    }
    return ups;
}

void Player::heal(GameState& st, int amount) {
    st.hp += amount;
    if (st.hp > st.maxHp) st.hp = st.maxHp;
}

std::wstring Player::hudText(const GameState& st) {
    wchar_t buf[128];
    swprintf_s(buf, L"Lv.%d  HP %d/%d  攻%d  防%d  经验%d/%d",
               st.level, st.hp, st.maxHp, st.attack, st.defense,
               st.exp, Player::expNeedFor(st.level) > st.exp ? st.exp : st.exp);
    std::wstring s = buf;
    int t = st.elapsedSeconds;
    wchar_t tm[64];
    swprintf_s(tm, L"  用时 %02d:%02d  死亡 %d", t / 60, t % 60, st.deaths);
    s += tm;
    return s;
}
