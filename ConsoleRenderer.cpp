// ConsoleRenderer.cpp —— 无闪烁整帧渲染实现（光标定位 + 尾部补空格 + 逐字符着色）
#include "ConsoleRenderer.h"

#include <cstdio>

namespace ConsoleUtil {

    void init() {
        // UTF-8 代码页（配合 WriteConsoleW 与 /utf-8 编译）
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);
        hideCursor(true);

        // 自动调整窗口尺寸：保证地图、状态、图例和交互区完整显示。
        // 不要求全屏，也不提示玩家调整——程序自己适配。
        HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
        if (!h || h == INVALID_HANDLE_VALUE) return;
        CONSOLE_SCREEN_BUFFER_INFO bi;
        if (!GetConsoleScreenBufferInfo(h, &bi)) return;
        int winW = bi.srWindow.Right - bi.srWindow.Left + 1;
        int winH = bi.srWindow.Bottom - bi.srWindow.Top + 1;
        if (winW < 96 || winH < 40) {
            // 先扩缓冲（新缓冲宽必须 >= 当前窗口宽）
            int bw = winW < 96 ? 96 : winW;
            int bh = winH < 40 ? 40 : winH;
            COORD sz = { (SHORT)bw, (SHORT)bh };
            if (SetConsoleScreenBufferSize(h, sz)) {
                SMALL_RECT w = { 0, 0, (SHORT)(bw - 1), (SHORT)(bh - 1) };
                SetConsoleWindowInfo(h, TRUE, &w);
            }
        }
    }

    void hideCursor(bool hide) {
        HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
        if (!h || h == INVALID_HANDLE_VALUE) return;
        CONSOLE_CURSOR_INFO info;
        GetConsoleCursorInfo(h, &info);
        info.bVisible = hide ? FALSE : TRUE;
        SetConsoleCursorInfo(h, &info);
    }

    void setColor(int fg, int bg) {
        HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
        if (!h || h == INVALID_HANDLE_VALUE) return;
        SetConsoleTextAttribute(h, (WORD)((fg & 0x0F) | ((bg & 0x0F) << 4)));
    }

    void setCursorPos(int x, int y) {
        HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
        if (!h || h == INVALID_HANDLE_VALUE) return;
        COORD c = { (SHORT)x, (SHORT)y };
        SetConsoleCursorPosition(h, c);
    }

    int windowWidth() {
        CONSOLE_SCREEN_BUFFER_INFO info;
        HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
        if (!h || h == INVALID_HANDLE_VALUE) return 120;
        GetConsoleScreenBufferInfo(h, &info);
        return info.dwSize.X;
    }

    int windowHeight() {
        CONSOLE_SCREEN_BUFFER_INFO info;
        HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
        if (!h || h == INVALID_HANDLE_VALUE) return 30;
        GetConsoleScreenBufferInfo(h, &info);
        return info.srWindow.Bottom - info.srWindow.Top + 1;
    }
}

namespace ConsoleUtil {
    bool isWideChar(wchar_t c) {
        // 东亚宽字符区间（中文/日文假名/韩文/全角符号等）
        return (c >= 0x1100 && c <= 0x115F) ||   // Hangul Jamo
               (c >= 0x2E80 && c <= 0xA4CF) ||   // CJK 部首、假名、CJK 符号、汉字区
               (c >= 0xAC00 && c <= 0xD7A3) ||   // Hangul
               (c >= 0xF900 && c <= 0xFAFF) ||   // CJK 兼容汉字
               (c >= 0xFE10 && c <= 0xFE6F) ||   // 竖排符号
               (c >= 0xFF00 && c <= 0xFF60) ||   // 全角形式
               (c >= 0xFFE0 && c <= 0xFFE6) ||
               (c >= 0x20000 && c <= 0x2FFFD) ||
               c == 0x3000;                      // 全角空格
    }

    int displayWidth(const std::wstring& s) {
        int w = 0;
        for (wchar_t c : s) w += isWideChar(c) ? 2 : 1;
        return w;
    }

    std::wstring fitLine(const std::wstring& s, int maxCells) {
        int w = 0;
        size_t upto = s.size();
        bool tooLong = false;
        for (size_t i = 0; i < s.size(); ++i) {
            w += isWideChar(s[i]) ? 2 : 1;
            if (w > maxCells) { upto = i; tooLong = true; break; }
        }
        if (!tooLong) return s;
        std::wstring cut = s.substr(0, upto);
        if (displayWidth(cut) > maxCells - 1) cut.pop_back();
        cut += L"…";
        // 确保结尾不超宽
        while (displayWidth(cut) > maxCells) cut.pop_back();
        return cut;
    }

    std::wstring padTo(const std::wstring& s, int cells) {
        std::wstring out = fitLine(s, cells);
        int need = cells - displayWidth(out);
        if (need > 0) out.append(need, L' ');
        return out;
    }

    std::vector<std::wstring> wrapLine(const std::wstring& s, int maxCells) {
        std::vector<std::wstring> out;
        std::wstring cur;
        int w = 0;
        for (wchar_t c : s) {
            int cw = isWideChar(c) ? 2 : 1;
            if (w + cw > maxCells && w > 0) {
                out.push_back(cur);
                cur.clear();
                w = 0;
            }
            cur.push_back(c);
            w += cw;
        }
        out.push_back(cur);
        return out;
    }
}

namespace {
    HANDLE console() { return GetStdHandle(STD_OUTPUT_HANDLE); }

    // 每行统一补到 FRAME_W 宽（按显示单元格，尾部空格覆盖旧内容）
    std::wstring padWidth(const std::wstring& s, int width) {
        return ConsoleUtil::padTo(s, width);
    }

    // 写入一个字符
    void writeCh(HANDLE h, wchar_t ch, int fg) {
        ConsoleUtil::setColor(fg, 0);
        DWORD w = 0;
        WriteConsoleW(h, &ch, 1, &w, nullptr);
    }

    // 按钮行文本
    std::wstring buttonText(const MenuButton& b) {
        std::wstring line;
        if (b.n > 0) {
            wchar_t num[8];
            swprintf_s(num, L"[%d] ", b.n);
            line += num;
        }
        line += b.label;
        if (!b.enabled && !b.reason.empty()) {
            line += L"（";
            line += b.reason;
            line += L"）";
        }
        return line;
    }

    std::wstring centerText(const std::wstring& text, int width) {
        std::wstring fitted = ConsoleUtil::fitLine(text, width);
        int gap = width - ConsoleUtil::displayWidth(fitted);
        if (gap < 0) gap = 0;
        int left = gap / 2;
        return std::wstring(left, L' ') + fitted + std::wstring(gap - left, L' ');
    }

    // 字标按整块宽度居中，避免不同高度行因自身长度不同而产生横向漂移。
    std::wstring centerBlockLine(const std::wstring& text, int blockWidth, int width) {
        std::wstring line = ConsoleUtil::fitLine(text, blockWidth);
        int right = blockWidth - ConsoleUtil::displayWidth(line);
        if (right > 0) line.append(right, L' ');
        int outer = width - blockWidth;
        if (outer < 0) outer = 0;
        int left = outer / 2;
        return std::wstring(left, L' ') + line + std::wstring(outer - left, L' ');
    }

    std::wstring frameBorder(int width, wchar_t left, wchar_t fill, wchar_t right) {
        return std::wstring(1, left) + std::wstring(width - 2, fill) + std::wstring(1, right);
    }

    std::wstring frameContent(const std::wstring& text, int width) {
        return L"│" + ConsoleUtil::padTo(text, width - 2) + L"│";
    }

    std::wstring frameRule(const std::wstring& label, int width) {
        const int inner = width - 2;
        std::wstring lead = L"─ " + label + L" ";
        int remaining = inner - ConsoleUtil::displayWidth(lead);
        if (remaining < 1) return frameBorder(width, L'├', L'─', L'┤');
        return L"├" + lead.substr(0) + std::wstring(remaining, L'─') + L"┤";
    }
}

// ---------- 当前可用帧宽 ----------
int ConsoleRenderer::frameWidth() {
    CONSOLE_SCREEN_BUFFER_INFO info;
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (!h || h == INVALID_HANDLE_VALUE) return FRAME_W;
    GetConsoleScreenBufferInfo(h, &info);
    int w = info.srWindow.Right - info.srWindow.Left + 1;
    if (w < 60) w = 60;             // 极端窄窗口：宁可截断内容，也不折行破坏布局
    return w < FRAME_W ? w : FRAME_W;
}

// ---------- 通用整帧文字行 ----------
// 注意：Windows 控制台只有写满缓冲宽度才会自动换行（且宽字符占 2 列），
// 因此每一行都必须显式 SetCursorPosition(0, y)，绝不依赖自动折行推进。
void ConsoleRenderer::flush(const std::vector<std::pair<std::wstring, int>>& rows) {
    HANDLE h = console();
    if (!h || h == INVALID_HANDLE_VALUE) return;
    ConsoleUtil::hideCursor(true);
    const int fw = frameWidth();
    int y = 0;
    for (const auto& [text, color] : rows) {
        ConsoleUtil::setCursorPos(0, y);
        ConsoleUtil::setColor(color, 0);
        DWORD w = 0;
        std::wstring line = padWidth(text, fw);
        WriteConsoleW(h, line.c_str(), (DWORD)line.size(), &w, nullptr);
        ++y;
    }
    while (y < lastHeight_) {       // 新帧更矮时清掉旧行
        ConsoleUtil::setCursorPos(0, y);
        ConsoleUtil::setColor(7, 0);
        DWORD w = 0;
        std::wstring blank = padWidth(L"", fw);
        WriteConsoleW(h, blank.c_str(), (DWORD)blank.size(), &w, nullptr);
        ++y;
    }
    lastHeight_ = (int)rows.size();
    ConsoleUtil::setColor(7, 0);
}

// ---------- 房间帧：标题行 + 地图网格（逐格着色）+ 状态区 ----------
void ConsoleRenderer::renderRoom(const SceneRoom& s) {
    HANDLE h = console();
    if (!h || h == INVALID_HANDLE_VALUE) return;
    ConsoleUtil::hideCursor(true);
    ConsoleUtil::setCursorPos(0, 0);

    const int fw = frameWidth();
    int y = 0;
    auto outLine = [&](const std::wstring& text, int color) {
        ConsoleUtil::setCursorPos(0, y);
        ConsoleUtil::setColor(color, 0);
        DWORD w = 0;
        std::wstring line = padWidth(text, fw);
        WriteConsoleW(h, line.c_str(), (DWORD)line.size(), &w, nullptr);
        ++y;
    };

    outLine(L"═══ " + s.titleLine + L" ═══", 10);
    outLine(L"", 7);

    // 地图网格（逐格着色；列数限制在窗口宽内，窄窗口自动截断地图多余列）
    const int drawW = s.map.w < fw ? s.map.w : fw;
    for (int gy = 0; gy < s.map.h; ++gy) {
        ConsoleUtil::setCursorPos(0, y);
        const auto& row = s.map.rows[gy];
        for (int gx = 0; gx < drawW; ++gx) {
            const FrameCell& c = row[gx];
            writeCh(h, c.ch == L' ' ? L' ' : c.ch, c.ch == L' ' ? 7 : c.fg);
        }
        if (drawW < fw) {
            DWORD w = 0;
            ConsoleUtil::setColor(7, 0);
            std::wstring rest = padWidth(L"", fw - drawW);
            WriteConsoleW(h, rest.c_str(), (DWORD)rest.size(), &w, nullptr);
        }
        ++y;
    }

    outLine(s.hudLine, 7);
    outLine(s.backpackLine, 10);
    outLine(s.objectiveLine, 11);
    for (const auto& line : s.legendLines) outLine(line, 8);
    outLine(s.suggestionLine, 14);
    // 两行固定分隔线，让地图信息与数字交互区保持清晰层次。
    outLine(L"────────────────────────────────────────────────────────────────────", 8);
    outLine(L"────────────────────────────────────────────────────────────────────", 8);
    for (const auto& mv : s.messages) {
        outLine(mv.first, mv.second);
    }
    for (const auto& b : s.buttons) {
        outLine(buttonText(b), b.enabled ? 14 : 8);
    }
    if (!s.inputLine.empty()) {
        outLine(s.inputLine, 15);
    }

    while (y < lastHeight_) {       // 清旧
        outLine(L"", 7);
    }
    lastHeight_ = y;
    ConsoleUtil::setColor(7, 0);
}

void ConsoleRenderer::renderCombat(const SceneCombat& s) {
    std::vector<std::pair<std::wstring, int>> rows;
    rows.push_back({ L"═⚔ 战斗 ⚔═", 12 });
    rows.push_back({ s.targetLine, 12 });
    rows.push_back({ s.targetHpLine, 12 });
    rows.push_back({ s.playerLine, 7 });
    rows.push_back({ s.hintLine, 14 });
    rows.push_back({ L"", 7 });
    for (const auto& mv : s.messages) rows.push_back(mv);
    rows.push_back({ L"", 7 });
    for (const auto& b : s.buttons) rows.push_back({ buttonText(b), b.enabled ? 14 : 8 });
    flush(rows);
}

void ConsoleRenderer::renderNarrative(const SceneNarrative& s) {
    std::vector<std::pair<std::wstring, int>> rows;
    rows.push_back({ L"┄┄┄ 剧情 ┄┄┄", 11 });
    rows.push_back({ L"", 7 });
    // 已完成行：软换行（长对白不截断、不折帧）
    const int fw = frameWidth();
    size_t prevLimit = 6;
    for (const auto& line : s.previous) {
        if (prevLimit == 0) break;
        --prevLimit;
        auto wrapped = ConsoleUtil::wrapLine(line, fw);
        for (const auto& wl : wrapped) rows.push_back({ wl, 8 });
    }
    // 当前行：按已显示字符量软换行
    std::wstring cur = s.current.substr(0, s.shown);
    auto curWrapped = ConsoleUtil::wrapLine(cur, fw);
    for (const auto& wl : curWrapped) rows.push_back({ wl, 15 });
    rows.push_back({ L"", 7 });
    rows.push_back({ s.footer, 14 });
    flush(rows);
}

void ConsoleRenderer::renderSelection(const SceneSelection& s) {
    std::vector<std::pair<std::wstring, int>> rows;
    rows.push_back({ L"✦ " + s.title, 11 });
    rows.push_back({ L"", 7 });
    for (const auto& b : s.options) rows.push_back({ buttonText(b), b.enabled ? 14 : 8 });
    rows.push_back({ L"", 7 });
    if (!s.footer.empty()) rows.push_back({ s.footer, 14 });
    if (s.cancellable) rows.push_back({ L"[0] 取消", 8 });
    flush(rows);
}

void ConsoleRenderer::renderInfo(const SceneInfo& s) {
    std::vector<std::pair<std::wstring, int>> rows;
    rows.push_back({ L"■ " + s.title, 11 });
    rows.push_back({ L"", 7 });
    for (const auto& line : s.lines) rows.push_back({ line, 7 });
    rows.push_back({ L"", 7 });
    rows.push_back({ s.footer, 14 });
    flush(rows);
}

void ConsoleRenderer::renderTitle(const SceneTitle& s) {
    std::vector<std::pair<std::wstring, int>> rows;
    const int fw = frameWidth();
    const int inner = fw - 2;
    auto addFrame = [&](const std::wstring& text, int color) {
        rows.push_back({ frameContent(text, fw), color });
    };
    auto addRule = [&](const std::wstring& label) {
        rows.push_back({ frameRule(label, fw), 10 });
    };

    rows.push_back({ frameBorder(fw, L'┌', L'─', L'┐'), 10 });
    int artWidth = 0;
    for (const auto& line : s.art)
        artWidth = std::max(artWidth, ConsoleUtil::displayWidth(line));
    artWidth = std::min(artWidth, inner);
    for (size_t i = 0; i < s.art.size(); ++i) {
        int color = 10;
        if (i + 1 == s.art.size()) color = 14;
        else if (i + 2 == s.art.size()) color = 11;
        const bool footerLabel = i + 2 >= s.art.size();
        addFrame(footerLabel ? centerText(s.art[i], inner)
                             : centerBlockLine(s.art[i], artWidth, inner), color);
    }
    addRule(L"SECURE FACILITY / 安全终端");

    const bool teamPage = !s.buttons.empty() && s.buttons.front().n == 0;
    if (teamPage) {
        for (size_t i = 0; i < s.caption.size(); ++i)
            addFrame(centerText(s.caption[i], inner), i == 0 ? 14 : 7);
        addRule(L"RETURN / 返回");
        addFrame(centerText(L"按任意数字键返回标题", inner), 14);
    } else {
        for (size_t i = 0; i < s.caption.size(); ++i)
            addFrame(centerText(s.caption[i], inner), i == 0 ? 14 : 7);

        addRule(L"COMMAND GUIDE / 指令提示");
        addFrame(L"W/A/S/D 移动   ·   数字交互   ·   H 查看提示", 15);
        addFrame(L"F5 快速存档   ·   Q 退出   ·   地图下方显示图例", 15);

        addRule(L"SYSTEM MESSAGE / 系统提示");
        addFrame(L"[!] 欢迎回来，研究员。", 10);
        addFrame(L"目标：调查污染源 / 收集三份样本 / 处置厄生", 7);

        addRule(L"MAIN MENU / 主菜单");
        for (const auto& b : s.buttons) {
            std::wstring line = buttonText(b);
            if (b.n == 1) line += L"  |  开始一段新的调查";
            else if (b.n == 2) line += L"  |  读取已有进度";
            else if (b.n == 3) line += L"  |  离开终端";
            else if (b.n == 4) line += L"  |  查看项目成员";
            addFrame(line, b.enabled ? 14 : 8);
        }
        addRule(L"APF-X00  ·  ECO RESTRICTED ZONE");
    }
    flush(rows);
}

void ConsoleRenderer::renderGameOver(const std::vector<std::wstring>& lines) {
    std::vector<std::pair<std::wstring, int>> rows;
    rows.push_back({ L"✖ 你倒下了 ✖", 12 });
    rows.push_back({ L"", 7 });
    for (const auto& line : lines) rows.push_back({ line, 7 });
    rows.push_back({ L"", 7 });
    rows.push_back({ L"按任意键返回检查点……", 14 });
    flush(rows);
}
