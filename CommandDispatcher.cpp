// CommandDispatcher.cpp —— 数字快捷交互的统一入口
#include "CommandDispatcher.h"
#include "CommandParser.h"
#include "Game.h"

namespace {
    bool combatVerb(const std::string& v) {
        return v == "attack" || v == "purify" || v == "control" || v == "use";
    }
    bool exploreVerb(const std::string& v) {
        return v == "talk" || v == "open" || v == "read" || v == "take" ||
               v == "access" || v == "inspect" || v == "attack" ||
               v == "purify" || v == "control" || v == "use";
    }
}

void CommandDispatcher::dispatch(const std::string& rawLine) {
    GameState& st = game_.state();
    ParsedCommand pc = CommandParser::parse(rawLine);
    if (!pc.ok) {
        game_.post(pc.error, 12);
        return;
    }

    // 模式隔离
    switch (st.mode) {
        case GameMode::Narrative:
        case GameMode::Selection:
        case GameMode::PauseOrConfirm:
        case GameMode::Title:
        case GameMode::Ending:
        case GameMode::GameOver:
            return;   // 这些画面不接受数字交互
        case GameMode::Combat:
            if (!combatVerb(pc.verb)) return;
            break;
        case GameMode::Exploration:
            if (!exploreVerb(pc.verb)) return;
            break;
    }

    const std::string& v = pc.verb;
    if (v == "talk") game_.doTalk(pc.args);
    else if (v == "open") game_.doOpen(pc.args);
    else if (v == "read") game_.doRead(pc.args);
    else if (v == "take") game_.doTake(pc.args);
    else if (v == "access") game_.doAccess(pc.args);
    else if (v == "inspect") game_.doInspect(pc.args, L"查看");
    else if (v == "attack") game_.doAttack(pc.args);
    else if (v == "purify") game_.doPurify(pc.args);
    else if (v == "control") game_.doControl(pc.args);
    else if (v == "use") game_.doUse(pc.args);
}
