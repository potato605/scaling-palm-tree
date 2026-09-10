// ConsoleRenderer.h —— 无闪烁整帧渲染：光标定位 + 逐字符颜色 + 尾部补空格
#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <string>
#include <vector>
#include <utility>
#include <windows.h>

namespace ConsoleUtil {
    // 兼容 CONSOLE_COLORS 的 16 色枚举
    enum Color {
        Black = 0, DarkBlue = 1, DarkGreen = 2, DarkCyan = 3, DarkRed = 4,
        DarkMagenta = 5, DarkYellow = 6, DarkGray = 7, Gray = 8, Blue = 9,
        Green = 10, Cyan = 11, Red = 12, Magenta = 13, Yellow = 14, White = 15
    };

    void init();                 // UTF-8 代码页 + 自动调整窗口尺寸（96×30，失败则保持）
    void hideCursor(bool hide);
    void setColor(int fg, int bg);
    void setCursorPos(int x, int y);
    int windowWidth();
    int windowHeight();

    // 通用泛型辅助函数（课程技术点：模板函数）
    template <typename T>
    T clampValue(const T& v, const T& lo, const T& hi) {
        return v < lo ? lo : (hi < v ? hi : v);
    }

    // ---- 控制台显示宽度（东亚宽字符占 2 光栅列）----
    bool isWideChar(wchar_t c);
    int displayWidth(const std::wstring& s);
    // 按显示单元格上限截断（超宽行防止自动折行破坏帧布局）
    std::wstring fitLine(const std::wstring& s, int maxCells);
    // 补空格到指定显示宽度
    std::wstring padTo(const std::wstring& s, int cells);
    // 软换行：按显示宽度把长行拆成多行（叙事用，不丢字符）
    std::vector<std::wstring> wrapLine(const std::wstring& s, int maxCells);
}

// 渲染用最小结构（Game 装配，ConsoleRenderer 只负责画）
struct FrameCell { wchar_t ch = L' '; int fg = 7; };
struct FrameGrid { int w = 0, h = 0; std::vector<std::vector<FrameCell>> rows; };

// 菜单按钮（数字快捷项）
struct MenuButton {
    int n = 0;
    std::wstring label;
    bool enabled = true;
    std::wstring reason;   // 禁用原因（置灰显示）
};

struct SceneTitle {
    std::vector<std::wstring> art;       // 大型 ASCII 标题
    std::vector<std::wstring> caption;   // 中文名/背景/操作提示
    std::vector<MenuButton> buttons;
};

struct SceneRoom {
    std::wstring titleLine;              // 房间名
    FrameGrid map;
    std::wstring hudLine;                // 状态栏
    std::wstring backpackLine;           // 背包栏
    std::wstring objectiveLine;          // [任务]
    std::wstring suggestionLine;         // [建议]
    std::vector<std::wstring> legendLines; // [解释] 地图图例
    std::vector<std::pair<std::wstring, int>> messages;  // 消息 + 颜色
    std::vector<MenuButton> buttons;     // 附近交互（数字快捷）
    std::wstring inputLine;              // 兼容字段，探索界面不再显示命令行
};

struct SceneCombat {
    std::wstring titleLine;
    std::wstring targetLine;
    std::wstring targetHpLine;
    std::wstring playerLine;
    std::wstring hintLine;
    std::vector<std::pair<std::wstring, int>> messages;
    std::vector<MenuButton> buttons;     // 战斗菜单
};

struct SceneNarrative {
    std::vector<std::wstring> previous;  // 已完成行（最近 6 行）
    std::wstring current;                // 当前行
    int shown = 0;                       // 已显示的字符数
    std::wstring footer;                 // 底部操作提示
};

struct SceneSelection {
    std::wstring title;
    std::vector<MenuButton> options;
    bool cancellable = false;            // 关键剧情不可取消
    std::wstring footer;
};

struct SceneInfo {                       // 信息页（help/mission/hint 全屏展示）
    std::wstring title;
    std::vector<std::wstring> lines;
    std::wstring footer;
};

class ConsoleRenderer {
public:
    static const int FRAME_W = 92;       // 标题页与地图共用的最大帧宽
    // 当前可用帧宽（min(92, 窗口宽)），每次渲染前探测，杜绝折行错位
    int frameWidth();

    void renderTitle(const SceneTitle& s);
    void renderRoom(const SceneRoom& s);
    void renderCombat(const SceneCombat& s);
    void renderNarrative(const SceneNarrative& s);
    void renderSelection(const SceneSelection& s);
    void renderInfo(const SceneInfo& s);
    void renderGameOver(const std::vector<std::wstring>& lines);

    // 整帧写入：光标回左上、逐字符着色、每行补空格、隐藏光标
    void flush(const std::vector<std::pair<std::wstring, int>>& rows);

private:
    int lastHeight_ = 0;
};
