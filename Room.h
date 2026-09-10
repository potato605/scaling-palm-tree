// Room.h —— 独立房间：地图、NPC、怪物、交互物、门
#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <utility>

#include "Tile.h"
#include "NPC.h"
#include "Monster.h"

struct GameState;

// 房间内的固定交互物（终端/容器/门/档案/地标/地面物品）
struct Interactable {
    enum class Kind { Door, Terminal, Container, Archive, Landmark, GroundItem };

    std::string id;            // 房间内唯一 id（命令目标名）
    std::wstring name;         // 中文名称
    int x = 0, y = 0;
    Kind kind = Kind::Archive;

    // Door 专属
    std::string doorId;        // 两扇门共享的全局门 id（如 g_0_1）
    std::string requiredKey;   // 开锁所需关键物品（空=无需钥匙）
    int targetRoom = -1;       // 通向的房间
    std::wstring dirLabel;     // 方位文字（东侧/西侧…，宽字符串）

    // Container 专属
    std::string itemId;        // 内容物品
    int itemCount = 0;

    // Archive/Landmark 专属
    std::vector<std::wstring> archiveLines;

    bool opened = false;       // 容器已打开
    bool unlocked = false;     // 门已解锁
    bool hidden = false;       // 隐藏（不参与交互，仅用于门锁标记等）
};

class Room {
public:
    Room(int i, const std::wstring& n);

    int id = 0;
    std::wstring name;
    std::vector<std::vector<Tile>> grid;
    int width = 0, height = 0;

    std::vector<std::unique_ptr<NPC>> npcs;          // Room 用 unique_ptr 拥有 NPC
    std::vector<std::unique_ptr<Monster>> monsters;   // Room 用 unique_ptr 拥有怪物
    std::vector<Interactable> interactables;

    std::vector<std::wstring> introLines;             // 首次进入的房间描写（逐字播放）

    // ---------- 网格 ----------
    bool inBounds(int x, int y) const;
    const Tile& tileAt(int x, int y) const;
    Tile& tileAt(int x, int y);

    // 实体查询
    NPC* npcAt(int x, int y);
    Monster* monsterAt(int x, int y);
    Interactable* interactableAt(int x, int y);
    const NPC* npcAt(int x, int y) const;
    const Monster* monsterAt(int x, int y) const;

    // 阻挡判定：怪物尸体/已开门/容器/终端等
    // canEnter: 玩家能否走进去（含门锁、实体、危险判断）
    bool canEnterTile(int x, int y, const GameState& st) const;
    // 为何不能进入（给玩家看的短句；可进入时返回空）
    std::wstring enterBlockReason(int x, int y, const GameState& st) const;
    // 踩上去会受伤吗
    bool tileDangerous(int x, int y) const;

    // 渲染底图（动态叠加前），已处理状态变化的颜色
    wchar_t glyphAt(int x, int y) const;
    int glyphColor(int x, int y) const;
};
