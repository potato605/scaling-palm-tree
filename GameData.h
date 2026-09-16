// GameData.h —— 数据中枢：房间地图、NPC、怪物、题目、剧情、档案、结局、脚本
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "World.h"

// 生态题
struct Question {
    std::wstring asker;                 // 出题 NPC
    std::wstring text;                  // 题目
    std::vector<std::wstring> options;  // 1~4
    int correct = 0;                    // 正确项（0 起）
    std::wstring explanation;           // 正确答案与解释
};

// 剧情脚本步骤（对话 = Lines + Quiz + 奖励 + 结束）
struct ScriptStep {
    enum class Kind {
        Lines, Message, Quiz, GiveKey, UnlockDoor,
        SetCheckpoint, SetFlag, DoCombat, End
    };
    Kind kind = Kind::End;
    std::vector<std::wstring> lines;    // Lines：逐字播放
    std::wstring message;               // Message：立即提示
    std::wstring text;                  // 扩展文本
    int quizIndex = -1;                 // Quiz：题目编号
    std::string arg1;                   // GiveKey keyId / UnlockDoor doorId / SetFlag 名 / DoCombat 怪物 id
    int roomId = -1, x = -1, y = -1;    // SetCheckpoint
};

namespace GameData {
    // 构建 6 个独立房间的完整世界（每张地图 68×15）
    std::unique_ptr<World> buildWorld();

    // 5 道生态题
    const std::vector<Question>& questions();

    // 大型 ASCII 标题与副标题说明
    const std::vector<std::wstring>& titleArt();
    const std::vector<std::wstring>& titleCaption();

    // 序章正文
    const std::vector<std::wstring>& prologue();

    // 首战教学
    const std::vector<std::wstring>& combatTutorial();

    // 三种结局正文（不含统计）
    std::vector<std::wstring> endingLines(const std::string& which);

    // 答题结果文本
    std::wstring quizResultText(const Question& q, bool correct);
}
