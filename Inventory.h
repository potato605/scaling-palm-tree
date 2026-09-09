// Inventory.h —— 背包：关键物品与消耗品分离管理
#pragma once

#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>

class Inventory {
public:
    // 关键物品：不可丢弃、不出售、死亡不消失、不被普通消耗逻辑删除
    void addKey(const std::string& defId) { keys_.insert(defId); }
    bool hasKey(const std::string& defId) const { return keys_.count(defId) > 0; }

    // 普通物品（数量不为负）
    void addItem(const std::string& defId, int n = 1);
    // 消耗物品；关键物品与数量不足时返回 false
    bool consume(const std::string& defId, int n = 1);
    int count(const std::string& defId) const;

    // 文本描述（背包装备用）
    std::vector<std::wstring> describe() const;

    // 存档用：全部条目
    const std::unordered_set<std::string>& keys() const { return keys_; }
    const std::unordered_map<std::string, int>& items() const { return items_; }

private:
    std::unordered_set<std::string> keys_;
    std::unordered_map<std::string, int> items_;
};
