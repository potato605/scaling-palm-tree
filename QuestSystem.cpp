// QuestSystem.cpp —— 主线阶段推进 / mission 全文 / 一致性校验
#include "QuestSystem.h"
#include "Game.h"
#include "Item.h"

namespace {
    // 阶段信息表：下标 1~12（0 为占位）
    const QuestSystem::StageInfo kStages[13] = {
        { L"", L"", 0, L"", L"", L"" },
        { L"进入生态禁区",
          L"在入口安全厅与林阅交谈（地图 N 处，按数字或输入 talk linyue），回答她的一道基础问题。",
          0, L"talk linyue", L"N",
          L"第一步：入口安全厅。走到 N（林阅）旁边，靠近后按数字 1，或输入 talk linyue。答完题必定获得【温室门禁卡】，补给箱里有生物营养块。" },
        { L"清理温室前廊",
          L"处置温室前廊的双头煞一（靠近触发战斗：攻击到基因衰弱后再净化，或直接肃清）。",
          1, L"attack", L"M",
          L"第二步：从东侧门进入温室前廊。靠近 M（双头煞一）触发战斗：[1]攻击直到它 HP 低于 35%，再用 [2]净化；或直接 [1]攻击到 0。避开 ~ 污染水。" },
        { L"清剿诱变培育舱",
          L"处置双头煞二，并取得污染样本一（S）。",
          2, L"take sample_1", L"S",
          L"第三步：诱变培育舱。先靠近 M 战斗处置双头煞二，再拾取 S（污染样本一），顺路可以读取培养日志。" },
        { L"取得实验层权限",
          L"在温室控制室与方晴交谈，取得实验层权限（解锁隔离走廊）。",
          3, L"talk fangqing", L"N",
          L"第四步：温室控制室，与 N（方晴）交谈答题，获得【实验层权限】，隔离走廊的门自动解锁。" },
        { L"穿越隔离走廊",
          L"处置隔离走廊的裂脊獾一（警戒范围小；长按移动会在靠近时被战斗打断）。",
          4, L"attack", L"M",
          L"第五步：进入隔离走廊。靠近 M（裂脊獾一）触发战斗。爪痕与血迹可调查。" },
        { L"清剿活体实验舱",
          L"处置裂脊獾二，取得污染样本二（S），注意碎玻璃（*）危险区。",
          5, L"take sample_2", L"S",
          L"第六步：活体实验舱。战斗处置裂脊獾二，拾取 S（样本二），避开 * 碎玻璃区。" },
        { L"取得饲养场钥匙",
          L"在安保值班室与赵诚交谈，取得饲养场钥匙与神经控制协议。",
          6, L"talk zhaocheng", L"N",
          L"第七步：安保值班室。与 N（赵诚）交谈答题，获得【饲养场钥匙】和【神经控制协议】——控制怪物靠它。" },
        { L"通过饲养场入口",
          L"处置骸甲巨麋一（小心冲撞；躲在大型障碍附近可削弱伤害）。",
          7, L"attack", L"M",
          L"第八步：饲养场入口，处置 M（骸甲巨麋一）。它可能冲撞，站在障碍物（#）旁伤害减半。" },
        { L"修复破损围栏区",
          L"处置骸甲巨麋二，取得污染样本三（S）。",
          8, L"take sample_3", L"S",
          L"第九步：破损围栏区。处置 M（骸甲巨麋二），拾取 S（样本三）。控制终端可查看状态。" },
        { L"取得中央权限",
          L"在生态观察室与陈砚交谈，取得逆转录净化剂与中央权限。",
          9, L"talk chenyan", L"N",
          L"第十步：生态观察室。与 N（陈砚）交谈答题，获得【逆转录净化剂】（净化厄生必需）与【中央权限】。" },
        { L"观看苏瑾记录",
          L"在中央控制室前厅观看苏瑾全息影像，了解厄生三阶段与三种处置条件（BOSS 前最后补给）。",
          10, L"talk sujin", L"N",
          L"第十一步：中央控制室前厅。先拿最终补给（C），再与苏瑾全息影像（N）交谈。" },
        { L"终结厄生",
          L"进入嵌合核心室，击败 APF-X00「厄生」，在最终处置菜单选择：肃清 / 净化 / 控制。",
          11, L"attack eps", L"X",
          L"第十二步：核心室。攻击 X 打破外壳（一）→ 核心暴露（二）→ 意识崩解（三）→ 最终处置 [1]肃清 [2]净化 [3]控制，二次确认。" },
    };
}

GameState& QuestSystem::st() const { return game_.state(); }

const QuestSystem::StageInfo& QuestSystem::stageInfo(int n) {
    if (n < 1) n = 1;
    if (n > 12) n = 12;
    return kStages[n];
}

const QuestSystem::StageInfo& QuestSystem::stage() const {
    return stageInfo(game_.state().questStage);
}

int QuestSystem::stageNum() const { return game_.state().questStage; }

// ------- 推进条件（与存档一致性校验同表） -------
void QuestSystem::checkAndAdvance() {
    GameState& st = game_.state();
    World* world = game_.world();
    if (!world) return;

    auto monsterTreated = [&](const std::string& id) -> bool {
        for (auto& r : world->rooms()) {
            for (auto& m : r->monsters) {
                if (m->id == id) return m->treated();
            }
        }
        return false;
    };

    bool advanced = false;
    for (;;) {
        int s = st.questStage;
        bool done = false;
        switch (s) {
            case 1: done = st.hasFlag("talked_linyue"); break;
            case 2: done = monsterTreated("shuang_1"); break;
            case 3: done = monsterTreated("shuang_2") && st.hasKey("sample_1"); break;
            case 4: done = st.hasFlag("talked_fangqing"); break;
            case 5: done = monsterTreated("lieji_1"); break;
            case 6: done = monsterTreated("lieji_2") && st.hasKey("sample_2"); break;
            case 7: done = st.hasFlag("talked_zhaocheng"); break;
            case 8: done = monsterTreated("haijia_1"); break;
            case 9: done = monsterTreated("haijia_2") && st.hasKey("sample_3"); break;
            case 10: done = st.hasFlag("talked_chenyan"); break;
            case 11: done = st.hasFlag("talked_sujin"); break;
            default: done = true;
        }
        if (s >= 12 || !done) break;
        st.questStage = s + 1;
        advanced = true;
        game_.post(L"【新目标】" + stageInfo(st.questStage).next, 11);
    }
    if (advanced) game_.reqRender();
}

// 当前主线目标实体 id（地图 '!' 标记）
std::optional<std::string> QuestSystem::objectiveEntity() const {
    const GameState& st = game_.state();
    switch (st.questStage) {
        case 1: return "linyue";
        case 2: return "shuang_1";
        case 3: return "shuang_2";
        case 4: return "fangqing";
        case 5: return "lieji_1";
        case 6: return "lieji_2";
        case 7: return "zhaocheng";
        case 8: return "haijia_1";
        case 9: return "haijia_2";
        case 10: return "chenyan";
        case 11: return "sujin";
        case 12: return "eps";
        default: return std::nullopt;
    }
}

std::wstring QuestSystem::directionText() const {
    const GameState& st = game_.state();
    int target = stageInfo(st.questStage).targetRoom;
    World* w = game_.world();
    return w ? w->dirToward(st.roomId, target) : L"?";
}

// mission 全文
std::wstring QuestSystem::missionText() const {
    const GameState& st = game_.state();
    std::wstring out;
    out += L"当前主线：" + stage().name + L"\n";
    out += L"下一步：" + stage().next + L"\n";
    out += L"目标方向：" + directionText() + L"\n";
    out += L"推荐操作：靠近目标后按数字，或输入 " +
           std::wstring(stage().keyCmd.begin(), stage().keyCmd.end()) + L"\n";
    out += L"\n已完成：\n";
    if (st.questStage <= 1) out += L"（序章完成，尚未推进任务）\n";
    else {
        for (int i = 1; i < st.questStage; ++i) {
            if (i > 1) out += L"、";
            out += stageInfo(i).name;
        }
        out += L"\n";
    }
    out += L"处置统计：肃清 " + std::to_wstring(st.eliminateCount) +
           L" · 净化 " + std::to_wstring(st.purifyCount) +
           L" · 控制 " + std::to_wstring(st.controlCount) +
           L"  当前目标符号：" + std::wstring(stage().targetSymbol.begin(), stage().targetSymbol.end());
    return out;
}

// ------- 一致性校验（读档） -------
std::wstring QuestSystem::verifyConsistency(const GameState& st) const {
    auto need = [&](const std::string& id, const std::wstring& what) -> std::wstring {
        const ItemDef* def = findItemDef(id);
        return std::wstring(L"存档损坏：任务阶段 ") + what + L" 需要 " +
               (def ? def->name : std::wstring(L"(未知物品)")) + L"。";
    };
    int s = st.questStage;
    if (s >= 2 && !st.hasKey("greenhouse_card")) return need("greenhouse_card", L"≥2");
    if (s >= 4 && !st.hasKey("lab_permit")) return need("lab_permit", L"≥4");
    if (s >= 4 && !st.hasKey("sample_1")) return need("sample_1", L"≥4");
    if (s >= 7 && !st.hasKey("farm_key")) return need("farm_key", L"≥7");
    if (s >= 7 && !st.hasKey("neural_protocol")) return need("neural_protocol", L"≥7");
    if (s >= 7 && !st.hasKey("sample_2")) return need("sample_2", L"≥7");
    if (s >= 10 && !st.hasKey("core_permit")) return need("core_permit", L"≥10");
    if (s >= 10 && !st.hasKey("retrovirus")) return need("retrovirus", L"≥10");
    if (s >= 10 && !st.hasKey("sample_3")) return need("sample_3", L"≥10");
    return std::wstring();
}
