// Inventory.cpp —— 背包管理
#include "Inventory.h"
#include "Item.h"

void Inventory::addItem(const std::string& defId, int n) {
    const ItemDef* def = findItemDef(defId);
    if (!def || n < 0) return;
    if (isKeyItemKind(def->kind)) { keys_.insert(defId); return; } // 关键物品走 keys
    items_[defId] += n;
}

bool Inventory::consume(const std::string& defId, int n) {
    const ItemDef* def = findItemDef(defId);
    if (!def || n < 0) return false;
    if (isKeyItemKind(def->kind)) return false;     // 关键物品受保护
    auto it = items_.find(defId);
    if (it == items_.end() || it->second < n) return false;
    it->second -= n;
    if (it->second <= 0) items_.erase(it);
    return true;
}

int Inventory::count(const std::string& defId) const {
    const ItemDef* def = findItemDef(defId);
    if (def && isKeyItemKind(def->kind)) return keys_.count(defId) ? 1 : 0;
    auto it = items_.find(defId);
    return it == items_.end() ? 0 : it->second;
}

std::vector<std::wstring> Inventory::describe() const {
    std::vector<std::wstring> out;
    if (keys_.empty() && items_.empty()) {
        out.push_back(L"（空）");
        return out;
    }
    for (const ItemDef& def : allItemDefs()) {
        int c = count(def.id);
        if (c <= 0) continue;
        std::wstring line = def.name;
        if (def.kind == ItemDef::Consumable) {
            wchar_t buf[32];
            swprintf_s(buf, L" ×%d", c);
            line += buf;
        }
        out.push_back(line);
    }
    return out;
}
