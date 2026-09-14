// Monster.h —— 普通怪物与 BOSS APF-X00「厄生」
#pragma once

#include <string>
#include "Entity.h"

struct Monster : public Entity {
    // 处置状态机
    enum class Disposition { Alive, Weakened, Purified, Controlled, Eliminated };

    int hp = 0, maxHp = 0;
    int attack = 0, defense = 0;
    int aggroRange = 2;      // 警戒范围（曼哈顿距离）
    int exp = 0;             // 处置后获得经验
    Disposition disp = Disposition::Alive;

    // BOSS 专属
    bool boss = false;                 // 是否厄生
    int bossPhase = 0;                 // 1外壳防御 2核心暴露 3意识崩解
    bool chargable = false;            // 骸甲巨麋：可能冲撞
    std::wstring flavor;               // 遭遇描述
    std::wstring weakMsg;              // 进入基因衰弱时的消息

    bool alive() const { return disp == Disposition::Alive; }
    bool weakened() const { return disp == Disposition::Weakened; }
    // 进入基因衰弱只是可处置状态，只有三种最终处置才算完成。
    bool treated() const {
        return disp == Disposition::Purified || disp == Disposition::Controlled ||
               disp == Disposition::Eliminated;
    }
    bool hpLow() const { return maxHp > 0 && hp * 100 <= maxHp * 35; }
};

// 处置状态 <-> 文本（存档与地图符号）
const wchar_t* dispositionGlyph(Monster::Disposition d);
const std::string dispositionName(Monster::Disposition d);
