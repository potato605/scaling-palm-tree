// CommandDispatcher.cpp —— 数字快捷与英文命令的统一入口
#include "CommandDispatcher.h"
#include "CommandParser.h"
#include "Game.h"

namespace {
    // 保留底层英文命令解析以兼容旧存档/调试路径；玩家主界面使用数字交互。
    bool combatVerb(const std::string& v) {
        return v == "attack" || v == "purify" || v == "control" || v == "use" || v == "guard";
    }
    bool exploreVerb(const std::string& v) {
        return v == "look" || v == "mission" || v == "help" ||
               v == "inventory" || v == "inv" || v == "status" || v == "inspect" ||
               v == "talk" || v == "open" || v == "read" || v == "take" ||
               v == "access" || v == "attack" || v == "purify" || v == "control" ||
               v == "use" || v == "save" || v == "load" || v == "quit";
    }
}

void CommandDispatcher::dispatch(const std::string& rawLine) {
    GameState& st = game_.state();
    ParsedCommand pc = CommandParser::parse(rawLine);

    // 中文命令拒绝（按需求文本）
    if (pc.chinese) {
        game_.post(L"本游戏不接受中文命令。\nEnglish commands only.\n\n你也可以靠近对象后直接按数字进行交互。", 12);
        return;
    }
    if (!pc.ok) {
        game_.post(pc.error, 12);
        return;
    }

    // 模式隔离
    switch (st.mode) {
        case GameMode::Narrative:
            game_.post(L"剧情播放中：按 Enter 补全当前行（长按也只有一行）。", 8);
            return;
        case GameMode::Selection:
        case GameMode::PauseOrConfirm:
            game_.post(L"请按菜单上的数字键选择。", 8);
            return;
        case GameMode::Title:
            game_.post(L"标题页请按 [1] 新游戏 / [2] 读档 / [3] 退出 / [4] 项目成员。", 8);
            return;
        case GameMode::Ending:
        case GameMode::GameOver:
            game_.post(L"当前画面不接受命令。", 8);
            return;
        case GameMode::Combat:
            if (!combatVerb(pc.verb)) {
                game_.post(L"战斗中使用 [1]攻击 [2]净化 [3]控制 [4]补给 [5]防御。", 8);
                return;
            }
            break;
        case GameMode::Exploration:
        case GameMode::CommandInput:
            if (!exploreVerb(pc.verb)) {
                game_.post(L"该命令当前不可用。输入 help 查看命令列表。", 8);
                return;
            }
            break;
    }

    // 统一业务路由
    const std::string& v = pc.verb;
    if (v == "look") game_.doLook(pc.args);
    else if (v == "mission") game_.doMission(pc.args);
    else if (v == "help") game_.doHelp(pc.args);
    else if (v == "inventory" || v == "inv") game_.doInventory(pc.args);
    else if (v == "status") game_.doStatus(pc.args);
    else if (v == "inspect") game_.doInspect(pc.args, L"查看");
    else if (v == "talk") game_.doTalk(pc.args);
    else if (v == "open") game_.doOpen(pc.args);
    else if (v == "read") game_.doRead(pc.args);
    else if (v == "take") game_.doTake(pc.args);
    else if (v == "access") game_.doAccess(pc.args);
    else if (v == "attack") game_.doAttack(pc.args);
    else if (v == "purify") game_.doPurify(pc.args);
    else if (v == "control") game_.doControl(pc.args);
    else if (v == "use") game_.doUse(pc.args);
    else if (v == "save") game_.doSave(pc.args);
    else if (v == "load") game_.doLoad(pc.args);
    else if (v == "quit") game_.doQuit(pc.args);
}
