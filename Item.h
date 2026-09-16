// Item.h —— 物品定义（关键物品 / 样本 / 消耗品）
#pragma once

#include <string>
#include <vector>

struct ItemDef {
    enum Kind { Key, Sample, Consumable };
    std::string id;
    std::wstring name;
    std::wstring desc;
    Kind kind = Consumable;
    int heal = 0;  // 消耗品回复的生命值
};

// 全物品表（GameData 提供）
extern const std::vector<ItemDef>& allItemDefs();

// 按 id 查找；找不到返回 nullptr
const ItemDef* findItemDef(const std::string& id);

// 关键物品（Key/Sample）：不可丢弃、死亡不丢失、不能被普通消耗逻辑删除
inline bool isKeyItemKind(ItemDef::Kind k) {
    return k == ItemDef::Key || k == ItemDef::Sample;
}
