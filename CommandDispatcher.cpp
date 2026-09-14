// CommandDispatcher.cpp —— 动词注册 + 模式隔离 + 命令查表执行（命令模式）
#include "CommandDispatcher.h"

#include <unordered_set>

#include "CommandParser.h"
#include "Game.h"

namespace {

// 各模式允许的动词集合（与 CommandParser 的已知动词表对应）
const std::unordered_set<std::string>& combatVerbs() {
    static const std::unordered_set<std::string> v = {
        "attack", "purify", "control", "use", "guard",
    };
    return v;
}

const std::unordered_set<std::string>& exploreVerbs() {
    static const std::unordered_set<std::string> v = {
        "look", "mission", "help", "inventory", "inv", "status", "inspect",
        "talk", "open", "read", "take", "access", "attack", "purify",
        "control", "use", "save", "load", "quit",
    };
    return v;
}

}  // namespace

CommandDispatcher::CommandDispatcher(Game& game) : game_(game) {
    // 注册 动词 -> 命令对象（同义动词映射到同一命令）
    registry_.add("look", make_command<&Game::doLook>());
    registry_.add("mission", make_command<&Game::doMission>());
    registry_.add("help", make_command<&Game::doHelp>());
    registry_.add("inventory", make_command<&Game::doInventory>());
    registry_.add("inv", make_command<&Game::doInventory>());
    registry_.add("status", make_command<&Game::doStatus>());
    registry_.add("inspect", make_command<&Game::doInspect>());
    registry_.add("talk", make_command<&Game::doTalk>());
    registry_.add("open", make_command<&Game::doOpen>());
    registry_.add("read", make_command<&Game::doRead>());
    registry_.add("take", make_command<&Game::doTake>());
    registry_.add("access", make_command<&Game::doAccess>());
    registry_.add("attack", make_command<&Game::doAttack>());
    registry_.add("purify", make_command<&Game::doPurify>());
    registry_.add("control", make_command<&Game::doControl>());
    registry_.add("use", make_command<&Game::doUse>());
    registry_.add("save", make_command<&Game::doSave>());
    registry_.add("load", make_command<&Game::doLoad>());
    registry_.add("quit", make_command<&Game::doQuit>());
    // 注："guard"（[5] 防御）在战斗模式通过门控检查后，历史上未接入任何业务
    // 分支（静默忽略）。为保持既有玩法不变，此处不注册——若有需求再补 doGuard。
}

void CommandDispatcher::dispatch(const std::string& rawLine) {
    GameState& st = game_.state();
    const ParsedCommand pc = CommandParser::parse(rawLine);

    if (pc.chinese) {
        game_.post(L"本游戏不接受中文命令。\nEnglish commands only.\n\n你也可以靠近对象后直接按数字进行交互。", 12);
        return;
    }
    if (!pc.ok) {
        game_.post(pc.error, 12);
        return;
    }

    // 模式隔离：各模式只接受允许的动词
    switch (st.mode) {
        case GameMode::Narrative:
            game_.post(L"剧情播放中：按 Enter 补全当前行（长按也只有一行）。", 8);
            return;
        case GameMode::Selection:
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
            if (!combatVerbs().count(pc.verb)) {
                game_.post(L"战斗中使用 [1]攻击 [2]净化 [3]控制 [4]补给 [5]防御。", 8);
                return;
            }
            break;
        case GameMode::Exploration:
        case GameMode::CommandInput:
            if (!exploreVerbs().count(pc.verb)) {
                game_.post(L"该命令当前不可用。输入 help 查看命令列表。", 8);
                return;
            }
            break;
    }

    // 命令模式：查表执行（未注册动词静默忽略，保持既有行为）
    if (const Command* cmd = registry_.find(pc.verb)) {
        cmd->execute(game_, pc.args);
    }
}
