// NPC.h —— 关键人物（林阅/方晴/赵诚/陈砚/苏瑾）
#pragma once

#include <string>
#include <vector>
#include "Entity.h"

struct NPC : public Entity {
    std::vector<std::wstring> firstLines;   // 首次对话（逐字播放）
    std::vector<std::wstring> repeatLines;  // 重复对话（只复述当前目标，最多给出当前主线）
    int quizIndex = -1;                     // 5 道生态题编号；-1 表示不答题
    std::wstring quizNote;                  // 答题前的一句话铺垫（自然嵌入）
    std::string rewardKey;                  // 无论对错都给予的关键物品 id
    std::string rewardKey2;                 // 第二件关键物品（可为空）
    std::wstring rewardMsg;                 // 给予关键物品的消息
    bool blocking = true;                   // 是否挡住行走格（全息影像设为 false）
    bool met = false;                       // 首次对话是否已发生
};
