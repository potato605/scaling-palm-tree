// Game.cpp —— 主循环：模式切换、移动、交互、剧情、战斗、存档、结局
#include "Game.h"

#include <chrono>
#include <cstdlib>
#include <fstream>

#include "CommandParser.h"
#include "SaveSystem.h"
#include "Item.h"
#include "Player.h"

// ============================================================
// 基础工具
// ============================================================
namespace {
    uint64_t clockMs() {
        using namespace std::chrono;
        return (uint64_t)duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
    }
    std::wstring wstr(const std::string& s) { return std::wstring(s.begin(), s.end()); }

    // 剧情物品 → 门的联动表
    const std::string doorOfKey(const std::string& keyId) {
        if (keyId == "greenhouse_card") return "g_0_1";
        if (keyId == "lab_permit") return "g_3_4";
        if (keyId == "farm_key") return "g_6_7";
        if (keyId == "core_permit") return "g_9_10";
        return "";
    }
    // 出题人 NPC id（问卷编号顺序）
    const char* kNpcOfQuiz[] = { "linyue", "fangqing", "zhaocheng", "chenyan", "sujin" };
}

Game::Game(bool selftest)
    : world_(nullptr), st_(), input_(), renderer_(), narrator_(),
      dispatcher_(*this), combat_(*this), quests_(*this), hints_(*this),
      selftest_(selftest) {}

Game::~Game() = default;

void Game::post(const std::wstring& text, int color) {
    st_.addMessage(text, color);
    render_ = true;
}

// ============================================================
// 主循环
// ============================================================
void Game::run() {
    ConsoleUtil::init();
    if (selftest_) { selftestRun(); return; }

    uint64_t prev = clockMs();
    while (running_) {
        now_ = clockMs();
        input_.beginFrame(now_);

        // 运行计时
        uint64_t dt = now_ >= prev ? now_ - prev : 0;
        prev = now_;
        st_.elapsedSeconds += (int)(dt / 1000);

        switch (st_.mode) {
            case GameMode::Title: updateModeTitle(); break;
            case GameMode::Narrative: updateModeNarrative(); break;
            case GameMode::Exploration: updateModeExploration(); break;
            case GameMode::Selection: updateModeSelection(); break;
            case GameMode::Combat: updateModeCombat(); break;
            case GameMode::CommandInput: updateModeCommandInput(); break;
            case GameMode::PauseOrConfirm: updateModePauseOrConfirm(); break;
            case GameMode::Ending: updateModeEnding(); break;
            case GameMode::GameOver: updateModeGameOver(); break;
        }

        // 自动三级提示（仅探索）
        if (st_.mode == GameMode::Exploration && world_) {
            std::wstring h = hints_.tick(now_, *world_);
            if (!h.empty() && !infoOpen_) post(h, 14);
        }

        if (render_) renderNow();

        Sleep(10);
    }
    ConsoleUtil::setColor(7, 0);
    ConsoleUtil::hideCursor(false);
}

// ============================================================
// 封面
// ============================================================
void Game::buildSceneTitle() {
    SceneTitle s;
    s.art = GameData::titleArt();
    s.caption = GameData::titleCaption();
    bool hasSave = SaveSystem::exists(SaveSystem::defaultPath());
    s.buttons = {
        { 1, L"新游戏", true, L"" },
        { 2, L"读档", hasSave, hasSave ? L"" : L"没有存档" },
        { 3, L"退出", true, L"" },
    };
    renderer_.renderTitle(s);
}

void Game::updateModeTitle() {
    static bool first = true;
    if (first) { render_ = true; first = false; }
    if (input_.pressed('1')) {
        // 新游戏
        world_ = GameData::buildWorld();
        st_ = GameState();
        st_.px = 3; st_.py = 7;
        st_.checkpointRoom = 0; st_.checkpointX = 3; st_.checkpointY = 7;
        // 序章（逐字播放）
        std::vector<ScriptStep> s;
        s.push_back({ ScriptStep::Kind::Lines, GameData::prologue() });
        s.push_back({ ScriptStep::Kind::Lines,
                      { L"（教学）W/A/S/D 移动；靠近目标后按数字交互；Enter 打开英文命令行；H 提示；F5 存档。" } });
        s.push_back({ ScriptStep::Kind::End });
        startScript(std::move(s));
        render_ = true;
    }
    else if (input_.pressed('2')) {
        doLoadImpl(true);
    }
    else if (input_.pressed('3')) {
        running_ = false;
    }
}

// ============================================================
// 叙事（逐字播放，只接受 Enter 补全当前行）
// ============================================================
void Game::buildSceneNarrative() {
    SceneNarrative s;
    s.previous = narrator_.tailLines(6);
    s.current = narrator_.currentLine();
    size_t shown = narrator_.shown();
    s.shown = (int)std::min(shown, s.current.size());
    s.footer = L"Enter 补全当前行（需按一下松开一下）· 剧情期间不可移动/存读档";
    renderer_.renderNarrative(s);
}

void Game::updateModeNarrative() {
    if (input_.enterPressed()) narrator_.completeCurrentLine();
    narrator_.tick(now_);
    if (render_ || narrator_.playing()) render_ = true;
}

// ============================================================
// 脚本引擎
// ============================================================
void Game::startScript(std::vector<ScriptStep> steps) {
    script_ = std::move(steps);
    scriptPos_ = 0;
    scriptActive_ = true;
    advanceScript();
}

void Game::advanceScript() {
    if (!scriptActive_) return;
    while (scriptPos_ < script_.size()) {
        ScriptStep s = script_[scriptPos_++];
        switch (s.kind) {
            case ScriptStep::Kind::Lines:
                st_.mode = GameMode::Narrative;
                narrator_.start(s.lines, [this] { advanceScript(); });
                render_ = true;
                return;
            case ScriptStep::Kind::Message:
                post(s.message, 11);
                break;
            case ScriptStep::Kind::Quiz: {
                if (selftest_) {
                    // 自检：0 题选对，1 题故意选错（验证答错也给奖励）
                    onQuizAnswered(s.quizIndex, s.quizIndex == 1 ? 0 : 2);
                } else {
                    openQuiz(s.quizIndex);
                    return;
                }
                break;
            }
            case ScriptStep::Kind::GiveKey:
                grantKey(s.arg1, s.message);
                break;
            case ScriptStep::Kind::UnlockDoor:
                world_->unlockDoor(s.arg1);
                break;
            case ScriptStep::Kind::SetCheckpoint:
                st_.checkpointRoom = s.roomId;
                st_.checkpointX = s.x;
                st_.checkpointY = s.y;
                break;
            case ScriptStep::Kind::SetFlag:
                st_.setFlag(s.arg1);
                quests_.checkAndAdvance();
                break;
            case ScriptStep::Kind::DoCombat:
                // 剧情（教学等）结束后开战
                combat_.start(s.arg1, false);
                st_.mode = GameMode::Combat;
                scriptActive_ = false;
                render_ = true;
                return;
            case ScriptStep::Kind::End:
                break;
        }
    }
    scriptActive_ = false;
    quests_.checkAndAdvance();
    // 脚本收尾：战斗仍在进行时不得覆盖战斗模式（如中途剧情结束）
    if (!combat_.active()) st_.mode = GameMode::Exploration;
    if (afterScript_) {
        auto fn = afterScript_;
        afterScript_ = nullptr;
        fn();
    } else {
        render_ = true;
    }
}

void Game::openQuiz(int quizIndex) {
    const Question& q = GameData::questions()[quizIndex];
    std::vector<MenuButton> opts;
    for (size_t i = 0; i < q.options.size(); ++i) {
        opts.push_back({ (int)i + 1, q.options[i], true, L"" });
    }
    openSelection(L"生态知识问答：" + q.text, opts, false,
                  [this, quizIndex](int pick) { onQuizAnswered(quizIndex, pick - 1); },
                  L"答对答错，主线奖励都会给你。");
}

void Game::onQuizAnswered(int quizIndex, int pick) {
    const Question& q = GameData::questions()[quizIndex];
    bool correct = pick >= 0 && pick < (int)q.options.size() && pick == q.correct;
    if (correct) st_.correctAnswers++;

    // 记录对话完成 + 任务推进
    std::string npcId = kNpcOfQuiz[quizIndex];
    st_.quizDone.insert(npcId);
    st_.setFlag("talked_" + npcId);

    // 插入结果文本，继续执行后续奖励步骤
    std::vector<ScriptStep> ins;
    ins.push_back({ ScriptStep::Kind::Lines, { GameData::quizResultText(q, correct) } });
    script_.insert(script_.begin() + scriptPos_, ins.begin(), ins.end());
    quests_.checkAndAdvance();
    advanceScript();
    render_ = true;
}

// ============================================================
// 选项菜单
// ============================================================
void Game::openSelection(const std::wstring& title, const std::vector<MenuButton>& opts,
                         bool cancellable, std::function<void(int)> onPick,
                         const std::wstring& footer) {
    st_.mode = GameMode::Selection;
    selTitle_ = title;
    selOptions_ = opts;
    selFooter_ = footer;
    selCancellable_ = cancellable;
    onPick_ = std::move(onPick);
    render_ = true;
}

void Game::buildSceneSelection() {
    SceneSelection s;
    s.title = selTitle_;
    for (const auto& b : selOptions_) s.options.push_back(b);
    s.cancellable = selCancellable_;
    s.footer = selFooter_;
    renderer_.renderSelection(s);
}

void Game::updateModeSelection() {
    for (const auto& b : selOptions_) {
        if (b.n < 0) continue;
        wchar_t vk = (wchar_t)('0' + b.n);
        if (input_.pressed(vk)) {
            if (!b.enabled) { post(b.reason.empty() ? L"该项不可用。" : b.reason, 12); render_ = true; return; }
            std::function<void(int)> cb = onPick_;
            onPick_ = nullptr;
            if (cb) cb(b.n);
            return;
        }
    }
    if (selCancellable_ && input_.pressed('0')) {
        onPick_ = nullptr;
        st_.mode = combat_.active() ? GameMode::Combat : GameMode::Exploration;
        render_ = true;
    }
}

// ============================================================
// 探索：移动 + 交互
// ============================================================
void Game::updateMovement() {
    auto ev = input_.pollMove(now_);
    if (!ev) return;
    stepCommand(ev->dir);
}

void Game::stepCommand(int dir) {
    if (!world_) return;
    Room* room = world_->room(st_.roomId);
    if (!room) return;
    int dx = dir == 2 ? -1 : dir == 3 ? 1 : 0;
    int dy = dir == 0 ? -1 : dir == 1 ? 1 : 0;
    int nx = st_.px + dx, ny = st_.py + dy;
    if (!room->inBounds(nx, ny)) { input_.noteMoveBlocked(dir); return; }

    const Tile& t = room->tileAt(nx, ny);

    // ---- 墙 / 终端 / 容器等不可通行 ----
    if (!t.passable) {
        std::wstring reason = room->enterBlockReason(nx, ny, st_);
        if (!reason.empty()) post(reason, 8);
        input_.noteMoveBlocked(dir);
        return;
    }

    // ---- 门（锁门/通行） ----
    if (t.ch == L'L' || t.ch == L'D') {
        Interactable* door = nullptr;
        for (auto& it : room->interactables) {
            if (it.kind == Interactable::Kind::Door && it.x == nx && it.y == ny) { door = &it; break; }
        }
        if (!door) { input_.noteMoveBlocked(dir); return; }
        if (!door->unlocked) {
            std::wstring reason = room->enterBlockReason(nx, ny, st_);
            post(reason.empty() ? L"门锁着。" : reason, 12);
            checkTeaching("teach_door", L"（教学）门锁需要对应权限：靠近后按数字或输 open <门>。补给与权限能开锁。");
            input_.noteMoveBlocked(dir);
            return;
        }
        // 进入新房间（从西/东 door 落位）
        bool fromWest = (nx == 0);
        enterRoomInner(door->targetRoom, fromWest);
        return;
    }

    // ---- NPC 实体阻挡 ----
    NPC* npc = room->npcAt(nx, ny);
    if (npc && npc->blocking) {
        post(npc->name + L" 挡着路。靠近按数字交谈。", 8);
        input_.noteMoveBlocked(dir);
        return;
    }
    // ---- 存活的怪物阻挡 ----
    Monster* m2 = room->monsterAt(nx, ny);
    if (m2 && m2->alive()) {
        post(L"前方是" + m2->name + L"——按数字或命令开战。", 8);
        input_.noteMoveBlocked(dir);
        return;
    }

    // ---- 移动 ----
    st_.px = nx; st_.py = ny;
    hints_.noteMove(now_);

    // 危险区（污染水 ~ / 碎玻璃 *）
    if (room->tileDangerous(nx, ny)) {
        int dmg = room->tileAt(nx, ny).dangerDmg;
        st_.hp -= dmg;
        if (st_.hp < 1) st_.hp = 1;   // 危险区不会杀死玩家
        post(L"踩进了危险区（" + std::wstring(t.ch == L'~' ? L"污染水" : L"碎玻璃") +
             L"）：- " + std::to_wstring(dmg) + L" 生命。", 12);
        input_.noteMoveBlocked(dir);
        render_ = true;
        return;
    }

    // ---- 怪物警戒 ----
    for (auto& m : room->monsters) {
        if (!m->alive()) continue;
        if (m->aggroRange > 0 && m->distTo(nx, ny) <= m->aggroRange) {
            post(m->flavor, 12);
            startCombat(m->id);
            return;
        }
    }

    // 任务点/剧情点由数字交互触发（此处不额外拦截）
    render_ = true;
}

// 进入目标房间：fromWest 表示玩家是从"源房间的西墙门"走出的，
// 即从目标房间的东侧进入，落点应在目标房间东端；反之落西端。
void Game::enterRoomInner(int targetRoom, bool fromWest) {
    Room* r = world_->room(targetRoom);
    if (!r) return;
    st_.roomId = targetRoom;
    st_.px = fromWest ? r->width - 3 : 2;
    st_.py = 7;
    onEnterRoom(targetRoom);
    render_ = true;
}

void Game::onEnterRoom(int roomId) {
    Room* r = world_->room(roomId);
    if (!r) return;
    hints_.noteProgress(now_);
    hints_.noteMove(now_);   // 重置同房间移动计数
    st_.setFlag("explored_" + std::to_string(roomId));

    // 区域检查点：每区域首次进入 + BOSS 前
    bool firstVisit = !r->introLines.empty() && !st_.hasFlag("seen_" + std::to_string(roomId));
    if (firstVisit) {
        st_.setFlag("seen_" + std::to_string(roomId));
        std::vector<ScriptStep> s;
        s.push_back({ ScriptStep::Kind::Lines, r->introLines });
        if (roomId == 1 || roomId == 4 || roomId == 7 || roomId == 10) {
            s.push_back({ ScriptStep::Kind::SetCheckpoint, {},
                          L"检查点已记录：" + r->name,
                          L"", -1, "", roomId, st_.px, st_.py });
        }
        s.push_back({ ScriptStep::Kind::End });
        startScript(std::move(s));
    }
    quests_.checkAndAdvance();
}

void Game::checkTeaching(const std::string& flag, const std::wstring& text) {
    if (st_.hasFlag(flag)) return;
    st_.setFlag(flag);
    post(text, 11);
}

void Game::updateModeExploration() {
    // 信息页打开：任意键关闭
    if (infoOpen_) {
        bool any = input_.pressed('W') || input_.pressed('A') || input_.pressed('S') ||
                   input_.pressed('D') || input_.pressed(VK_RETURN) ||
                   input_.pressed(VK_ESCAPE) || input_.pressed('H') ||
                   input_.pressed('Q') || input_.pressed(VK_F5) ||
                   input_.pressed(' ') || input_.pressed('0') || input_.pressed('1') ||
                   input_.pressed('2') || input_.pressed('3') || input_.pressed('4') ||
                   input_.pressed('5') || input_.pressed('6') || input_.pressed('7') ||
                   input_.pressed('8') || input_.pressed('9');
        if (any) { infoOpen_ = false; render_ = true; }
        return;
    }

    updateMovement();

    // 数字快捷交互
    for (int n = 0; n <= 9; ++n) {
        if (input_.pressed((wchar_t)('0' + n))) { quickAction(n); break; }
    }

    if (input_.enterPressed()) {
        st_.mode = GameMode::CommandInput;
        cmdLine_.clear();
        checkTeaching("teach_cmd", L"（教学）输入 help 查看命令，或直接输 talk、look 等英文命令。");
        render_ = true;
        return;
    }
    if (input_.pressed('H')) {
        doHint({});
        return;
    }
    if (input_.pressed(VK_F5)) {
        doSave({});
        return;
    }
    if (input_.pressed('Q')) {
        doQuit({});
        return;
    }
}

// 数字 → ContextAction.commandText → CommandParser → CommandDispatcher → 统一业务
void Game::quickAction(int number) {
    if (number == 0) { post(L"没有 0 号交互。", 12); return; }
    auto actions = InteractionSystem::build(st_, *world_, quests_.objectiveEntity());
    for (auto& a : actions) {
        if (a.number != number) continue;
        if (!a.enabled) {
            post(a.unavailableReason.empty() ? L"这个交互暂时不可用。" : a.unavailableReason, 12);
            return;
        }
        dispatcher_.dispatch(a.commandText);
        return;
    }
    post(L"附近没有对应数字的交互对象。", 12);
}

// ============================================================
// 命令行模式
// ============================================================
void Game::updateModeCommandInput() {
    for (int vk : input_.takeTyped()) {
        if (vk == VK_BACK) {
            if (!cmdLine_.empty()) cmdLine_.pop_back();
        } else if (cmdLine_.size() < 60) {
            if (vk >= 'A' && vk <= 'Z') cmdLine_.push_back((wchar_t)(vk - 'A' + 'a'));
            else if (vk >= '0' && vk <= '9') cmdLine_.push_back((wchar_t)vk);
            else if (vk == VK_SPACE) cmdLine_.push_back(L' ');
        }
        render_ = true;
    }
    if (input_.enterPressed()) {
        std::wstring line = cmdLine_;
        cmdLine_.clear();
        std::string ascii;
        for (wchar_t ch : line) ascii.push_back((char)ch);
        dispatcher_.dispatch(ascii);
        // 命令可能触发战斗/剧情/菜单并改变模式——仅当仍是命令行状态才回收
        if (st_.mode == GameMode::CommandInput) {
            st_.mode = GameMode::Exploration;
        }
        render_ = true;
        return;
    }
    if (input_.pressed(VK_ESCAPE)) {
        st_.mode = GameMode::Exploration;
        cmdLine_.clear();
        render_ = true;
    }
}

// ============================================================
// 战斗模式
// ============================================================
void Game::startCombat(const std::string& monsterId) {
    Monster* m = findMonsterInRoom(monsterId);
    if (!m || !m->alive() || combat_.active()) return;

    // 首战：先播放战斗教学
    if (!st_.hasFlag("flag_combat_tutorial")) {
        st_.setFlag("flag_combat_tutorial");
        std::vector<ScriptStep> s;
        s.push_back({ ScriptStep::Kind::Lines, GameData::combatTutorial() });
        s.push_back({ ScriptStep::Kind::DoCombat, {}, L"", L"", -1, monsterId, -2, -2, -2 });
        startScript(std::move(s));
        return;
    }
    combat_.start(monsterId, false);
}

void Game::buildSceneCombat() {
    SceneCombat sc;
    Monster* m = combat_.target();
    sc.titleLine = L"（角色需到战场上）";   // 仅占位，标题由渲染行处理
    if (m) {
        std::wstring stage;
        if (m->boss) stage = m->bossPhase == 1 ? L"第一阶段：外壳防御"
                            : m->bossPhase == 2 ? L"第二阶段：核心暴露"
                            : L"第三阶段：意识崩解";
        else stage = m->weakened() ? L"基因衰弱（可净化/控制）" : L"出现";
        sc.targetLine = m->name + L"  【" + stage + L"】";
        sc.targetHpLine = L"HP " + std::to_wstring(m->hp < 0 ? 0 : m->hp) + L" / " +
                          std::to_wstring(m->maxHp);
    }
    sc.playerLine = L"你：Lv." + std::to_wstring(st_.level) + L"  HP " +
                    std::to_wstring(st_.hp) + L"/" + std::to_wstring(st_.maxHp);
    sc.hintLine = L"[1]攻击 [2]净化 [3]控制 [4]使用道具 —— 净化/控制需目标基因衰弱（HP<35%）";
    sc.messages = combat_.log();
    sc.buttons = {
        { 1, L"攻击", true, L"" },
        { 2, L"净化", combat_.target() && combat_.target()->weakened(),
          combat_.target() && combat_.target()->weakened() ? L"" : L"需要基因衰弱" },
        { 3, L"控制", st_.hasKey("neural_protocol") && combat_.target() && combat_.target()->weakened(),
          !st_.hasKey("neural_protocol") ? L"缺少神经控制协议" : L"需要基因衰弱" },
        { 4, L"使用道具", st_.itemCount("nutrient_block") > 0, L"没有道具" },
    };
    // 战斗画面直接使用渲染器
    std::vector<std::pair<std::wstring, int>> rows;
    rows.push_back({ L"⚔ 战斗 ⚔", 12 });
    rows.push_back({ sc.targetLine, 12 });
    rows.push_back({ sc.targetHpLine, 12 });
    rows.push_back({ sc.playerLine, 7 });
    rows.push_back({ sc.hintLine, 14 });
    rows.push_back({ L"", 7 });
    for (const auto& mv : sc.messages) rows.push_back(mv);
    rows.push_back({ L"", 7 });
    for (const auto& b : sc.buttons) {
        std::wstring line;
        wchar_t num[8];
        swprintf_s(num, L"[%d] ", b.n);
        line += num;
        line += b.label;
        if (!b.enabled && !b.reason.empty()) line += L"（" + b.reason + L"）";
        rows.push_back({ line, b.enabled ? 14 : 8 });
    }
    renderer_.flush(rows);
}

void Game::updateModeCombat() {
    if (!combat_.active()) { st_.mode = GameMode::Exploration; render_ = true; return; }
    if (input_.pressed('1')) dispatcher_.dispatch("attack");
    else if (input_.pressed('2')) dispatcher_.dispatch("purify");
    else if (input_.pressed('3')) dispatcher_.dispatch("control");
    else if (input_.pressed('4')) dispatcher_.dispatch("use");
    render_ = true;
}

// ============================================================
// 二次确认 / 退出
// ============================================================
void Game::confirmQuit() {
    std::vector<MenuButton> opts = {
        { 1, L"确认退出", true, L"" },
        { 2, L"返回游戏", true, L"" },
    };
    openSelection(L"确定退出吗？", opts, false, [this](int pick) {
        if (pick == 1) running_ = false;
        else { st_.mode = GameMode::Exploration; render_ = true; }
    });
}

void Game::updateModePauseOrConfirm() {
    // 已并入 Selection（确认菜单），此模式保留用于兼容
    if (input_.pressed('1') || input_.pressed('2')) {
        st_.mode = GameMode::Exploration;
        render_ = true;
    }
}

// ============================================================
// 结局 / 死亡
// ============================================================
void Game::openFinalDisposal() {
    bool purifier = st_.hasKey("retrovirus");
    bool protocol = st_.hasKey("neural_protocol");
    std::vector<MenuButton> opts = {
        { 1, L"肃清厄生：污染阻断、终结痛苦", true, L"" },
        { 2, L"净化厄生：逆转录净化剂 + 核心暴露", purifier, purifier ? L"" : L"缺少逆转录净化剂" },
        { 3, L"控制厄生：神经控制协议 + 意识崩解", protocol, protocol ? L"" : L"缺少神经控制协议" },
    };
    openSelection(L"最终处置 —— APF-X00「厄生」", opts, false,
                  [this](int pick) {
        // 二次确认
        const wchar_t* warn = pick == 1 ? L"肃清意味着彻底终结它的生命。确定吗？"
                             : pick == 2 ? L"净化意味着你承担长期的修复责任。确定吗？"
                             : L"控制意味着它将继续痛苦地活着。确定吗？";
        std::vector<MenuButton> cfm = {
            { 1, L"确认", true, L"" },
            { 2, L"取消", true, L"" },
        };
        openSelection(warn, cfm, false, [this, pick](int c) {
            if (c != 1) { finishDisposalContinue(); return; }
            onFinalDisposed(pick);
        });
    });
}

void Game::finishDisposalContinue() {
    // 取消处置：回到战斗（BOSS 残血倒地待处置）
    st_.mode = GameMode::Combat;
    render_ = true;
}

void Game::onFinalDisposed(int pick) {
    Monster* boss = findMonsterInRoom("eps");
    if (!boss) {
        st_.mode = GameMode::Exploration;
        render_ = true;
        return;
    }
    const std::string name = pick == 1 ? "eliminate" : pick == 2 ? "purify" : "control";
    if (pick == 1) { boss->disp = Monster::Disposition::Eliminated; ++st_.eliminateCount; st_.finalDisposition = L"肃清"; }
    else if (pick == 2) { boss->disp = Monster::Disposition::Purified; ++st_.purifyCount; st_.finalDisposition = L"净化"; }
    else { boss->disp = Monster::Disposition::Controlled; ++st_.controlCount; st_.finalDisposition = L"控制"; }
    int ups = Player::addExp(st_, boss->exp);
    (void)ups;
    st_.questStage = 12;
    st_.mode = GameMode::Exploration;
    // 结局文本逐字播放后进入结局统计画面
    std::vector<ScriptStep> s;
    s.push_back({ ScriptStep::Kind::Lines, GameData::endingLines(name) });
    s.push_back({ ScriptStep::Kind::End, {} });
    afterScript_ = [this] { st_.mode = GameMode::Ending; render_ = true; };
    startScript(std::move(s));
}

void Game::updateModeEnding() {
    if (input_.pressed('1')) {
        st_ = GameState();
        world_.reset();
        render_ = true;
    } else if (input_.pressed('2')) {
        running_ = false;
    }
}

void Game::dieNow(const std::vector<std::wstring>& notes) {
    deathNotes_ = notes;
    st_.mode = GameMode::GameOver;
    render_ = true;
}

void Game::updateModeGameOver() {
    // 任意键返回检查点
    if (input_.pressed(' ') || input_.pressed(VK_RETURN) || input_.pressed('W') ||
        input_.pressed('A') || input_.pressed('S') || input_.pressed('D') ||
        input_.pressed('Q') || input_.pressed(VK_F5) || input_.pressed('H')) {
        st_.restoreVitals();
        st_.roomId = st_.checkpointRoom;
        st_.px = st_.checkpointX >= 0 ? st_.checkpointX : 2;
        st_.py = st_.checkpointY >= 0 ? st_.checkpointY : 7;
        st_.mode = GameMode::Exploration;
        post(L"你从最近的检查点醒来（检查点：" + world_->roomName(st_.roomId) + L"，生命恢复 60%）。", 14);
        render_ = true;
    }
}

// ============================================================
// 命令实现（CommandDispatcher 统一路由）
// ============================================================
void Game::doLook(const std::vector<std::string>&) {
    hints_.noteLook(now_);
    Room* room = world_->room(st_.roomId);
    if (!room) return;
    post(L"你在" + room->name + L"。W/A/S/D 移动；靠近对象按数字交互。", 7);
    auto acts = InteractionSystem::build(st_, *world_, quests_.objectiveEntity());
    if (!acts.empty()) {
        std::wstring line = L"附近：";
        int shown = 0;
        for (const auto& a : acts) {
            if (shown >= 3) break;
            if (shown) line += L"|";
            line += std::to_wstring(a.number) + a.chineseLabel;
            ++shown;
        }
        post(line, 11);
    }
    render_ = true;
}

void Game::doMission(const std::vector<std::string>&) {
    infoOpen_ = true;
    infoTitle_ = L"任务 mission";
    std::wstring text = quests_.missionText();
    infoLines_.clear();
    size_t pos = 0;
    while (pos < text.size()) {
        size_t nxt = text.find(L'\n', pos);
        if (nxt == std::wstring::npos) nxt = text.size();
        infoLines_.push_back(text.substr(pos, nxt - pos));
        pos = nxt + 1;
    }
    render_ = true;
}

void Game::doHint(const std::vector<std::string>&) {
    infoOpen_ = true;
    infoTitle_ = L"提示 hint";
    std::wstring text = hints_.hintText(*world_);
    infoLines_.clear();
    size_t pos = 0;
    while (pos < text.size()) {
        size_t nxt = text.find(L'\n', pos);
        if (nxt == std::wstring::npos) nxt = text.size();
        infoLines_.push_back(text.substr(pos, nxt - pos));
        pos = nxt + 1;
    }
    render_ = true;
}

void Game::doHelp(const std::vector<std::string>&) {
    infoOpen_ = true;
    infoTitle_ = L"帮助 help";
    infoLines_ = {
        L"移动：W/A/S/D（短按一格，长按连续；撞墙请松手）",
        L"交互：靠近目标后按数字；或输入英文命令（数字与命令完全等价）",
        L"",
        L"命令列表（不区分大小写）：",
        L"look        看房间与附近交互",
        L"mission     当前主线任务",
        L"hint        下一步建议",
        L"inventory   背包（inv）",
        L"status      角色属性",
        L"talk <id>   与目标交谈（如 talk linyue）",
        L"open <id>   打开门/容器（open exit_e）",
        L"read <id>   阅读档案",
        L"access <id> 访问终端",
        L"take <id>   拾取物品/样本",
        L"inspect <id> 查看痕迹",
        L"attack/purify/control <id>  战斗处置",
        L"use         使用恢复道具",
        L"save        存档（F5 快捷）",
        L"load        读档",
        L"quit        退出（Q 快捷）",
        L"",
        L"提示：靠近对象后按数字，不需要背命令。中文命令会被拒绝。",
    };
    render_ = true;
}

void Game::doInventory(const std::vector<std::string>&) {
    // 复用 info 页
    infoOpen_ = true;
    infoTitle_ = L"背包 inventory";
    infoLines_.clear();
    bool hasKeyItem = false;
    for (const ItemDef& def : allItemDefs()) {
        if (isKeyItemKind(def.kind) && st_.hasKey(def.id)) {
            hasKeyItem = true;
            infoLines_.push_back(L"关键物品：" + def.name + L" —— " + def.desc);
        }
    }
    if (!hasKeyItem) infoLines_.push_back(L"关键物品：无");
    infoLines_.push_back(L"");
    int nb = st_.itemCount("nutrient_block");
    infoLines_.push_back(L"消耗品：生物营养块 ×" + std::to_wstring(nb) + L"（恢复 80 生命，使用：use）");
    render_ = true;
}

void Game::doStatus(const std::vector<std::string>&) {
    infoOpen_ = true;
    infoTitle_ = L"状态 status";
    infoLines_ = {
        L"等级 Lv." + std::to_wstring(st_.level) + L"   经验 " +
            std::to_wstring(st_.exp) + L"/" + std::to_wstring(Player::expNeedFor(st_.level)),
        L"生命 HP " + std::to_wstring(st_.hp) + L"/" + std::to_wstring(st_.maxHp) +
            L"   攻击 " + std::to_wstring(st_.attack) + L"   防御 " + std::to_wstring(st_.defense),
        L"处置统计：肃清 " + std::to_wstring(st_.eliminateCount) +
            L" 净化 " + std::to_wstring(st_.purifyCount) +
            L" 控制 " + std::to_wstring(st_.controlCount),
        L"生态题正确 " + std::to_wstring(st_.correctAnswers) +
            L"/5   档案 " + std::to_wstring(st_.archivesRead) +
            L"   死亡 " + std::to_wstring(st_.deaths),
    };
    render_ = true;
}

// ---- 通用：找 id 目标并给出距离检查 ----
NPC* Game::findNPCInRoom(const std::string& id) {
    Room* room = world_->room(st_.roomId);
    if (!room) return nullptr;
    for (auto& npc : room->npcs) if (npc->id == id) return npc.get();
    return nullptr;
}
Monster* Game::findMonsterInRoom(const std::string& id) {
    Room* room = world_->room(st_.roomId);
    if (!room) return nullptr;
    for (auto& m : room->monsters) if (m->id == id) return m.get();
    return nullptr;
}
Interactable* Game::findInteractableInRoom(const std::string& id) {
    Room* room = world_->room(st_.roomId);
    if (!room) return nullptr;
    for (auto& it : room->interactables) if (it.id == id) return &it;
    return nullptr;
}
bool Game::nearTarget(int x, int y) const {
    int d = (x - st_.px < 0 ? st_.px - x : x - st_.px) +
            (y - st_.py < 0 ? st_.py - y : y - st_.py);
    return d <= 1;
}

void Game::doTalk(const std::vector<std::string>& args) {
    if (args.empty()) {
        for (auto& npc : world_->room(st_.roomId)->npcs) {
            if (nearTarget(npc->x, npc->y)) { post(L"可以用 talk " + wstr(npc->id) + L" 与" + npc->name + L"交谈。", 14); return; }
        }
        post(L"附近没有可交谈的人。", 12);
        return;
    }
    NPC* npc = findNPCInRoom(args[0]);
    if (!npc) { post(L"当前房间没有这个人。", 12); return; }
    if (!nearTarget(npc->x, npc->y)) { post(L"走得太远了，靠近再交谈。", 12); return; }
    if (st_.quizDone.count(npc->id)) {
        post(npc->repeatLines.empty() ? L"你们已经谈过了。" : npc->repeatLines[0], 11);
        return;
    }
    startNpcScript(npc->id);
}

void Game::startNpcScript(const std::string& npcId) {
    NPC* npc = findNPCInRoom(npcId);
    if (!npc) return;
    if (st_.quizDone.count(npc->id)) { doTalk({ npc->id }); return; }

    std::vector<ScriptStep> s;
    // 首次对话 = 剧情 + 生态题 + 奖励
    for (const auto& line : npc->firstLines) {
        s.push_back({ ScriptStep::Kind::Lines, { line } });
    }
    s.push_back({ ScriptStep::Kind::Quiz, {}, L"", L"", npc->quizIndex });
    s.push_back({ ScriptStep::Kind::Message, {}, npc->rewardMsg });
    s.push_back({ ScriptStep::Kind::GiveKey, {}, L"", L"", -1, npc->rewardKey });
    if (!npc->rewardKey2.empty()) {
        s.push_back({ ScriptStep::Kind::GiveKey, {}, L"", L"", -1, npc->rewardKey2 });
    }
    s.push_back({ ScriptStep::Kind::UnlockDoor, {}, L"", L"", -1, doorOfKey(npc->rewardKey) });
    // 关键 NPC 处检查点（控制室/安全房）
    if (npc->id == "linyue") { s.push_back({ ScriptStep::Kind::SetCheckpoint, {}, L"", L"", -1, "", 0, st_.px, st_.py }); }
    if (npc->id == "fangqing") { s.push_back({ ScriptStep::Kind::SetCheckpoint, {}, L"", L"", -1, "", 3, st_.px, st_.py }); }
    if (npc->id == "zhaocheng") { s.push_back({ ScriptStep::Kind::SetCheckpoint, {}, L"", L"", -1, "", 6, st_.px, st_.py }); }
    if (npc->id == "chenyan") { s.push_back({ ScriptStep::Kind::SetCheckpoint, {}, L"", L"", -1, "", 9, st_.px, st_.py }); }
    if (npc->id == "sujin") { s.push_back({ ScriptStep::Kind::SetCheckpoint, {}, L"", L"", -1, "", 10, 2, 7 }); }
    s.push_back({ ScriptStep::Kind::SetFlag, {}, L"", L"", -1, "talked_" + npc->id });
    s.push_back({ ScriptStep::Kind::End });
    startScript(std::move(s));
}

// ---- 关键物品（防丢失：固定获得 + 联动开门；奖励消息由 NPC 脚本提供） ----
void Game::grantKey(const std::string& keyId, const std::wstring& msg) {
    const ItemDef* def = findItemDef(keyId);
    if (!def) return;
    if (!st_.hasKey(keyId)) {
        st_.addKey(keyId);
        post(L"★ 获得关键物品【" + def->name + L"】", 11);
    }
    if (!msg.empty()) post(msg, 11);
    // 联动解锁路径
    std::string doorId = doorOfKey(keyId);
    if (!doorId.empty()) {
        world_->unlockDoor(doorId);
        post(std::wstring(L"信息：") + def->name + L" 对应的防护门已解锁（地图上显示绿色 D）。", 10);
    }
    hints_.noteProgress(now_);
    render_ = true;
}

void Game::doOpen(const std::vector<std::string>& args) {
    if (args.empty()) {
        prelistInteractions();
        return;
    }
    Interactable* it = findInteractableInRoom(args[0]);
    if (!it) { post(L"当前房间没有这个对象。", 12); return; }
    switch (it->kind) {
        case Interactable::Kind::Door: {
            if (!nearTarget(it->x, it->y)) { post(L"距离太远，走到门边再说。", 12); return; }
            if (it->unlocked) {
                bool fromWest = (it->x == 0);
                enterRoomInner(it->targetRoom, fromWest);
            } else {
                std::wstring reason;
                const ItemDef* need = it->requiredKey.empty() ? nullptr : findItemDef(it->requiredKey);
                if (need && !st_.hasKey(it->requiredKey)) {
                    reason = L"门锁着，需要【" + need->name + L"】——按提示先找对应 NPC 领取。";
                } else if (need) {
                    world_->unlockDoor(it->doorId);
                    post(L"咔嚓——" + it->name + L"解锁了。", 10);
                    render_ = true;
                } else reason = L"门锁着。";
                if (!reason.empty()) {
                    post(reason, 12);
                    checkTeaching("teach_door", L"（教学）锁门需要对应权限；权限固定从关键 NPC 处获得。");
                }
            }
            break;
        }
        case Interactable::Kind::Container: {
            if (!nearTarget(it->x, it->y)) { post(L"距离太远。", 12); return; }
            if (it->opened) { post(it->name + L" 已经空了。", 8); return; }
            it->opened = true;
            const ItemDef* def = findItemDef(it->itemId);
            if (def) {
                st_.items[it->itemId] += it->itemCount;
                post(L"你打开" + it->name + L"，获得 " + def->name +
                     (it->itemCount > 1 ? std::wstring(L" ×") + std::to_wstring(it->itemCount) : L"") +
                     L"（背包 +" + std::to_wstring(it->itemCount) + L"）。", 10);
                checkTeaching("teach_item", L"（教学）关键道具与消耗品在 inventory 中查看。");
            }
            hints_.noteProgress(now_);
            render_ = true;
            break;
        }
        default:
            post(L"可以直接说 read/access/take 的方式处理它。", 8);
            break;
    }
}

void Game::doRead(const std::vector<std::string>& args) {
    if (args.empty()) { prelistInteractions(); return; }
    Interactable* it = findInteractableInRoom(args[0]);
    if (!it) { post(L"当前房间没有这个档案。", 12); return; }
    if (!nearTarget(it->x, it->y)) { post(L"距离太远。", 12); return; }
    if (it->kind != Interactable::Kind::Archive && it->kind != Interactable::Kind::Terminal) {
        post(L"这个对象无法阅读。", 12);
        return;
    }
    if (!st_.archivesDone.count(it->id)) {
        st_.archivesDone.insert(it->id);
        ++st_.archivesRead;
    }
    std::vector<ScriptStep> s;
    for (const auto& line : it->archiveLines) s.push_back({ ScriptStep::Kind::Lines, { line } });
    s.push_back({ ScriptStep::Kind::End });
    startScript(std::move(s));
}

void Game::doAccess(const std::vector<std::string>& args) {
    if (args.empty()) { prelistInteractions(); return; }
    Interactable* it = findInteractableInRoom(args[0]);
    if (!it) { post(L"当前房间没有这个终端。", 12); return; }
    if (!nearTarget(it->x, it->y)) { post(L"距离太远。", 12); return; }
    if (it->kind != Interactable::Kind::Terminal) { post(L"这不是终端。", 12); return; }
    if (!st_.archivesDone.count(it->id)) {
        st_.archivesDone.insert(it->id);
        ++st_.archivesRead;
    }
    std::vector<ScriptStep> s;
    for (const auto& line : it->archiveLines) s.push_back({ ScriptStep::Kind::Lines, { line } });
    s.push_back({ ScriptStep::Kind::End });
    startScript(std::move(s));
}

void Game::doInspect(const std::vector<std::string>& args, const std::wstring&) {
    if (args.empty()) { prelistInteractions(); return; }
    Interactable* it = findInteractableInRoom(args[0]);
    if (!it) {
        Monster* m = findMonsterInRoom(args[0]);
        if (m) { post(L"查看" + m->name + L"：" + m->flavor, 7); render_ = true; return; }
        post(L"没有可查看的对象。", 12);
        return;
    }
    if (!nearTarget(it->x, it->y)) { post(L"距离太远。", 12); return; }
    if (it->kind != Interactable::Kind::Landmark) { post(L"这个对象用其他方式交互。", 8); return; }
    if (!st_.archivesDone.count(it->id)) {
        st_.archivesDone.insert(it->id);
        ++st_.archivesRead;
    }
    std::vector<ScriptStep> s;
    for (const auto& line : it->archiveLines) s.push_back({ ScriptStep::Kind::Lines, { line } });
    s.push_back({ ScriptStep::Kind::End });
    startScript(std::move(s));
}

void Game::doTake(const std::vector<std::string>& args) {
    if (args.empty()) { prelistInteractions(); return; }
    Interactable* it = findInteractableInRoom(args[0]);
    if (!it) { post(L"当前房间没有这个物品。", 12); return; }
    if (it->kind != Interactable::Kind::GroundItem) { post(L"这个对象不能拾取。", 12); return; }
    if (!nearTarget(it->x, it->y)) { post(L"距离太远。", 12); return; }
    if (it->opened) { post(L"这里已经空了。", 8); return; }
    it->opened = true;
    const ItemDef* def = findItemDef(it->itemId);
    if (!def) return;
    if (isKeyItemKind(def->kind)) {
        if (!st_.hasKey(def->id)) post(L"★ 获得关键物品【" + def->name + L"】", 11);
        st_.addKey(def->id);
        post(L"污染样本已安全封装，随身的样本槽亮起一格。", 10);
        hints_.noteProgress(now_);
    } else {
        st_.items[def->id] += it->itemCount;
        post(L"获得 " + def->name + L" ×" + std::to_wstring(it->itemCount) + L"。", 10);
    }
    quests_.checkAndAdvance();
    render_ = true;
}

void Game::prelistInteractions() {
    auto acts = InteractionSystem::build(st_, *world_, quests_.objectiveEntity());
    if (acts.empty()) {
        post(L"附近没有人/物。试试 mission 查看任务。", 8);
        return;
    }
    std::wstring line;
    for (const auto& a : acts) {
        if (a.number > 5) break;
        if (!line.empty()) line += L"|";
        line += std::to_wstring(a.number) + a.chineseLabel;
    }
    post(L"附近交互：" + line, 11);
    render_ = true;
}

// ---- 战斗命令（探索状态也可 direkt 触发） ----
void Game::startCombatFromCommand(const std::string& monsterId) {
    Monster* m = findMonsterInRoom(monsterId);
    if (!m || !m->alive()) { post(L"这里没有可战斗的目标。", 12); return; }
    if (!nearTarget(m->x, m->y)) { post(L"离得太远——先走过去（进入警戒圈自动开战）。", 12); return; }
    startCombat(monsterId);
}

void Game::doAttack(const std::vector<std::string>& args) {
    // 战斗中：按 [1]（无参数）即攻击当前目标（数字快捷与 attack 命令共用此入口）
    if (st_.mode == GameMode::Combat) { combat_.handle("attack"); return; }
    if (args.empty()) {
        // 探索中缺目标：列出附近敌人
        Room* room = world_->room(st_.roomId);
        if (room) {
            for (auto& m : room->monsters) {
                if (m->alive() && nearTarget(m->x, m->y))
                    post(L"可以使用 attack " + wstr(m->id) + L"。", 14);
                if (m->alive()) post(L"目标：" + m->name, 14);
            }
        }
        post(L"缺少目标：attack <id>，或靠近敌人后按数字 1。", 8);
        return;
    }
    startCombatFromCommand(args[0]);
}

void Game::doPurify(const std::vector<std::string>& args) {
    if (st_.mode == GameMode::Combat) { combat_.handle("purify"); return; }
    if (args.empty()) { post(L"缺少目标：purify <id>。", 8); return; }
    Monster* m = findMonsterInRoom(args[0]);
    if (!m) { post(L"当前房间没有这个目标。", 12); return; }
    if (m->boss) { post(L"厄生需要在战斗中处置（见最终处置菜单）。", 12); return; }
    if (!nearTarget(m->x, m->y)) { post(L"距离太远。", 12); return; }
    if (m->treated()) { post(L"已经处置过了。", 8); return; }
    combat_.quickPurify(args[0]);   // 与战斗内净化共用同一业务（applyPurify）
}

void Game::doControl(const std::vector<std::string>& args) {
    if (st_.mode == GameMode::Combat) { combat_.handle("control"); return; }
    if (args.empty()) { post(L"缺少目标：control <id>。", 8); return; }
    Monster* m = findMonsterInRoom(args[0]);
    if (!m) { post(L"当前房间没有这个目标。", 12); return; }
    if (m->boss) { post(L"厄生需要在战斗中处置（见最终处置菜单）。", 12); return; }
    if (!nearTarget(m->x, m->y)) { post(L"距离太远。", 12); return; }
    if (m->treated()) { post(L"已经处置过了。", 8); return; }
    combat_.quickControl(args[0]);   // 与战斗内控制共用同一业务（applyControl）
}

void Game::doUse(const std::vector<std::string>&) {
    std::vector<MenuButton> opts;
    int nb = st_.itemCount("nutrient_block");
    opts.push_back({ 1, L"生物营养块 ×" + std::to_wstring(nb) + L"（恢复 80）", nb > 0, nb > 0 ? L"" : L"没有库存" });
    bool inCombat = st_.mode == GameMode::Combat;
    openSelection(L"使用道具", opts, true, [this](int pick) {
        if (pick != 1) { st_.mode = combat_.active() ? GameMode::Combat : GameMode::Exploration; render_ = true; return; }
        if (!InventoryConsume("nutrient_block")) { post(L"道具不足。", 12); render_ = true; return; }
        Player::heal(st_, 80);
        post(L"使用生物营养块：恢复 80 点生命（当前 HP " +
             std::to_wstring(st_.hp) + L"/" + std::to_wstring(st_.maxHp) + L"）。", 10);
        if (!combat_.active()) { st_.mode = GameMode::Exploration; render_ = true; }
        else { st_.mode = GameMode::Combat; render_ = true; }
    }, inCombat ? L"取消 [0] 返回战斗" : L"取消 [0]");
}

bool Game::InventoryConsume(const std::string& id) {
    auto it = st_.items.find(id);
    if (it == st_.items.end() || it->second <= 0) return false;
    --it->second;
    if (it->second <= 0) st_.items.erase(it);
    return true;
}

void Game::doSave(const std::vector<std::string>&) { doSaveImpl(); }

void Game::doSaveImpl() {
    if (st_.mode == GameMode::Narrative || st_.mode == GameMode::Combat ||
        st_.mode == GameMode::Selection || st_.mode == GameMode::PauseOrConfirm ||
        st_.mode == GameMode::Ending || st_.mode == GameMode::GameOver || st_.mode == GameMode::Title) {
        post(L"当前状态不支持存读档（剧情播放中不可保存）。", 8);
        return;
    }
    std::string err = SaveSystem::save(st_, *world_, SaveSystem::defaultPath());
    post(err.empty() ? L"已存档（saves/slot1.txt）。" : L"存档失败：" + wstr(err), err.empty() ? 10 : 12);
    render_ = true;
}

void Game::doLoad(const std::vector<std::string>&) { doLoadImpl(false); }

void Game::doLoadImpl(bool fromTitle) {
    if (!fromTitle && (st_.mode == GameMode::Narrative || st_.mode == GameMode::Combat)) {
        post(L"当前状态不支持读档。", 8);
        return;
    }
    if (!world_) world_ = GameData::buildWorld();
    std::string err = SaveSystem::load(st_, *world_, SaveSystem::defaultPath());
    if (!err.empty()) {
        std::wstring werr = wstr(err);
        if (fromTitle) {
            st_ = GameState();
            st_.mode = GameMode::Title;
            st_.addMessage(werr, 12);
        } else {
            // 损坏存档：安全返回标题，不崩溃
            st_ = GameState();
            st_.mode = GameMode::Title;
            st_.addMessage(werr, 12);
        }
        render_ = true;
        return;
    }
    // 任务/关键物品一致性（损坏保护第二道闸）
    std::wstring cons = quests_.verifyConsistency(st_);
    if (!cons.empty()) {
        st_ = GameState();
        st_.mode = GameMode::Title;
        st_.addMessage(cons, 12);
        render_ = true;
        return;
    }
    st_.mode = GameMode::Exploration;
    st_.addMessage(L"读档完成：已恢复到" + world_->roomName(st_.roomId) + L"。", 10);
    render_ = true;
}

void Game::doQuit(const std::vector<std::string>&) { confirmQuit(); }

// ============================================================
// 渲染装配
// ============================================================
void Game::buildSceneRoom() {
    if (!world_) { renderer_.flush({ { L"（未初始化）", 12 } }); return; }
    Room* room = world_->room(st_.roomId);
    if (!room) { renderer_.flush({ { L"（房间未找到）", 12 } }); return; }

    SceneRoom sc;
    sc.titleLine = room->name;
    sc.map.w = room->width;
    sc.map.h = room->height;
    sc.map.rows.assign(room->height, std::vector<FrameCell>(room->width));

    // 1) 静态层
    for (int y = 0; y < room->height; ++y)
        for (int x = 0; x < room->width; ++x) {
            const Tile& t = room->grid[y][x];
            sc.map.rows[y][x] = { t.ch, t.fg };
        }
    // 2) 交互物层（门状态字符 / 地面物品 / 终端容器字符已由 tile 提供）
    for (const auto& it : room->interactables) {
        if (!room->inBounds(it.x, it.y)) continue;
        FrameCell& c = sc.map.rows[it.y][it.x];
        if (it.kind == Interactable::Kind::Door) {
            c.ch = it.unlocked ? L'D' : L'L';
            c.fg = it.unlocked ? 10 : 12;
        } else if (it.kind == Interactable::Kind::GroundItem) {
            const ItemDef* def = findItemDef(it.itemId);
            c.ch = it.opened ? L'.' :
                   (def && def->kind == ItemDef::Sample) ? L'S' : L'I';
            c.fg = 10;
        }
    }
    // 3) NPC 层（带目标 '!' 标记）
    for (const auto& npc : room->npcs) {
        if (!room->inBounds(npc->x, npc->y)) continue;
        FrameCell& c = sc.map.rows[npc->y][npc->x];
        c.ch = L'N';
        c.fg = npc->blocking ? 11 : 13;
    }
    // 4) 怪物层
    for (const auto& m : room->monsters) {
        if (!room->inBounds(m->x, m->y)) continue;
        FrameCell& c = sc.map.rows[m->y][m->x];
        switch (m->disp) {
            case Monster::Disposition::Alive:
            case Monster::Disposition::Weakened:
                c.ch = m->boss ? L'X' : L'M';
                c.fg = m->boss ? 12 : (m->weakened() ? 13 : 12);
                break;
            case Monster::Disposition::Purified: c.ch = L'+'; c.fg = 10; break;
            case Monster::Disposition::Controlled: c.ch = L'R'; c.fg = 11; break;
            case Monster::Disposition::Eliminated: c.ch = L'*'; c.fg = 7; break;
        }
    }
    // 5) 主线目标 '!'
    if (auto obj = quests_.objectiveEntity()) {
        for (const auto& npc : room->npcs) {
            if (npc->id == *obj) { FrameCell& c = sc.map.rows[npc->y][npc->x]; c.ch = L'!'; c.fg = 14; }
        }
        for (const auto& m : room->monsters) {
            if (m->id == *obj && m->alive()) { FrameCell& c = sc.map.rows[m->y][m->x]; c.ch = L'!'; c.fg = 14; }
        }
    }
    // 6) 玩家
    if (room->inBounds(st_.px, st_.py)) {
        FrameCell& c = sc.map.rows[st_.py][st_.px];
        c.ch = L'@';
        c.fg = 14;
    }
    // 7) 图例行并入标题下（简单图例）
    std::wstring legend = L"图例 @你 N人 M怪 X厄生 I物品 S样本 T终端 C容器 D门 L锁 ~水 *危险 !目标";
    (void)legend;   // 图例在 suggestion 行后追加显示（节省行数，不再单独列）

    sc.hudLine = Player::hudText(st_);
    sc.objectiveLine = L"[任务] " + quests_.stage().name + L"：" + quests_.stage().next;
    sc.suggestionLine = L"[建议] " + (quests_.directionText() == L"当前房间"
                                      ? L"当前房间内行动：先解决目标附近的怪物与物品。"
                                      : L"先前往" + quests_.directionText() + L"，或按数字交互（H=hint）。");
    sc.messages = st_.messages;

    auto acts = InteractionSystem::build(st_, *world_, quests_.objectiveEntity());
    int shown = 0;
    for (const auto& a : acts) {
        if (shown >= 5) break;
        MenuButton b;
        b.n = a.number;
        b.label = a.chineseLabel;
        if (a.enabled) {
            b.enabled = true;
            b.label += L"（" + a.effectDescription + L"）";
        } else {
            b.enabled = false;
            b.reason = a.unavailableReason;
        }
        sc.buttons.push_back(b);
        ++shown;
    }
    sc.inputLine = L"Command > " + cmdLine_;
    renderer_.renderRoom(sc);
}

void Game::renderNow() {
    switch (st_.mode) {
        case GameMode::Title: buildSceneTitle(); break;
        case GameMode::Narrative: buildSceneNarrative(); break;
        case GameMode::Exploration:
            if (infoOpen_) {
                SceneInfo si; si.title = infoTitle_; si.lines = infoLines_;
                si.footer = L"按任意键返回探索（数字菜单与移动都已暂停）";
                renderer_.renderInfo(si);
            } else {
                buildSceneRoom();
            }
            break;
        case GameMode::Selection: buildSceneSelection(); break;
        case GameMode::Combat: buildSceneCombat(); break;
        case GameMode::CommandInput: buildSceneRoom(); break;
        case GameMode::PauseOrConfirm: { SceneSelection s; s.title = selTitle_; s.options = selOptions_; renderer_.renderSelection(s); break; }
        case GameMode::Ending: buildSceneEnding(); break;
        case GameMode::GameOver: renderer_.renderGameOver(deathNotes_); break;
    }
}

void Game::buildSceneEnding() {
    // 结局帧：结局文本 + 统计
    std::vector<std::pair<std::wstring, int>> rows;
    rows.push_back({ L"◈ 结局 ◈", 10 });
    rows.push_back({ L"", 7 });
    // 结局文本已通过 Narrative 播放；此页显示统计
    rows.push_back({ L"—— Cleaner-07 任务报告 ——", 15 });
    rows.push_back({ L"最终处置：" + st_.finalDisposition, 14 });
    rows.push_back({ L"肃清 " + std::to_wstring(st_.eliminateCount) +
                     L" · 净化 " + std::to_wstring(st_.purifyCount) +
                     L" · 控制 " + std::to_wstring(st_.controlCount), 7 });
    rows.push_back({ L"生态题正确 " + std::to_wstring(st_.correctAnswers) + L"/5 · 档案 " +
                     std::to_wstring(st_.archivesRead) + L" · 死亡 " +
                     std::to_wstring(st_.deaths) + L" 次", 7 });
    wchar_t tm[64];
    swprintf_s(tm, L"总用时 %02d:%02d", st_.elapsedSeconds / 60, st_.elapsedSeconds % 60);
    rows.push_back({ tm, 7 });
    std::wstring verdict;
    if (st_.finalDisposition == L"肃清") verdict = L"综合评价：安全优先的执行者——但你写下了一个没有答案的问题。";
    else if (st_.finalDisposition == L"净化") verdict = L"综合评价：温柔而沉重。你选择了承担，而不是终结。";
    else verdict = L"综合评价：现实主义者。你控制住了它——但控制不负责解决痛苦。";
    rows.push_back({ verdict, 13 });
    rows.push_back({ L"", 7 });
    rows.push_back({ L"[1] 返回标题   [2] 退出游戏", 14 });
    renderer_.flush(rows);
}

// ============================================================
// 自检（--selftest）：驱动完整主线，验证核心逻辑
// ============================================================
void Game::selftestRun() {
    // 简化：直接开始探索模式（无标题/序章），并使用高层 API 跑流程
    std::vector<std::string> failures;
    auto check = [&](bool ok, const std::string& what) {
        if (!ok) failures.push_back(what);
        printf("[%s] %s\n", ok ? "PASS" : "FAIL", what.c_str());
    };

    world_ = GameData::buildWorld();
    st_ = GameState();
    st_.px = 3; st_.py = 7;
    st_.mode = GameMode::Exploration;
    narrator_.setInstant(true);

    // 直接传送（自检不做完整键鼠模拟，交互距离校验照常生效）
    auto tp = [&](int x, int y) { st_.px = x; st_.py = y; };

    // 剧情泵：instant 叙事 + 脚本推进直到全部完成
    auto pumpAndScript = [&](int guard = 120) {
        int g = 0;
        while (g++ < guard) {
            if (narrator_.playing()) { narrator_.tick(nowMs() + 1); continue; }
            if (!scriptActive_) break;
            advanceScript();
        }
    };

    // ---- 1. 序章与入口 ----
    check(quests_.stageNum() == 1, "初始阶段 1");
    // ---- 2. 补给箱 ----
    tp(25, 7);
    doOpen({ "supply_crate" });
    check(st_.itemCount("nutrient_block") == 2, "补给箱获得 2 块营养块");
    // ---- 3. 林阅对话（答对题）----
    tp(13, 7);
    doTalk({ "linyue" });
    pumpAndScript();
    check(st_.hasFlag("talked_linyue"), "林阅对话完成");
    check(st_.hasKey("greenhouse_card"), "答对仍获得温室卡");
    check(st_.correctAnswers == 1, "生态题正确数 = 1");
    // 门已解锁
    bool door0 = false;
    for (auto& it : world_->room(0)->interactables)
        if (it.kind == Interactable::Kind::Door && it.doorId == "g_0_1" && it.unlocked) door0 = true;
    check(door0, "温室门已解锁");
    // ---- 4. mission/help 不崩溃 ----
    doMission({}); doHint({}); doHelp({}); doInventory({}); doStatus({});
    infoOpen_ = false;

    auto walkTo = [&](int tx, int ty) -> bool {
        // 简化寻路：直线先移动再绕 — 用 BFS 路径点序列逐个 stepCommand
        Room* room = world_->room(st_.roomId);
        struct Nd { int x, y, dir, prev; };
        std::vector<Nd> q;
        q.push_back({ st_.px, st_.py, -1, -1 });
        std::vector<int> seen(room->width * room->height, -1);
        seen[st_.py * room->width + st_.px] = 0;
        bool found = false;
        int endIdx = -1;
        static const int dx[4] = { 0, 0, -1, 1 }, dy[4] = { -1, 1, 0, 0 };
        for (size_t i = 0; i < q.size(); ++i) {
            if (q[i].x == tx && q[i].y == ty) { found = true; endIdx = (int)i; break; }
            if (q[i].x == tx && q[i].y == ty) break;
            for (int d = 0; d < 4; ++d) {
                int nx = q[i].x + dx[d], ny = q[i].y + dy[d];
                if (!room->inBounds(nx, ny)) continue;
                if (seen[ny * room->width + nx] >= 0) continue;
                const Tile& t = room->tileAt(nx, ny);
                if (!t.passable) continue;
                if (room->monsterAt(nx, ny) && room->monsterAt(nx, ny)->alive()) continue;
                if (room->npcAt(nx, ny) && room->npcAt(nx, ny)->blocking) continue;
                seen[ny * room->width + nx] = (int)q.size();
                q.push_back({ nx, ny, d, (int)i });
            }
        }
        if (!found) return false;
        // 回溯路径
        std::vector<int> path;
        int cur = endIdx;
        while (q[cur].prev >= 0) { path.push_back(q[cur].dir); cur = q[cur].prev; }
        for (auto it = path.rbegin(); it != path.rend(); ++it) {
            stepCommand(*it);
            if (st_.mode != GameMode::Exploration) return true;   // 途中触发战斗/剧情也视为到达
        }
        return true;
    };

    auto fightTo = [&]() {
        // 战斗直至结束（优先攻击；弱化后净化）
        int guard = 0;
        while (combat_.active() && guard++ < 60) {
            // 阵亡保护：从检查点恢复并回满血继续（自检不判失败）
            if (st_.mode == GameMode::GameOver) {
                st_.mode = GameMode::Exploration;
                st_.restoreVitals();
                st_.roomId = st_.checkpointRoom;
                st_.px = st_.checkpointX; st_.py = st_.checkpointY;
            }
            // 战斗激活时强制处于战斗模式（模拟玩家已在战斗中按键）
            if (combat_.active()) st_.mode = GameMode::Combat;
            Monster* m = combat_.target();
            if (!m) break;
            // 走真实分发路径（与玩家按键 1~4 完全相同），验证数字快捷→命令管线
            if (!m->boss) {
                if (m->weakened()) dispatcher_.dispatch("purify");
                else dispatcher_.dispatch("attack");
            } else {
                dispatcher_.dispatch("attack");
            }
            if (scriptActive_) return;   // 有剧情（教学/阶段）转入 narrative
        }
    };

    auto moveToRoom = [&](int roomId) {
        // 一直往东走直到进入目标房间
        int guard = 0;
        while (st_.roomId != roomId && guard++ < 20) {
            // 走到东门旁再前进
            Room* room = world_->room(st_.roomId);
            if (!walkTo(room->width - 2, 7)) { failures.push_back("无法走到东门 room=" + std::to_string(st_.roomId)); return false; }
            if (st_.mode != GameMode::Exploration) {
                // 战斗/剧情打断：先处理
                if (scriptActive_) pumpAndScript();
                if (combat_.active()) fightTo();
                if (st_.mode == GameMode::Selection) {
                    // 答题：自检已自动处理，直接关菜单
                    st_.mode = GameMode::Exploration;
                }
                continue;
            }
            stepCommand(3);   // 向东进门
        }
        return st_.roomId == roomId;
    };

    // ---- 5. 温室前廊战斗 + 净化 ----
    check(moveToRoom(1), "进入温室前廊");
    // 步骤闭环：教学脚本 → 战斗 → （可能被走位再次触发）
    if (scriptActive_) pumpAndScript();
    if (combat_.active()) fightTo();
    if (st_.mode == GameMode::Exploration) walkTo(19, 7);
    if (st_.mode == GameMode::Exploration) walkTo(18, 7);
    if (scriptActive_) pumpAndScript();
    if (combat_.active()) fightTo();
    check(!world_->room(1)->monsters.empty() && world_->room(1)->monsters[0]->treated(), "双头煞一已处置");

    // ---- 6. 培育舱：样本一 + 双头煞二 ----
    check(moveToRoom(2), "进入培育舱");
    tp(43, 5);
    doTake({ "sample_1" });
    check(st_.hasKey("sample_1"), "取得样本一");

    // ---- 7. 方晴（故意答错）----
    check(moveToRoom(3), "进入温室控制室");
    tp(13, 7);
    doTalk({ "fangqing" });
    pumpAndScript();
    check(st_.hasKey("lab_permit"), "答错仍获得实验层权限");
    check(st_.correctAnswers == 1, "答错不增加正确数");

    // ---- 8-12: 一路向东 ----
    check(moveToRoom(4), "进入隔离走廊");
    check(moveToRoom(5), "进入活体实验舱");
    tp(48, 7);
    doTake({ "sample_2" });
    check(st_.hasKey("sample_2"), "取得样本二");
    check(moveToRoom(6), "进入安保值班室");
    tp(13, 7);
    doTalk({ "zhaocheng" });
    pumpAndScript();
    check(st_.hasKey("farm_key") && st_.hasKey("neural_protocol"), "赵诚双件套");
    check(moveToRoom(7), "进入饲养场入口");
    check(moveToRoom(8), "进入破损围栏区");
    tp(52, 10);
    doTake({ "sample_3" });
    check(st_.hasKey("sample_3"), "取得样本三");
    check(moveToRoom(9), "进入生态观察室");
    tp(15, 7);
    doTalk({ "chenyan" });
    pumpAndScript();
    check(st_.hasKey("retrovirus") && st_.hasKey("core_permit"), "陈砚双件套");
    check(moveToRoom(10), "进入中央控制室前厅");
    // 清场：逐个迎战尚未处置的普通怪（真实玩家靠走位进入警戒圈触发；
    // 自检用紧凑传送跳过走位，这里显式补一次交战验证战斗逻辑）
    for (auto& r : world_->rooms()) {
        for (auto& m : r->monsters) {
            if (m->id == "eps" || !m->alive()) continue;
            st_.roomId = r->id;
            st_.px = m->x > 1 ? m->x - 1 : m->x + 1;
            st_.py = m->y;
            if (st_.px >= r->width - 1) st_.px = r->width - 2;
            if (scriptActive_) pumpAndScript();   // 残留房间 intro 先放完
            startCombat(m->id);
            if (scriptActive_) pumpAndScript();
            if (combat_.active()) fightTo();
        }
    }
    int aliveCount = 0;
    for (auto& r : world_->rooms())
        for (auto& m : r->monsters)
            if (m->id != "eps" && m->alive()) ++aliveCount;
    check(aliveCount == 0, "全部普通怪物已处置（计数）");

    // ---- 13. 苏瑾 ----
    st_.roomId = 10;   // 清场循环把玩家留在了饲养区，回到前厅
    tp(33, 7);
    doTalk({ "sujin" });
    pumpAndScript();
    check(st_.hasFlag("talked_sujin"), "苏瑾记录完成");
    // ---- 14. BOSS（三阶段：外壳→核心→意识）----
    check(moveToRoom(11), "进入嵌合核心室");
    if (scriptActive_) pumpAndScript();   // 房间 intro 叙事先播完
    st_.hp = st_.maxHp;   // 自检保证满血进 BOSS（真实游戏由最终补给+检查点保证）
    walkTo(33, 7);
    if (scriptActive_) pumpAndScript();
    if (combat_.active()) fightTo();
    // 三阶段推进断言：战斗结束时已进入处置菜单
    check(st_.mode == GameMode::Selection, "三阶段完成，进入最终处置菜单");

    // ---- 15. 最终处置：净化（真实路径：菜单 [2] → 二次确认 [1]）----
    {
        int guard = 0;
        while (st_.mode == GameMode::Selection && guard++ < 6) {
            if (selTitle_.find(L"最终处置") != std::wstring::npos) { selftestPick(2); continue; }
            if (selTitle_.find(L"确定吗") != std::wstring::npos) { selftestPick(1); continue; }
            // 其他菜单（不影响主线）直接关闭
            st_.mode = GameMode::Exploration;
        }
        pumpAndScript();
        check(st_.finalDisposition == L"净化", "最终处置：净化（含二次确认）");
        check(st_.mode == GameMode::Ending, "结局画面进入（Ending 模式）");
    }

    // ---- 16. 存档/读档/损坏档 ----
    std::string saveErr = SaveSystem::save(st_, *world_, SaveSystem::defaultPath());
    check(saveErr.empty(), "存档成功");
    int beforeRoom = st_.roomId;
    st_.roomId = 0; st_.px = 1; st_.py = 1;
    std::string loadErr = SaveSystem::load(st_, *world_, SaveSystem::defaultPath());
    check(loadErr.empty() && st_.roomId == beforeRoom, "读档恢复房间");
    // 损坏存档
    {
        std::ofstream ofs(SaveSystem::defaultPath(), std::ios::binary);
        ofs << "garbage not a save file";
    }
    GameState before = st_;
    std::string badErr = SaveSystem::load(st_, *world_, SaveSystem::defaultPath());
    check(!badErr.empty(), "损坏存档被拒绝: " + badErr);
    st_ = before;   // 恢复（Game 内保持）

    if (failures.empty()) printf("\nSELFTEST PASS: 全部核心检查通过\n");
    else {
        printf("\nSELFTEST FAIL (%d):\n", (int)failures.size());
        for (auto& f : failures) printf("  - %s\n", f.c_str());
    }
    exit(failures.empty() ? 0 : 1);
}

// 自检菜单模拟：等价于在菜单中按数字 n
void Game::selftestPick(int n) {
    for (const auto& b : selOptions_) {
        if (b.n == n) {
            if (!b.enabled) return;
            std::function<void(int)> cb = onPick_;
            onPick_ = nullptr;
            if (cb) cb(n);
            return;
        }
    }
}
