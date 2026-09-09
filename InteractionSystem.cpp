// InteractionSystem.cpp —— 附近交互菜单构建与排序
#include "InteractionSystem.h"
#include "World.h"
#include "Item.h"

#include <algorithm>

namespace {
    struct Cand {
        int priority = 999;
        ContextAction act;
    };

    // 窄串 → 宽串（id 均为 ASCII）
    std::wstring wstrx(const std::string& s) {
        return std::wstring(s.begin(), s.end());
    }

    std::wstring itemNameOf(const std::string& id) {
        const ItemDef* d = findItemDef(id);
        return d ? d->name : std::wstring(L"?");
    }
}

std::vector<ContextAction> InteractionSystem::build(GameState& st, World& world,
                                                    const std::optional<std::string>& objectiveId) {
    std::vector<ContextAction> out;
    Room* room = world.room(st.roomId);
    if (!room) return out;

    auto near = [&](int x, int y) {
        int dx = x - st.px, dy = y - st.py;
        if (dx < 0) dx = -dx;
        if (dy < 0) dy = -dy;
        return (dx + dy) <= 1;   // 曼哈顿距离 1
    };
    auto isObj = [&](const std::string& id) {
        return objectiveId && *objectiveId == id;
    };
    int baseForObj = 10;   // 主线目标最优先

    // ---------- NPC ----------
    for (auto& npc : room->npcs) {
        if (!near(npc->x, npc->y)) continue;
        bool done = st.hasFlag("talked_" + npc->id);
        ContextAction a;
        a.priority = done ? 90 : (isObj(npc->id) ? baseForObj : 20);
        a.chineseLabel = L"与" + npc->name + L"交谈";
        a.englishLabel = std::wstring(L"talk ") + wstrx(npc->id);
        a.effectDescription = done ? L"复述当前目标" : L"首次交谈：剧情与生态题（奖励固定获得）";
        a.commandText = "talk " + npc->id;
        a.enabled = !done;
        a.unavailableReason = done ? L"已交谈过" : L"";
        out.push_back(a);
    }

    // ---------- 门 ----------
    for (auto& it : room->interactables) {
        if (it.kind != Interactable::Kind::Door) continue;
        if (!near(it.x, it.y)) continue;
        ContextAction a;
        a.priority = isObj(it.doorId) ? baseForObj : 30;
        a.chineseLabel = it.unlocked ? L"进入" + world.roomName(it.targetRoom)
                                     : L"打开防护门（" + it.dirLabel + L"）";
        a.englishLabel = std::wstring(L"open ") + wstrx(it.id);
        a.effectDescription = it.unlocked ? L"前往设施深处" : L"需要" + itemNameOf(it.requiredKey);
        a.commandText = "open " + it.id;
        a.enabled = it.unlocked || (!it.requiredKey.empty() && st.hasKey(it.requiredKey));
        a.unavailableReason = it.unlocked ? L"" : L"需要" + itemNameOf(it.requiredKey);
        out.push_back(a);
    }

    // ---------- 怪物（敌人） ----------
    for (auto& m : room->monsters) {
        if (!near(m->x, m->y)) continue;
        if (m->disp == Monster::Disposition::Purified || m->disp == Monster::Disposition::Controlled ||
            m->disp == Monster::Disposition::Eliminated) {
            ContextAction a;
            a.priority = 95;
            a.chineseLabel = L"（已处置）" + m->name;
            a.englishLabel = std::wstring(L"inspect ") + wstrx(m->id);
            a.effectDescription = L"该目标已不再构成威胁";
            a.commandText = "inspect " + m->id;
            a.enabled = false;
            a.unavailableReason = L"已处置";
            out.push_back(a);
            continue;
        }
        // 交战
        {
            ContextAction a;
            a.priority = isObj(m->id) ? baseForObj : 40;
            a.chineseLabel = m->boss ? L"与" + m->name + L"对峙！（BOSS）" : L"与" + m->name + L"交战";
            a.englishLabel = std::wstring(L"attack ") + wstrx(m->id);
            a.effectDescription = m->boss ? L"三阶段战斗 → 最终处置" : L"战斗：攻击/净化/控制";
            a.commandText = "attack " + m->id;
            a.enabled = true;
            out.push_back(a);
        }
        if (!m->boss) {
            {
                ContextAction a;
                a.priority = 45;
                a.chineseLabel = L"净化 " + m->name;
                a.englishLabel = std::wstring(L"purify ") + wstrx(m->id);
                a.effectDescription = L"停止攻击、地图标记 +";
                a.commandText = "purify " + m->id;
                a.enabled = m->weakened();
                a.unavailableReason = m->weakened() ? L"" : L"需攻击至基因衰弱（HP<35%）";
                out.push_back(a);
            }
            {
                ContextAction a;
                a.priority = 46;
                a.chineseLabel = L"控制 " + m->name;
                a.englishLabel = std::wstring(L"control ") + wstrx(m->id);
                a.effectDescription = L"神经协议操纵、地图标记 R（控制≠治愈）";
                a.commandText = "control " + m->id;
                a.enabled = st.hasKey("neural_protocol") && m->weakened();
                a.unavailableReason = !st.hasKey("neural_protocol") ? L"缺少神经控制协议"
                                    : L"需先基因衰弱（HP<35%）";
                out.push_back(a);
            }
        }
    }

    // ---------- 其他交互物（终端/容器/档案/地标） ----------
    for (auto& it : room->interactables) {
        if (it.kind == Interactable::Kind::Door) continue;
        if (!near(it.x, it.y)) continue;
        ContextAction a;
        switch (it.kind) {
            case Interactable::Kind::Container: {
                a.priority = it.opened ? 93 : 70;
                a.chineseLabel = L"打开" + it.name;
                a.englishLabel = std::wstring(L"open ") + wstrx(it.id);
                a.effectDescription = it.opened ? L"已搜空" : L"获得" + itemNameOf(it.itemId);
                a.commandText = "open " + it.id;
                a.enabled = !it.opened;
                a.unavailableReason = it.opened ? L"已空" : L"";
                out.push_back(a);
                break;
            }
            case Interactable::Kind::Terminal: {
                a.priority = 55;
                a.chineseLabel = L"访问" + it.name;
                a.englishLabel = std::wstring(L"access ") + wstrx(it.id);
                a.effectDescription = L"终端：记录与提示";
                a.commandText = "access " + it.id;
                a.enabled = true;
                out.push_back(a);
                break;
            }
            case Interactable::Kind::Archive: {
                a.priority = 55;
                a.chineseLabel = L"阅读" + it.name;
                a.englishLabel = std::wstring(L"read ") + wstrx(it.id);
                a.effectDescription = L"档案：计入档案数（可选）";
                a.commandText = "read " + it.id;
                a.enabled = true;
                out.push_back(a);
                break;
            }
            case Interactable::Kind::Landmark: {
                a.priority = 55;
                a.chineseLabel = L"查看" + it.name;
                a.englishLabel = std::wstring(L"inspect ") + wstrx(it.id);
                a.effectDescription = L"现场痕迹";
                a.commandText = "inspect " + it.id;
                a.enabled = true;
                out.push_back(a);
                break;
            }
            case Interactable::Kind::GroundItem: {
                const ItemDef* def = findItemDef(it.itemId);
                a.priority = 50;
                a.chineseLabel = L"拾取" + it.name;
                a.englishLabel = std::wstring(L"take ") + wstrx(it.id);
                a.effectDescription = def && isKeyItemKind(def->kind) ? L"固定取得的污染样本/关键物品"
                                                                      : L"补给物品";
                a.commandText = "take " + it.id;
                a.enabled = !it.opened;
                a.unavailableReason = it.opened ? L"已拾取" : L"";
                out.push_back(a);
                break;
            }
            case Interactable::Kind::Door: break;
        }
    }

    // ---------- 排序 + 编号 ----------
    std::stable_sort(out.begin(), out.end(), [](const ContextAction& x, const ContextAction& y) {
        return x.priority < y.priority;
    });
    for (size_t i = 0; i < out.size(); ++i) {
        out[i].number = (int)i + 1;
    }
    return out;
}
