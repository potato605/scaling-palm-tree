// CommandParser.cpp —— 英文命令解析
#include "CommandParser.h"

#include <algorithm>
#include <cctype>
#include <numeric>
#include <sstream>
#include <unordered_set>

namespace {
    const std::vector<std::string> kVerbs = {
        "look", "mission", "help", "inventory", "inv", "status",
        "inspect", "talk", "open", "read", "take", "access", "attack",
        "purify", "control", "use", "guard", "save", "load", "quit",
    };
}

std::string CommandParser::normalize(const std::string& raw) {
    std::string out;
    out.reserve(raw.size());
    bool lastSpace = true;   // 去首部空格
    for (char c : raw) {
        if (c == ' ' || c == '\t') {
            if (!lastSpace) { out.push_back(' '); lastSpace = true; }
        } else {
            out.push_back((char)std::tolower((unsigned char)c));
            lastSpace = false;
        }
    }
    while (!out.empty() && out.back() == ' ') out.pop_back();
    return out;
}

bool CommandParser::containsNonAscii(const std::string& s) {
    return std::any_of(s.begin(), s.end(),
                       [](unsigned char c) { return c >= 0x80; });
}

int CommandParser::editDistance(const std::string& a, const std::string& b) {
    if (a.size() > b.size()) return editDistance(b, a);
    std::vector<int> prev(a.size() + 1), cur(a.size() + 1);
    std::iota(prev.begin(), prev.end(), 0);
    for (size_t j = 1; j <= b.size(); ++j) {
        cur[0] = (int)j;
        for (size_t i = 1; i <= a.size(); ++i) {
            int cost = a[i - 1] == b[j - 1] ? 0 : 1;
            cur[i] = std::min({ prev[i] + 1, cur[i - 1] + 1, prev[i - 1] + cost });
        }
        prev = cur;
    }
    return prev[a.size()];
}

std::optional<std::string> CommandParser::suggest(const std::string& verb) {
    auto it = std::min_element(kVerbs.begin(), kVerbs.end(),
        [&](const std::string& x, const std::string& y) {
            return editDistance(verb, x) < editDistance(verb, y);
        });
    if (it == kVerbs.end()) return std::nullopt;
    return editDistance(verb, *it) <= 2 ? std::optional<std::string>(*it) : std::nullopt;
}

ParsedCommand CommandParser::parse(const std::string& raw) {
    ParsedCommand pc;
    const std::string s = normalize(raw);
    if (s.empty()) {
        pc.error = L"空命令。输入 help 查看命令列表。";
        return pc;
    }
    if (containsNonAscii(s)) {
        pc.chinese = true;
        pc.error = L"本游戏不接受中文命令。\nEnglish commands only.\n\n你也可以靠近对象后直接按数字进行交互。";
        return pc;
    }

    // 拆词：首个为动词，其余为参数
    std::istringstream iss(s);
    for (std::string tok; iss >> tok;) {
        if (pc.verb.empty()) pc.verb = tok;
        else pc.args.push_back(tok);
    }

    static const std::unordered_set<std::string> verbSet(kVerbs.begin(), kVerbs.end());
    if (!verbSet.count(pc.verb)) {
        auto sug = suggest(pc.verb);
        pc.error = sug ? std::wstring(L"未知命令。") + L" 你要找的是 " +
                             std::wstring(sug->begin(), sug->end()) + L" 吗？" :
                         L"未知命令。输入 help 查看命令列表。";
        return pc;
    }
    pc.ok = true;
    return pc;
}
