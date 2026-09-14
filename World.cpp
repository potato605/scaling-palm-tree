// World.cpp —— 房间查询/门锁/方向
#include "World.h"

#include <algorithm>
#include <queue>

Room* World::room(int id) {
    auto it = std::find_if(rooms_.begin(), rooms_.end(),
                           [id](const auto& r) { return r->id == id; });
    return it == rooms_.end() ? nullptr : it->get();
}

const Room* World::room(int id) const {
    auto it = std::find_if(rooms_.begin(), rooms_.end(),
                           [id](const auto& r) { return r->id == id; });
    return it == rooms_.end() ? nullptr : it->get();
}

std::wstring World::roomName(int id) const {
    const Room* r = room(id);
    return r ? r->name : std::wstring(L"?");
}

Monster* World::findMonster(const std::string& id) {
    for (auto& r : rooms_) {
        if (Monster* m = r->monsterById(id)) return m;
    }
    return nullptr;
}

bool World::monsterTreated(const std::string& id) {
    const Monster* m = findMonster(id);
    return m && m->treated();
}

void World::unlockDoor(const std::string& doorId) {
    for (auto& r : rooms_) {
        for (auto& it : r->interactables) {
            if (it.kind == Interactable::Kind::Door && it.doorId == doorId) {
                it.unlocked = true;
                Tile& t = r->tileAt(it.x, it.y);
                t.passable = true;
                t.ch = L'D';      // 已解锁出口
                t.fg = 10;        // 绿
            }
        }
    }
}

std::wstring World::dirToward(int from, int target) const {
    if (from == target) return L"当前房间";
    // 门网 BFS（这个线性设施中直接走门链）
    std::vector<int> parent(rooms_.size(), -1);
    std::vector<std::wstring> label(rooms_.size());
    parent[from] = from;
    std::queue<int> q;
    q.push(from);
    while (!q.empty()) {
        const int cur = q.front();
        q.pop();
        const Room* r = room(cur);
        if (!r) continue;
        for (const auto& it : r->interactables) {
            if (it.kind != Interactable::Kind::Door || it.targetRoom < 0) continue;
            if (parent[it.targetRoom] != -1) continue;
            parent[it.targetRoom] = cur;
            label[it.targetRoom] = it.dirLabel;
            if (it.targetRoom == target) return label[target];
            q.push(it.targetRoom);
        }
    }
    if (target < 0 || target >= (int)parent.size() || parent[target] == -1) return L"未知区域";
    return label[target];
}
