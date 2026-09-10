// CommandParser.cpp —— 英文命令解析
#include "CommandParser.h"

#include <algorithm>
#include <cctype>
#include <unordered_set>

namespace {
    const std::vector<std::string> kVerbs = {
        "look", "mission", "help", "inventory", "inv", "status",
        "inspect", "talk", "open", "read", "take", "access", "attack",
        "purify", "control", "use", "guard", "save", "load", "quit",
    };
}

const std::vector<std::string>& knownVerbs() {
    return kVerbs;
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
    for (char c : s) {
        if ((unsigned char)c >= 0x80) return true;
    }
    return false;
}

int CommandParser::editDistance(const std::string& a, const std::string& b) {
    if (a.size() > b.size()) return editDistance(b, a);
    std::vector<int> prev(a.size() + 1), cur(a.size() + 1);
    for (size_t i = 0; i <= a.size(); ++i) prev[i] = (int)i;
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
    int best = 999;
    std::string bestVerb;
    for (const std::string& v : kVerbs) {
        int d = editDistance(verb, v);
        if (d < best) { best = d; bestVerb = v; }
    }
    if (best <= 2) return bestVerb;
    return std::nullopt;
}

ParsedCommand CommandParser::parse(const std::string& raw) {
    ParsedCommand pc;
    std::string s = normalize(raw);
    if (s.empty()) {
        pc.error = L"空命令。输入 help 查看命令列表。";
        return pc;
    }
    if (containsNonAscii(s)) {
        pc.chinese = true;
        pc.error = L"本游戏不接受中文命令。\nEnglish commands only.\n\n你也可以靠近对象后直接按数字进行交互。";
        return pc;
    }
    size_t pos = 0;
    while (pos < s.size()) {
        size_t nxt = s.find(' ', pos);
        if (nxt == std::string::npos) nxt = s.size();
        std::string tok = s.substr(pos, nxt - pos);
        if (!tok.empty()) {
            if (pc.verb.empty()) pc.verb = tok;
            else pc.args.push_back(tok);
        }
        pos = nxt + 1;
    }
    // 检查动词是否已知
    static const std::unordered_set<std::string> verbSet(kVerbs.begin(), kVerbs.end());
    if (!verbSet.count(pc.verb)) {
        auto sug = suggest(pc.verb);
        pc.ok = false;
        pc.error = sug ? std::wstring(L"未知命令。") + L" 你要找的是 " +
                             std::wstring(sug->begin(), sug->end()) + L" 吗？" :
                         L"未知命令。输入 help 查看命令列表。";
        return pc;
    }
    pc.ok = true;
    return pc;
}
