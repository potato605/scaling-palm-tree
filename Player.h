// Player.h —— 玩家属性成长：经验 / 升级 / 恢复
#pragma once

#include "GameState.h"

namespace Player {
    // 升级所需经验（累计）
    int expNeedFor(int level);

    // 增加经验并处理升级，返回升级次数（附带消息由调用方处理）
    int addExp(GameState& st, int gain);

    // 使用恢复道具
    void heal(GameState& st, int amount);

    // 状态栏文本
    std::wstring hudText(const GameState& st);
}
