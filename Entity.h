// Entity.h —— 地图实体基类（稳定 ID + 名称 + 位置）
#pragma once

#include <string>

struct Entity {
    std::string id;      // 稳定的英文 id（命令行目标名）
    std::wstring name;   // 中文显示名
    int x = 0, y = 0;    // 在房间网格中的坐标

    // 曼哈顿距离
    int distTo(int tx, int ty) const {
        int dx = tx - x, dy = ty - y;
        return (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
    }
};
