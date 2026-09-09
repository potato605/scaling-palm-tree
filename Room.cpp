// Room.cpp —— 房间网格查询与通行判定
#include "Room.h"
#include "GameState.h"
#include "Item.h"

Room::Room(int i, const std::wstring& n) : id(i), name(n) {}

bool Room::inBounds(int x, int y) const {
    return x >= 0 && y >= 0 && x < width && y < height;
}

const Tile& Room::tileAt(int x, int y) const {
    static Tile fallback;   // 越界返回默认（安全返回）
    if (!inBounds(x, y)) return fallback;
    return grid[y][x];
}

Tile& Room::tileAt(int x, int y) {
    static Tile fallback;
    if (!inBounds(x, y)) return fallback;
    return grid[y][x];
}

NPC* Room::npcAt(int x, int y) {
    for (auto& npc : npcs) {
        if (npc->x == x && npc->y == y) return npc.get();
    }
    return nullptr;
}

Monster* Room::monsterAt(int x, int y) {
    for (auto& m : monsters) {
        if (m->x == x && m->y == y) return m.get();
    }
    return nullptr;
}

Interactable* Room::interactableAt(int x, int y) {
    for (auto& it : interactables) {
        if (!it.hidden && it.x == x && it.y == y) return &it;
    }
    return nullptr;
}

const NPC* Room::npcAt(int x, int y) const {
    for (const auto& npc : npcs) {
        if (npc->x == x && npc->y == y) return npc.get();
    }
    return nullptr;
}

const Monster* Room::monsterAt(int x, int y) const {
    for (const auto& m : monsters) {
        if (m->x == x && m->y == y) return m.get();
    }
    return nullptr;
}

bool Room::canEnterTile(int x, int y, const GameState& st) const {
    if (!inBounds(x, y)) return false;
    const Tile& t = tileAt(x, y);
    if (!t.passable) return false;

    // 门：未解锁不可通过
    if (t.ch == L'L' || t.ch == L'D') {
        for (const auto& it : interactables) {
            if (it.kind == Interactable::Kind::Door && it.x == x && it.y == y && !it.unlocked)
                return false;
        }
    }
    return true;
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
        for (const auto& it : interactables) {
            if (it.kind == Interactable::Kind::Door && it.x == x && it.y == y) {
                if (it.unlocked) break;
                const ItemDef* def = it.requiredKey.empty() ? nullptr : findItemDef(it.requiredKey);
                if (def && !st.hasKey(it.requiredKey))
                    return std::wstring(L"门锁着，需要") + def->name;
                return L"门锁着";
            }
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

wchar_t Room::glyphAt(int x, int y) const {
    return tileAt(x, y).ch;
}

int Room::glyphColor(int x, int y) const {
    return tileAt(x, y).fg;
}
