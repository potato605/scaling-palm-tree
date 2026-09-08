// Tile.h —— 房间地图单元
#pragma once

#include <string>

// 单个地图单元：字符、颜色、通行性、危险、交互引用
struct Tile {
    // 符号：'#'墙 '.'地面 '~'污染水 '*'碎玻璃等危险区
    wchar_t ch = L'.';
    int fg = 7;              // 默认灰
    bool passable = true;    // 玩家能否站上该格（墙体/终端/容器为 false）
    bool dangerous = false;  // 踩上是否受伤
    int dangerDmg = 0;       // 危险伤害
    int interactId = 0;      // 指向 Room::interactables 的下标+1；0 表示无交互
    bool obstacleLarge = false; // 大型障碍（削弱骸甲巨麋的冲撞伤害）
};
