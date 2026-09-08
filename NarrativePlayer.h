// NarrativePlayer.h —— 固定速度逐字播放（标点停顿 / Enter 只补全当前行）
#pragma once

#include <functional>
#include <string>
#include <vector>
#include <cstdint>

class NarrativePlayer {
public:
    using DoneFn = std::function<void()>;

    // 开始播放多行剧情；每行独立逐字播放，行尾停顿后自动进入下一行
    // charMs: 普通字符间隔；combatMode=true 时约 15ms 快速播放（战斗短消息）
    void start(std::vector<std::wstring> lines, DoneFn onDone,
               int charMs = 25, bool combatMode = false);

    void tick(uint64_t now);

    // Enter 补全当前行（不含下一行）
    void completeCurrentLine();
    bool playing() const { return playing_; }
    bool lineFinished() const { return lineFinished_; }

    // 渲染数据
    size_t index() const { return li_; }                 // 当前行号
    size_t shown() const { return ci_; }                 // 当前行已显示字符数
    const std::wstring& currentLine() const;
    std::vector<std::wstring> tailLines(size_t n) const; // 最近 n 行已完成内容

    // 自检模式：0 延迟
    void setInstant(bool b) { instant_ = b; }

private:
    struct CharDelay { wchar_t ch = 0; uint64_t ms = 0; };

    std::vector<std::wstring> lines_;
    std::vector<std::vector<CharDelay>> delays_;
    DoneFn onDone_;
    uint64_t lastTick_ = 0;
    uint64_t carryMs_ = 0;    // 未消耗的时间
    uint64_t lineDoneAt_ = 0; // 当前行显示完成时刻（行尾停顿用）
    size_t li_ = 0, ci_ = 0;
    bool playing_ = false;
    bool lineFinished_ = false;
    bool instant_ = false;
    bool combat_ = false;
    int charMs_ = 25;

    void buildDelays(size_t li);
    void advanceLine();        // 行尾停顿完成后推进到下一行
};
