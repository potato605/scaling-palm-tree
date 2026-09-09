// InputManager.cpp —— 按键采样、Enter 边沿、WASD 长按连发
#include "InputManager.h"

#include <algorithm>

// 监视的按键
namespace {
    const std::vector<int> kKeySet = {
        'W', 'A', 'S', 'D', VK_RETURN, VK_ESCAPE, VK_F5,
        'Q', 'H', VK_SPACE, VK_BACK,
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
    };
    int dirOf(int vk) {
        switch (vk) {
            case 'W': return 0;
            case 'S': return 1;
            case 'A': return 2;
            default:  return 3;   // 'D'
        }
    }
    bool isDirKey(int vk) { return vk == 'W' || vk == 'A' || vk == 'S' || vk == 'D'; }
}

bool InputManager::realDown(int vk) const {
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
}

void InputManager::beginFrame(uint64_t now) {
    now_ = now;

    // 脚本模式：按时间注入按键事件
    if (scripted_) {
        while (scriptIdx_ < script_.size() && script_[scriptIdx_].atMs <= now) {
            const ScriptKey& ev = script_[scriptIdx_++];
            KState& s = states_[ev.vk];
            if (ev.isDown) s.downNow = true; else s.downNow = false;
        }
    }

    // 快照 → 边沿计算
    for (int vk : kKeySet) {
        KState& s = states_[vk];
        bool cur = scripted_ ? s.downNow : realDown(vk);
        if (!scripted_) s.downNow = cur;

        s.pressedEdge = false;
        s.releasedEdge = false;

        if (cur && !s.downPrev) {
            s.pressedEdge = true;
            s.downAt = now;
            s.firstRepeatAt = now + 180;   // 首次重复延迟约 180ms
            s.lastAutoAt = now;
        } else if (!cur && s.downPrev) {
            s.releasedEdge = true;
        }
        s.downPrev = cur;

        // 方向键的按下/松开：更新 lastDir 与解除 blocked
        if (isDirKey(vk)) {
            int d = dirOf(vk);
            if (s.pressedEdge) {
                blocked_[d] = false;
                lastDir_ = d;
            }
            if (s.releasedEdge) {
                blocked_[d] = false;
                if (lastDir_ == d) lastDir_ = -1;
            }
        }

        // Enter 边沿：需要 松开->按下 完整周期
        if (vk == VK_RETURN) {
            if (s.pressedEdge && enterArmed_) {
                enterPending_ = true;
                enterArmed_ = false;
            }
            if (s.releasedEdge) enterArmed_ = true;
        }

        // 命令行字符收集
        if (s.pressedEdge) {
            bool isChar = (vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9') ||
                          vk == VK_SPACE || vk == VK_BACK;
            if (isChar) typed_.push_back(vk);
        }
    }
}

bool InputManager::pressed(int vk) const {
    auto it = states_.find(vk);
    return it != states_.end() && it->second.pressedEdge;
}

bool InputManager::released(int vk) const {
    auto it = states_.find(vk);
    return it != states_.end() && it->second.releasedEdge;
}

bool InputManager::down(int vk) const {
    auto it = states_.find(vk);
    return it != states_.end() && it->second.downPrev;
}

bool InputManager::enterPressed() {
    if (!enterPending_) return false;
    enterPending_ = false;
    return true;
}

bool InputManager::enterReleased() const {
    return released(VK_RETURN);
}

std::optional<InputManager::MoveEv> InputManager::pollMove(uint64_t now) {
    if (lastDir_ < 0) {
        // 全部松开/未按下：找仍按住的方向（优先最新）恢复
        int best = -1; uint64_t bestAt = 0;
        for (int d = 0; d < 4; ++d) {
            int vk = d == 0 ? 'W' : d == 1 ? 'S' : d == 2 ? 'A' : 'D';
            auto it = states_.find(vk);
            if (it == states_.end() || !it->second.downPrev) continue;
            if (best < 0 || it->second.downAt > bestAt) { best = d; bestAt = it->second.downAt; }
        }
        lastDir_ = best;
        if (best < 0) return std::nullopt;
    }

    int d = lastDir_;
    int vk = d == 0 ? 'W' : d == 1 ? 'S' : d == 2 ? 'A' : 'D';
    auto it = states_.find(vk);
    if (it == states_.end() || !it->second.downPrev) {
        lastDir_ = -1;
        return std::nullopt;
    }
    if (blocked_[d]) return std::nullopt;   // 撞墙后必须松开

    KState& s = it->second;

    // 按下边沿：立即移动一次
    if (s.pressedEdge) {
        s.pressedEdge = false;             // 只发一次
        return MoveEv{ MoveEv::Press, d };
    }
    // 长按连发：180ms 后每 80ms 一次
    if (now >= s.firstRepeatAt && now - s.lastAutoAt >= 80) {
        s.lastAutoAt = now;
        return MoveEv{ MoveEv::Auto, d };
    }
    return std::nullopt;
}

void InputManager::noteMoveBlocked(int dir) {
    if (dir >= 0 && dir < 4) blocked_[dir] = true;
}

std::vector<int> InputManager::takeTyped() {
    std::vector<int> out;
    out.swap(typed_);
    return out;
}

// ---------- 自检模式 ----------
void InputManager::beginScript(std::vector<ScriptKey> script) {
    scripted_ = true;
    script_ = std::move(script);
    scriptIdx_ = 0;
    for (auto& kv : states_) {
        kv.second.downPrev = false;
        kv.second.downNow = false;
    }
}
