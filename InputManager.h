// InputManager.h —— 按键状态、Enter 边沿检测、WASD 短按/长按、命令行字符收集
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

class InputManager {
public:
    // 方向：0上 1下 2左 3右
    struct MoveEv {
        enum T { Press, Auto } type;
        int dir;
    };

    // 每帧快照（约 50Hz），之后才能调用查询方法
    void beginFrame(uint64_t now);

    // ---- 通用按键 ----
    bool pressed(int vk) const;   // 本帧恰好按下（边沿）
    bool released(int vk) const;  // 本帧恰好松开
    bool down(int vk) const;      // 当前按住

    // ---- WASD 移动（短按一格 + 长按连发）----
    // 首次重复延迟约 180ms，持续间隔约 80ms
    std::optional<MoveEv> pollMove(uint64_t now);
    // 撞墙后暂停该方向自动连发，必须松开才能重新请求
    void noteMoveBlocked(int dir);

    // ---- Enter：按下->松开->再次按下 才算一次有效（边沿检测）----
    bool enterPressed();          // 消费式：同一次边沿只返回一次
    bool enterReleased() const;

    // ---- 命令行字符（a-z A-Z 0-9 空格 退格 回车）----
    std::vector<int> takeTyped();

    // ------------------------------------------------------------------
    // 自检模式（--selftest）：注入按键序列，替代真实键盘读取
    struct ScriptKey { int vk; bool isDown; uint64_t atMs; };
    void beginScript(std::vector<ScriptKey> script);
    bool scripted() const { return scripted_; }

private:
    struct KState {
        bool downPrev = false, downNow = false;
        bool pressedEdge = false, releasedEdge = false;  // 本帧边沿
        uint64_t downAt = 0;          // 本次按下时刻
        uint64_t firstRepeatAt = 0;   // 长按首次重复时刻
        uint64_t lastAutoAt = 0;
    };
    std::unordered_map<int, KState> states_;
    int lastDir_ = -1;                       // 最后按下方向
    std::array<bool, 4> blocked_{false, false, false, false};
    bool enterArmed_ = true;                 // 初始为已松开，允许启动即按
    bool enterPending_ = false;
    std::vector<int> typed_;
    uint64_t now_ = 0;

    // 自检模式
    bool scripted_ = false;
    std::vector<ScriptKey> script_;
    size_t scriptIdx_ = 0;

    void sample(int vk);

    // 私有采样入口：真实键盘 / 自检注入
    bool realDown(int vk) const;
};
