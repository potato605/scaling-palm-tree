// SaveSystem.cpp —— 版本化文本存档：写入/读取/校验/损坏保护
#include "SaveSystem.h"
#include "GameState.h"
#include "World.h"
#include "Item.h"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

std::string SaveSystem::defaultPath() {
    return "saves" + std::string(1, '/') + "slot1.txt";
}

bool SaveSystem::exists(const std::string& path) {
    std::error_code ec;
    return fs::exists(fs::path(path), ec);
}

// ---------- 写入 ----------
static void appendSet(std::ostringstream& os, const std::string& key, const std::unordered_set<std::string>& set) {
    os << key << "=";
    bool first = true;
    for (const auto& s : set) {
        if (!first) os << ",";
        os << s;
        first = false;
    }
    os << "\n";
}

std::string SaveSystem::save(const GameState& st, const World& world, const std::string& path) {
    std::error_code ec;
    fs::path p(path);
    if (p.has_parent_path()) fs::create_directories(p.parent_path(), ec);

    std::ostringstream os;
    os << "# EcoRestrictedZone v1 save\n";
    os << "version=" << SAVE_VERSION << "\n";
    os << "room=" << st.roomId << "\n";
    os << "px=" << st.px << "\n";
    os << "py=" << st.py << "\n";
    os << "hp=" << st.hp << "\n";
    os << "maxhp=" << st.maxHp << "\n";
    os << "attack=" << st.attack << "\n";
    os << "defense=" << st.defense << "\n";
    os << "level=" << st.level << "\n";
    os << "exp=" << st.exp << "\n";
    os << "questStage=" << st.questStage << "\n";
    os << "checkpointRoom=" << st.checkpointRoom << "\n";
    os << "checkpointX=" << st.checkpointX << "\n";
    os << "checkpointY=" << st.checkpointY << "\n";
    os << "elapsed=" << st.elapsedSeconds << "\n";
    os << "deaths=" << st.deaths << "\n";
    os << "eliminate=" << st.eliminateCount << "\n";
    os << "purify=" << st.purifyCount << "\n";
    os << "control=" << st.controlCount << "\n";
    os << "correct=" << st.correctAnswers << "\n";
    os << "archives=" << st.archivesRead << "\n";
    // 最终处置用 ASCII 键值保存（读取时映射回中文显示）
    {
        std::string finalAsc = "-";
        if (st.finalDisposition == L"肃清") finalAsc = "eliminate";
        else if (st.finalDisposition == L"净化") finalAsc = "purify";
        else if (st.finalDisposition == L"控制") finalAsc = "control";
        os << "final=" << finalAsc << "\n";
    }
    // 物品与关键物品
    {
        os << "keys=";
        bool first = true;
        for (const auto& k : st.keyItems) { if (!first) os << ","; os << k; first = false; }
        os << "\n";
    }
    {
        os << "items=";
        bool first = true;
        for (const auto& [id, n] : st.items) { if (!first) os << ","; os << id << ":" << n; first = false; }
        os << "\n";
    }
    appendSet(os, "flags", st.flags);
    appendSet(os, "quiz", st.quizDone);
    appendSet(os, "archives", st.archivesDone);
    {
        os << "monsters=";
        bool first = true;
        for (const auto& [id, disp] : st.monsterStates) {
            if (!first) os << ",";
            os << id << ":" << disp;
            first = false;
        }
        os << "\n";
    }

    std::ofstream ofs(p, std::ios::binary);
    if (!ofs) return "无法写入存档文件。";
    ofs << os.str();
    if (!ofs) return "写档失败（磁盘/权限）。";
    return "";
}

// ---------- 解析辅助 ----------
const char* SaveSystem::parseLine(const std::string& line, std::string& key, std::string& value) {
    if (line.empty() || line[0] == '#' || line[0] == '\r') return "";
    size_t eq = line.find('=');
    if (eq == std::string::npos) return "";
    key = line.substr(0, eq);
    value = line.substr(eq + 1);
    if (!value.empty() && value.back() == '\r') value.pop_back();
    return "";
}

std::string SaveSystem::validate(const GameState& st, const World& world) {
    const Room* room = world.room(st.roomId);
    if (!room) return "存档损坏：未知房间。";
    if (st.px < 0 || st.py < 0 || st.px >= room->width || st.py >= room->height)
        return "存档损坏：坐标越界。";
    const Tile& t = room->tileAt(st.px, st.py);
    if (!t.passable)
        return "存档损坏：玩家在墙内。";
    if (t.ch == L'L')
        return "存档损坏：玩家在锁门中。";
    if (st.questStage < 1 || st.questStage > 12)
        return "存档损坏：任务阶段异常。";
    if (st.hp < 1 || st.hp > st.maxHp)
        return "存档损坏：生命值异常。";
    return "";
}

// ---------- 读取 ----------
std::string SaveSystem::load(GameState& st, World& world, const std::string& path) {
    std::ifstream ifs(fs::path(path), std::ios::binary);
    if (!ifs) return "找不到存档文件。";

    std::unordered_map<std::string, std::string> kv;
    std::string line;
    while (std::getline(ifs, line)) {
        std::string key, value;
        parseLine(line, key, value);
        if (!key.empty()) kv[key] = value;
    }

    auto has = [&](const std::string& k) { return kv.count(k) > 0; };
    auto geti = [&](const std::string& k, int def) {
        auto it = kv.find(k);
        return it == kv.end() ? def : std::atoi(it->second.c_str());
    };

    // 版本/关键字段检查
    if (kv.count("version") == 0 || kv["version"] != SAVE_VERSION)
        return "存档损坏：版本不匹配。";
    if (!has("room") || !has("px") || !has("py"))
        return "存档损坏：缺少字段。";

    GameState loaded = st;                      // 从标题读档时取默认
    loaded.roomId = geti("room", 0);
    loaded.px = geti("px", 1);
    loaded.py = geti("py", 1);
    loaded.hp = geti("hp", 120);
    loaded.maxHp = geti("maxhp", 120);
    loaded.attack = geti("attack", 18);
    loaded.defense = geti("defense", 6);
    loaded.level = geti("level", 1);
    loaded.exp = geti("exp", 0);
    loaded.questStage = geti("questStage", 1);
    loaded.checkpointRoom = geti("checkpointRoom", 0);
    loaded.checkpointX = geti("checkpointX", 1);
    loaded.checkpointY = geti("checkpointY", 1);
    loaded.elapsedSeconds = geti("elapsed", 0);
    loaded.deaths = geti("deaths", 0);
    loaded.eliminateCount = geti("eliminate", 0);
    loaded.purifyCount = geti("purify", 0);
    loaded.controlCount = geti("control", 0);
    loaded.correctAnswers = geti("correct", 0);
    loaded.archivesRead = geti("archives", 0);

    // 列表字段
    auto parseCsvSet = [](const std::string& s, std::unordered_set<std::string>& out) {
        size_t pos = 0;
        while (pos < s.size()) {
            size_t nxt = s.find(',', pos);
            if (nxt == std::string::npos) nxt = s.size();
            if (nxt > pos) out.insert(s.substr(pos, nxt - pos));
            pos = nxt + 1;
        }
    };
    if (has("keys")) parseCsvSet(kv["keys"], loaded.keyItems);
    if (has("flags")) parseCsvSet(kv["flags"], loaded.flags);
    if (has("quiz")) parseCsvSet(kv["quiz"], loaded.quizDone);
    if (has("archives")) parseCsvSet(kv["archives"], loaded.archivesDone);
    if (has("items")) {
        size_t pos = 0;
        const std::string& s = kv["items"];
        while (pos < s.size()) {
            size_t comma = s.find(',', pos), colon = s.find(':', pos);
            if (comma == std::string::npos) comma = s.size();
            if (colon == std::string::npos || colon > comma) { pos = comma + 1; continue; }
            loaded.items[s.substr(pos, colon - pos)] = std::atoi(s.substr(colon + 1, comma - colon - 1).c_str());
            pos = comma + 1;
        }
    }
    if (has("monsters")) {
        size_t pos = 0;
        const std::string& s = kv["monsters"];
        while (pos < s.size()) {
            size_t comma = s.find(',', pos), colon = s.find(':', pos);
            if (comma == std::string::npos) comma = s.size();
            if (colon == std::string::npos || colon > comma) { pos = comma + 1; continue; }
            loaded.monsterStates[s.substr(pos, colon - pos)] = s.substr(colon + 1, comma - colon - 1);
            pos = comma + 1;
        }
    }
    if (has("final")) {
        std::string f = kv["final"];
        if (f == "eliminate") loaded.finalDisposition = L"肃清";
        else if (f == "purify") loaded.finalDisposition = L"净化";
        else if (f == "control") loaded.finalDisposition = L"控制";
    }

    // 校验：房间/坐标/检查点
    std::string err = validate(loaded, world);
    if (!err.empty()) return err;
    if (loaded.checkpointRoom < 0 || loaded.checkpointRoom >= 12)
        return "存档损坏：检查点异常。";
    // 怪物状态应用（已处置不复活；存活的按满血出现）
    for (auto& r : world.rooms()) {
        for (auto& m : r->monsters) {
            auto it = loaded.monsterStates.find(m->id);
            if (it == loaded.monsterStates.end()) continue;
            const std::string& s = it->second;
            if (s == "purified") m->disp = Monster::Disposition::Purified;
            else if (s == "controlled") m->disp = Monster::Disposition::Controlled;
            else if (s == "eliminated") m->disp = Monster::Disposition::Eliminated;
            else if (s == "weakened") { m->disp = Monster::Disposition::Weakened; m->hp = m->maxHp * 3 / 10; }
            else if (s == "alive") { m->disp = Monster::Disposition::Alive; m->hp = m->maxHp; }
        }
    }

    st = loaded;
    return "";
}
