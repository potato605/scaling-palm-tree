// Monster.cpp —— 怪物处置状态与地图符号
#include "Monster.h"

const wchar_t* dispositionGlyph(Monster::Disposition d) {
    switch (d) {
        case Monster::Disposition::Alive:      return L"M";   // 未处置（BOSS 为 X）
        case Monster::Disposition::Weakened:   return L"M";
        case Monster::Disposition::Purified:   return L"+";   // 已净化
        case Monster::Disposition::Controlled: return L"R";   // 已控制
        case Monster::Disposition::Eliminated: return L"*";   // 已肃清
    }
    return L"M";
}

const std::string dispositionName(Monster::Disposition d) {
    switch (d) {
        case Monster::Disposition::Alive:      return "alive";
        case Monster::Disposition::Weakened:   return "weakened";
        case Monster::Disposition::Purified:   return "purified";
        case Monster::Disposition::Controlled: return "controlled";
        case Monster::Disposition::Eliminated: return "eliminated";
    }
    return "alive";
}
