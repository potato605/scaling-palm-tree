// CommandParser.h —— 英文命令解析：去空、小写、拒绝中文、拼写建议
#pragma once

#include <optional>
#include <string>
#include <vector>

struct ParsedCommand {
    bool ok = false;            // 解析成功
    bool chinese = false;       // 含中文（拒绝）
    std::string verb;
    std::vector<std::string> args;
    std::wstring error;         // 解析错误信息
};

// 已知动词表（CommandParser.cpp 定义）
extern const std::vector<std::string>& knownVerbs();

namespace CommandParser {
    // 去首尾空格、合并连续空格、转小写
    std::string normalize(const std::string& raw);
    bool containsNonAscii(const std::string& s);
    ParsedCommand parse(const std::string& raw);
    // 拼写错误建议（编辑距离 ≤ 2），无则返回空
    std::optional<std::string> suggest(const std::string& verb);
    // 编辑距离
    int editDistance(const std::string& a, const std::string& b);
}
