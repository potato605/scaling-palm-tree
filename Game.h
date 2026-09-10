// Game.h —— 主循环：模式切换、标题、新游戏/读档、房间切换、剧情触发、结局
#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "GameState.h"
#include "World.h"
#include "InputManager.h"
#include "ConsoleRenderer.h"
#include "NarrativePlayer.h"
#include "CommandDispatcher.h"
#include "CombatSystem.h"
#include "QuestSystem.h"
#include "HintSystem.h"
#include "InteractionSystem.h"
#include "GameData.h"

class Game {
public:
    explicit Game(bool selftest = false);
    ~Game();
    void run();

    // ---------- 组件访问（系统间协作） ----------
    GameState& state() { return st_; }
    World* world() { return world_.get(); }
    uint64_t nowMs() const { return now_; }
    void post(const std::wstring& text, int color = 7);
    void reqRender() { render_ = true; }
    CombatSystem& combatSys() { return combat_; }
    QuestSystem& quests() { return quests_; }
    HintSystem& hints() { return hints_; }

    // ---------- 命令统一业务（CommandDispatcher 调用） ----------
    void doLook(const std::vector<std::string>& args);
    void doMission(const std::vector<std::string>& args);
    void doHint(const std::vector<std::string>& args);
    void doHelp(const std::vector<std::string>& args);
    void doInventory(const std::vector<std::string>& args);
    void doStatus(const std::vector<std::string>& args);
    void doInspect(const std::vector<std::string>& args, const std::wstring& label);
    void doRead(const std::vector<std::string>& args);
    void doAccess(const std::vector<std::string>& args);
    void doTalk(const std::vector<std::string>& args);
    void doOpen(const std::vector<std::string>& args);
    void doTake(const std::vector<std::string>& args);
    void doAttack(const std::vector<std::string>& args);
    void doPurify(const std::vector<std::string>& args);
    void doControl(const std::vector<std::string>& args);
    void doUse(const std::vector<std::string>& args);
    void doSave(const std::vector<std::string>& args);
    void doLoad(const std::vector<std::string>& args);
    void doQuit(const std::vector<std::string>& args);

    // ---------- 剧情/战斗/交互入口 ----------
    void startNpcScript(const std::string& npcId);
    void startCombat(const std::string& monsterId);
    void enterRoom(int targetRoom);

    // 关键物品获得（带提示 + 联动解锁）
    void grantKey(const std::string& keyId, const std::wstring& msg);

    // 数字快捷入口：数字 -> ContextAction.commandText
    void quickAction(int number);

    // 存档读档（统一显示）
    void doSaveImpl();
    void doLoadImpl(bool fromTitle);

    // 交互辅助
    Interactable* findInteractableInRoom(const std::string& id);
    Monster* findMonsterInRoom(const std::string& id);
    NPC* findNPCInRoom(const std::string& id);
    bool nearTarget(int x, int y) const;

    // selftest
    void selftestRun();
    void dieNow(const std::vector<std::wstring>& notes);

private:
    // ---------- 运行时数据 ----------
    std::unique_ptr<World> world_;
    GameState st_;
    InputManager input_;
    ConsoleRenderer renderer_;
    NarrativePlayer narrator_;
    CommandDispatcher dispatcher_;
    CombatSystem combat_;
    QuestSystem quests_;
    HintSystem hints_;

    // 脚本引擎
    std::vector<ScriptStep> script_;
    size_t scriptPos_ = 0;
    bool scriptActive_ = false;
    std::function<void()> afterScript_;   // 脚本正常结束后的追加动作（如进入结局画面）

    // 选项菜单
    std::wstring selTitle_, selFooter_;
    std::vector<MenuButton> selOptions_;
    std::function<void(int)> onPick_;
    bool selCancellable_ = false;

    // 全屏信息页（help/mission/hint；仍处于 Exploration 模式，按任意键返回）
    bool infoOpen_ = false;
    std::wstring infoTitle_;
    std::vector<std::wstring> infoLines_;

    // 旧命令兼容输入（主界面不显示、不主动打开）
    bool cmdOpen_ = false;
    std::wstring cmdLine_;
    std::vector<std::wstring> cmdHist_;

    // 时间
    uint64_t now_ = 0;
    uint64_t sessionStartMs_ = 0;
    uint64_t sessionPauseStartMs_ = 0;
    uint64_t gameTimeMs_ = 0; // 可暂停/可忽略剧情的时间不用，直接记录运行秒数

    bool render_ = true;
    bool running_ = true;
    bool selftest_ = false;
    bool firstExplore_ = true;
    bool titleTeamOpen_ = false;

    // 死亡现场
    std::vector<std::wstring> deathNotes_;

    // ---------- 主循环分派 ----------
    void updateModeTitle();
    void updateModeNarrative();
    void updateModeExploration();
    void updateModeSelection();
    void updateModeCombat();
    void updateModeCommandInput();
    void updateModePauseOrConfirm();
    void updateModeEnding();
    void updateModeGameOver();

    // ---------- 探索 ----------
    void updateMovement();
    void stepCommand(int dir);
    void enterRoomInner(int targetRoom, bool fromWest);  // 从西/东门进入新房间
    void onEnterRoom(int roomId);   // 检查点/首次描写/教学
    void checkTeaching(const std::string& flag, const std::wstring& text);
    void prelistInteractions();     // 近距离对象列表（缺目标时提示）
    void startCombatFromCommand(const std::string& monsterId);
    bool InventoryConsume(const std::string& id);

    // ---------- 场景装配（渲染） ----------
    void buildSceneRoom();
    void buildSceneCombat();
    void buildSceneNarrative();
    void buildSceneTitle();
    void buildSceneSelection();
    void buildSceneInfo();
    void buildSceneEnding();
    void buildSceneGameOver();
    void renderNow();

    // ---------- 脚本引擎 ----------
    void startScript(std::vector<ScriptStep> steps);
    void advanceScript();

    // ---------- 菜单 ----------
    void openSelection(const std::wstring& title, const std::vector<MenuButton>& opts,
                       bool cancellable, std::function<void(int)> onPick,
                       const std::wstring& footer = L"");
    std::function<void(int)> detachOnPick() { return std::move(onPick_); }
    bool selMenuOut() const { return selCancellable_; }

    // ---------- 剧情事件 ----------
    void openQuiz(int quizIndex);
    void onQuizAnswered(int quizIndex, int pick);
    void openFinalDisposal();
    void onFinalDisposed(int pick);
    void confirmQuit();
    void finishDisposalContinue();   // 最终处置取消：回到战斗
    // 自检菜单模拟（--selftest）：等价于按数字 n
    void selftestPick(int n);

    // ---------- 状态书签 ----------
    void updateStatsTime();          // 累计 elapsedSeconds
    void applyDeath(const std::wstring& cause, const std::wstring& advice);

    friend class CombatSystem;       // 战斗系统需触发最终处置菜单
};
