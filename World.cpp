// World.cpp —— 房间查询/门锁/方向
#include "World.h"
#include "GameState.h"

Room* World::room(int id) {
    for (auto& r : rooms_) {
        if (r->id == id) return r.get();
    }
    return nullptr;
}

const Room* World::room(int id) const {
    for (const auto& r : rooms_) {
        if (r->id == id) return r.get();
    }
    return nullptr;
}

std::wstring World::roomName(int id) const {
    const Room* r = room(id);
    return r ? r->name : std::wstring(L"?");
}

void World::unlockDoor(const std::string& doorId) {
    for (auto& r : rooms_) {
        for (auto& it : r->interactables) {
            if (it.kind == Interactable::Kind::Door && it.doorId == doorId) {
                it.unlocked = true;
                r->tileAt(it.x, it.y).passable = true;
                r->tileAt(it.x, it.y).ch = L'D';      // 已解锁出口
                r->tileAt(it.x, it.y).fg = 10;        // 绿
            }
        }
    }
}

std::wstring World::dirToward(int from, int target) const {
    if (from == target) return L"当前房间";
    // 门网 BFS（这个线性设施中直接走门链）
    std::vector<int> queue = { from };
    std::vector<int> pre(rooms_.size(), -1);
    std::vector<std::wstring> label(rooms_.size(), L"");
    pre[from] = from;
    for (size_t i = 0; i < queue.size(); ++i) {
        const Room* r = room(queue[i]);
        if (!r) continue;
        for (const auto& it : r->interactables) {
            if (it.kind != Interactable::Kind::Door || it.targetRoom < 0) continue;
            if (pre[it.targetRoom] != -1) continue;
            pre[it.targetRoom] = queue[i];
            label[it.targetRoom] = it.dirLabel;
            queue.push_back(it.targetRoom);
            if (it.targetRoom == target) break;
        }
    }
    if (target < 0 || target >= (int)pre.size() || pre[target] == -1) return L"未知区域";
    return label[target];
}
