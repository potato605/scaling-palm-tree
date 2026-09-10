// NarrativePlayer.cpp —— 固定速度逐字播放：Unicode 字符级、标点停顿、Enter 只补全当前行
#include "NarrativePlayer.h"

#include <algorithm>

void NarrativePlayer::start(std::vector<std::wstring> lines, DoneFn onDone,
                            int charMs, bool combatMode) {
    lines_ = std::move(lines);
    onDone_ = std::move(onDone);
    charMs_ = charMs;
    combat_ = combatMode;
    li_ = 0;
    ci_ = 0;
    carryMs_ = 0;
    playing_ = !lines_.empty();
    lineFinished_ = false;
    lastTick_ = 0;
    delays_.assign(lines_.size(), {});
    if (playing_) buildDelays(0);
}

void NarrativePlayer::buildDelays(size_t li) {
    if (li >= lines_.size()) return;
    const std::wstring& line = lines_[li];
    auto& d = delays_[li];
    d.clear();
    d.reserve(line.size());
    for (wchar_t ch : line) {
        uint64_t ms = (uint64_t)(charMs_ > 0 ? charMs_ : 0);
        // 中文按 Unicode 字符播放，标点额外停顿
        if (ch == L'，' || ch == L'、') ms += 60;
        else if (ch == L'。' || ch == L'？' || ch == L'！' || ch == L'；') ms += 100;
        d.push_back({ ch, ms });
    }
}

void NarrativePlayer::tick(uint64_t now) {
    if (!playing_) return;

    if (instant_) {
        // 自检：瞬间完整显示
        if (li_ + 1 < lines_.size()) { ++li_; ci_ = 0; }
        else { li_ = lines_.size(); playing_ = false; if (onDone_) onDone_(); }
        return;
    }

    if (lastTick_ == 0) lastTick_ = now;
    uint64_t dt = now >= lastTick_ ? now - lastTick_ : 0;
    lastTick_ = now;
    carryMs_ += dt;

    if (lineFinished_) {
        // 行尾停顿后自动进入下一行（Enter 不会跨行）
        if (now >= lineDoneAt_ + (combat_ ? 100 : 180)) advanceLine();
        return;
    }

    const auto& d = delays_[li_];
    while (ci_ < d.size() && carryMs_ >= d[ci_].ms) {
        carryMs_ -= d[ci_].ms;
        ++ci_;
    }
    if (ci_ >= d.size()) {
        lineFinished_ = true;
        lineDoneAt_ = now;
    }
}

void NarrativePlayer::completeCurrentLine() {
    if (!playing_ || lineFinished_) return;
    ci_ = delays_[li_].size();
    lineFinished_ = true;
    lineDoneAt_ = lastTick_;
}

void NarrativePlayer::advanceLine() {
    if (!playing_) return;
    ++li_;
    ci_ = 0;
    carryMs_ = 0;
    lineFinished_ = false;
    if (li_ >= lines_.size()) {
        playing_ = false;
        if (onDone_) onDone_();
        return;
    }
    buildDelays(li_);
}

const std::wstring& NarrativePlayer::currentLine() const {
    static const std::wstring empty;
    if (li_ >= lines_.size()) return empty;
    return lines_[li_];
}

std::vector<std::wstring> NarrativePlayer::tailLines(size_t n) const {
    std::vector<std::wstring> out;
    size_t start = li_ > n ? li_ - n : 0;
    for (size_t i = start; i < li_; ++i) out.push_back(lines_[i]);
    return out;
}
