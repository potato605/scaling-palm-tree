// Room.cpp —— 房间网格查询与通行判定
#include "Room.h"

#include <algorithm>

#include "GameState.h"
#include "Item.h"

Room::Room(int i, const std::wstring& n) : id(i), name(n) {}

bool Room::inBounds(int x, int y) const {
    return x >= 0 && y >= 0 && x < width && y < height;
}

const Tile& Room::tileAt(int x, int y) const {
    static const Tile fallback;   // 越界返回默认（安全返回）
    return inBounds(x, y) ? grid[y][x] : fallback;
}

Tile& Room::tileAt(int x, int y) {
    static Tile fallback;
    return inBounds(x, y) ? grid[y][x] : fallback;
}

NPC* Room::npcAt(int x, int y) {
    auto it = std::find_if(npcs.begin(), npcs.end(),
                           [x, y](const auto& n) { return n->x == x && n->y == y; });
    return it == npcs.end() ? nullptr : it->get();
}

const NPC* Room::npcAt(int x, int y) const {
    auto it = std::find_if(npcs.begin(), npcs.end(),
                           [x, y](const auto& n) { return n->x == x && n->y == y; });
    return it == npcs.end() ? nullptr : it->get();
}

Monster* Room::monsterAt(int x, int y) {
    auto it = std::find_if(monsters.begin(), monsters.end(),
                           [x, y](const auto& m) { return m->x == x && m->y == y; });
    return it == monsters.end() ? nullptr : it->get();
}

const Monster* Room::monsterAt(int x, int y) const {
    auto it = std::find_if(monsters.begin(), monsters.end(),
                           [x, y](const auto& m) { return m->x == x && m->y == y; });
    return it == monsters.end() ? nullptr : it->get();
}

Interactable* Room::interactableAt(int x, int y) {
    auto it = std::find_if(interactables.begin(), interactables.end(),
                           [x, y](const Interactable& e) { return !e.hidden && e.x == x && e.y == y; });
    return it == interactables.end() ? nullptr : &(*it);
}

NPC* Room::npcById(const std::string& id) {
    auto it = std::find_if(npcs.begin(), npcs.end(),
                           [&](const auto& n) { return n->id == id; });
    return it == npcs.end() ? nullptr : it->get();
}

Monster* Room::monsterById(const std::string& id) {
    auto it = std::find_if(monsters.begin(), monsters.end(),
                           [&](const auto& m) { return m->id == id; });
    return it == monsters.end() ? nullptr : it->get();
}

Interactable* Room::interactableById(const std::string& id) {
    auto it = std::find_if(interactables.begin(), interactables.end(),
                           [&](const Interactable& e) { return e.id == id; });
    return it == interactables.end() ? nullptr : &(*it);
}

std::wstring Room::enterBlockReason(int x, int y, const GameState& st) const {
    if (!inBounds(x, y)) return L"";
    const Tile& t = tileAt(x, y);
    if (!t.passable) {
        if (t.ch == L'#') return L"墙壁";
        if (t.ch == L'T') return L"终端机";
        if (t.ch == L'C') return L"容器";
        return L"障碍";
    }
    if (t.ch == L'L' || t.ch == L'D') {
        // 门：已解锁则可通过；未解锁给出钥匙原因
        auto it = std::find_if(interactables.begin(), interactables.end(),
                               [x, y](const Interactable& e) {
                                   return e.kind == Interactable::Kind::Door && e.x == x && e.y == y;
                               });
        if (it != interactables.end() && !it->unlocked) {
            const ItemDef* def = it->requiredKey.empty() ? nullptr : findItemDef(it->requiredKey);
            if (def && !st.hasKey(it->requiredKey))
                return std::wstring(L"门锁着，需要") + def->name;
            return L"门锁着";
        }
        return t.ch == L'L' ? L"门锁着" : L"";
    }
    // 实体阻挡
    if (npcAt(x, y)) return L"有人挡着";
    const Monster* m = monsterAt(x, y);
    if (m && m->alive()) return L"前方有怪物";
    return L"";
}

bool Room::tileDangerous(int x, int y) const {
    const Tile& t = tileAt(x, y);
    return t.dangerous && t.dangerDmg > 0;
}
