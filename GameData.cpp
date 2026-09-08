// GameData.cpp —— 数据中枢：12 个房间地图、NPC、怪物、题目、剧情、档案、结局
#include "GameData.h"

#include <algorithm>
#include <stdexcept>

// ============================================================
// 地图以"边框 + 装饰块"方式程序化构建（行宽天然正确，无手串字符误差）
// y=7 为门通行行：西门 x=0、东门 x=W-1
// ============================================================
namespace {

// ------- 地图构建器 -------
struct MapBuilder {
    int w = 68, h = 15;
    std::vector<std::string> g;
    MapBuilder(int ww, int hh) : w(ww), h(hh), g(hh, std::string(ww, '.')) {
        for (int x = 0; x < w; ++x) { g[0][x] = '#'; g[h - 1][x] = '#'; }
        for (int y = 0; y < h; ++y) { g[y][0] = '#'; g[y][w - 1] = '#'; }
    }
    void put(char c, int x, int y) { g[y][x] = c; }
    void rect(char c, int x0, int y0, int x1, int y1) {
        for (int y = y0; y <= y1; ++y)
            for (int x = x0; x <= x1; ++x) g[y][x] = c;
    }
    void clearRect(int x0, int y0, int x1, int y1) { rect('.', x0, y0, x1, y1); }
};

// 字符 → Tile 属性
int colorOf(wchar_t ch) {
    switch (ch) {
        case L'#': return 8;   // 灰墙
        case L'~': return 9;   // 蓝污染水
        case L'*': return 12;  // 红玻璃
        case L'T': return 11;
        case L'C': return 9;
        default:  return 7;
    }
}
bool passableOf(wchar_t ch) {
    switch (ch) {
        case L'#': return false;
        case L'T': case L'C': return false;
        default: return true;
    }
}
Tile tileFrom(wchar_t ch) {
    Tile t;
    t.ch = ch;
    t.fg = colorOf(ch);
    t.passable = passableOf(ch);
    if (ch == L'~') { t.dangerous = true; t.dangerDmg = 12; }
    if (ch == L'*') { t.dangerous = true; t.dangerDmg = 10; }
    if (ch == L'N' || ch == L'M' || ch == L'X' || ch == L'I' || ch == L'S') {
        // 实体层：网格保持可通行，阻挡由实体判定（见 Game::tryStep）
        t.ch = L'.';
        t.passable = true;
    }
    return t;
}

// ------- 建筑辅助 -------
void setGrid(Room* r, const MapBuilder& mb) {
    r->width = mb.w;
    r->height = mb.h;
    r->grid.assign(mb.h, {});
    for (int y = 0; y < mb.h; ++y) {
        r->grid[y].resize(mb.w);
        for (int x = 0; x < mb.w; ++x) {
            wchar_t ch = (unsigned char)mb.g[y][x];
            r->grid[y][x] = tileFrom(ch);
        }
    }
}

void addDoor(Room* r, int x, int y, const std::string& id, const std::string& doorId,
             const std::string& reqKey, int targetRoom, const std::wstring& dirLabel) {
    Interactable d;
    d.id = id;
    d.name = dirLabel + L"防护门";
    d.x = x; d.y = y;
    d.kind = Interactable::Kind::Door;
    d.doorId = doorId;
    d.requiredKey = reqKey;
    d.targetRoom = targetRoom;
    d.dirLabel = dirLabel;
    d.unlocked = reqKey.empty();
    r->interactables.push_back(d);
    Tile& t = r->tileAt(x, y);
    t.ch = d.unlocked ? L'D' : L'L';
    t.passable = d.unlocked;
    t.fg = d.unlocked ? 10 : 12;
}

void addContainer(Room* r, int x, int y, const std::string& id, const std::wstring& name,
                  const std::string& itemId, int count) {
    Interactable c;
    c.id = id;
    c.name = name;
    c.x = x; c.y = y;
    c.kind = Interactable::Kind::Container;
    c.itemId = itemId;
    c.itemCount = count;
    r->interactables.push_back(c);
    Tile& t = r->tileAt(x, y);
    t.ch = L'C';
    t.passable = false;
    t.fg = 9;
}

void addArchive(Room* r, int x, int y, const std::string& id, const std::wstring& name,
                Interactable::Kind kind, const std::vector<std::wstring>& lines) {
    Interactable a;
    a.id = id;
    a.name = name;
    a.x = x; a.y = y;
    a.kind = kind;
    a.archiveLines = lines;
    r->interactables.push_back(a);
    Tile& t = r->tileAt(x, y);
    t.ch = kind == Interactable::Kind::Landmark ? L'T' : L'T';
    t.passable = false;
    t.fg = 11;
}

void addGround(Room* r, int x, int y, const std::string& id, const std::wstring& name,
               const std::string& itemId, int count) {
    Interactable it;
    it.id = id;
    it.name = name;
    it.x = x; it.y = y;
    it.kind = Interactable::Kind::GroundItem;
    it.itemId = itemId;
    it.itemCount = count;
    r->interactables.push_back(it);
}

// 物理阻挡校准：把矩形内实体字符替换为墙（用于"大型障碍"等）
void tagObstacle(Room* r, int x, int y) {
    Tile& t = r->tileAt(x, y);
    t.ch = L'#';
    t.passable = false;
    t.fg = 8;
    t.obstacleLarge = true;
}

// ---------------- 12 房间构建 ----------------
std::unique_ptr<World> buildWorldImpl() {
    auto world = std::make_unique<World>();

    // ============ 0 入口安全厅 ============
    auto r0 = std::make_unique<Room>(0, L"入口安全厅");
    {
        MapBuilder mb(68, 15);
        mb.rect('#', 12, 5, 30, 5);          // 接待柜台（上沿）
        mb.rect('#', 12, 9, 30, 9);          // 接待柜台（下沿）
        setGrid(r0.get(), mb);
        addDoor(r0.get(), 67, 7, "exit_e", "g_0_1", "greenhouse_card", 1, L"东侧");
        addArchive(r0.get(), 20, 7, "task_terminal", L"任务终端",
                   Interactable::Kind::Terminal, {
                       L"〔任务终端〕",
                       L"目标：调查污染源，收集污染样本一/二/三，处置 APF-X00「厄生」。",
                       L"处置三条路线：肃清（消灭）/ 净化（终止痛苦）/ 控制（限制）。",
                       L"权限发放者：林阅(温室卡) → 方晴(实验层) → 赵诚(饲养场) → 陈砚(中央)。",
                   });
        addContainer(r0.get(), 26, 7, "supply_crate", L"补给箱", "nutrient_block", 2);

        auto npc = std::make_unique<NPC>();
        npc->id = "linyue";
        npc->name = L"林阅";
        npc->x = 14; npc->y = 7;
        npc->quizIndex = 0;
        npc->firstLines = {
            L"（林阅在柜台后检查你的证件）Cleaner-07，“清洁者”——好名字，希望你不辱使命。",
            L"我长话短说：阿波菲斯曾为“生态修复”而建，后来捏出了厄生，现在它把整个生态链都搅黄了。",
            L"你要收集三份污染样本，最后一并处置厄生——肃清、净化或控制，三条路你自己选。",
            L"W/A/S/D 移动；靠近目标后屏幕上会列出数字，按数字就能交互；按 Enter 可以输入英文命令。",
            L"你的生命是 120，补给箱里有恢复用品，先去拿了吧。",
            L"进去之前我考你个基础问题。干我们这行，都绕不开《寂静的春天》。你知道它核心控诉的是什么吗？",
        };
        npc->repeatLines = {
            L"（林阅抬了抬下巴）温室卡已经给你了。往东走，先处置温室前廊的双头煞一。",
            L"按数字交互，或输入命令（talk/attack/open…）。看屏幕底部的【任务】行和数字菜单。",
        };
        npc->rewardKey = "greenhouse_card";
        npc->rewardMsg = L"【温室门禁卡】已发放——东侧防护门已解锁。";
        r0->npcs.push_back(std::move(npc));

        r0->introLines = {
            L"安全门的感应灯亮起。这里还保留着事故前的整洁：值班柜台上贴着「生态修复计划」的旧贴纸。",
            L"「一级生态禁区」——你按下了腰间的准录仪。",
        };
    }
    world->addRoom(std::move(r0));

    // ============ 1 温室前廊 ============
    auto r1 = std::make_unique<Room>(1, L"温室前廊");
    {
        MapBuilder mb(68, 15);
        mb.rect('~', 8, 11, 24, 12);         // 污染水
        mb.rect('#', 34, 2, 35, 10);         // 培养槽障碍
        mb.put('.', 34, 7); mb.put('.', 35, 7);   // 通道
        mb.put('C', 46, 7);
        setGrid(r1.get(), mb);
        addDoor(r1.get(), 0, 7, "exit_w", "g_0_1", "greenhouse_card", 0, L"西侧");
        addDoor(r1.get(), 67, 7, "exit_e", "g_1_2", "", 2, L"东侧");
        addContainer(r1.get(), 46, 7, "field_supply", L"固定净化补给", "nutrient_block", 1);

        auto m1 = std::make_unique<Monster>();
        m1->id = "shuang_1"; m1->name = L"双头煞一";
        m1->x = 20; m1->y = 7;
        m1->hp = m1->maxHp = 32;
        m1->attack = 9; m1->defense = 5;
        m1->aggroRange = 3; m1->exp = 25;
        m1->flavor = L"两个脑袋都盯着你，喉管里发出湿漉漉的腔调。";
        m1->weakMsg = L"双头煞一进入【基因衰弱】——现在可以净化或控制它了。";
        r1->monsters.push_back(std::move(m1));

        r1->introLines = {
            L"温室前廊的雾气带着铁锈味。远处一排巨大的培养槽闪着故障灯，地面渗着污染水。",
            L"靠近怪物会触发战斗——如果觉得吃力，就站在远处多观察一下它的警戒圈。",
        };
    }
    world->addRoom(std::move(r1));

    // ============ 2 诱变培育舱 ============
    auto r2 = std::make_unique<Room>(2, L"诱变培育舱");
    {
        MapBuilder mb(68, 15);
        mb.rect('#', 28, 3, 32, 5);          // 破损培养舱
        mb.rect('#', 48, 10, 52, 12);        // 废料堆
        setGrid(r2.get(), mb);
        addDoor(r2.get(), 0, 7, "exit_w", "g_1_2", "", 1, L"西侧");
        addDoor(r2.get(), 67, 7, "exit_e", "g_2_3", "", 3, L"东侧");
        addGround(r2.get(), 44, 5, "sample_1", L"污染样本一", "sample_1", 1);
        addArchive(r2.get(), 50, 8, "culture_log", L"培养日志",
                   Interactable::Kind::Archive, {
                       L"〔培育日志 · 项目组〕",
                       L"3月2日：厄生的细胞系出现异常增生，抑制方案全部失败。",
                       L"3月8日：我把这个“生产日期”写在报告里，主管看了之后说：再给它三个月。",
                       L"生态链备注：生产者（藻类/植物）吸收污染，食草动物富集，食肉动物再富集——",
                       L"毒素会顺着食物链向顶端层层浓缩，这就是生物富集：越往上越危险。",
                       L"记得林阅的问题吗？化学杀虫剂沿食物链富集，就是《寂静的春天》的核心控诉。",
                   });
        addContainer(r2.get(), 36, 8, "optional_crate", L"实验备用箱", "nutrient_block", 1);

        auto m2 = std::make_unique<Monster>();
        m2->id = "shuang_2"; m2->name = L"双头煞二";
        m2->x = 16; m2->y = 7;
        m2->hp = m2->maxHp = 34;
        m2->attack = 10; m2->defense = 6;
        m2->aggroRange = 3; m2->exp = 25;
        m2->flavor = L"它卡在破损的培育舱边缘，似乎有些害怕。";
        m2->weakMsg = L"双头煞二进入【基因衰弱】——它停止了挣扎，静静地看着你。";
        r2->monsters.push_back(std::move(m2));

        r2->introLines = {
            L"一排排生物反应器像失灵的巨棺。某个舱体的玻璃后，还残留着一道模糊的抓痕。",
        };
    }
    world->addRoom(std::move(r2));

    // ============ 3 温室控制室 ============
    auto r3 = std::make_unique<Room>(3, L"温室控制室");
    {
        MapBuilder mb(68, 15);
        mb.rect('#', 20, 5, 28, 5);          // 控制台（上沿）
        mb.rect('#', 20, 9, 28, 9);          // 控制台（下沿）
        mb.rect('#', 44, 4, 50, 6);          // 服务器柜
        setGrid(r3.get(), mb);
        addDoor(r3.get(), 0, 7, "exit_w", "g_2_3", "", 2, L"西侧");
        addDoor(r3.get(), 67, 7, "exit_e", "g_3_4", "lab_permit", 4, L"东侧");
        addArchive(r3.get(), 24, 7, "greenhouse_console", L"温室控制台",
                   Interactable::Kind::Terminal, {
                       L"〔温室控制台〕",
                       L"环境读数：湿度 94%、二氧化碳异常上升、水质污染残留 3.87%。",
                       L"植被失控：藤蔓在 72 小时内覆盖了三号苗床。",
                       L"生态学角度：温室植物是生产者；它失控不是因为“植物太坏”，",
                       L"而是消费者与分解者链条被污染切断——平衡崩坏，波动放大。",
                   });

        auto npc = std::make_unique<NPC>();
        npc->id = "fangqing";
        npc->name = L"方晴";
        npc->x = 14; npc->y = 7;
        npc->quizIndex = 1;
        npc->firstLines = {
            L"（方晴从植物样本堆里抬起头，眼镜片反射着绿光）你就是署里派来的？",
            L"这里原本是最“生态”的部门——植物组。直到厄生的污染让整座温室变成了疯长的丛林。",
            L"三号苗床的藤蔓一个月长出了二十五米，我三十年的驯化标签全作废了。",
            L"实验层在隔离走廊后面。权限我先给你，但这道题你总得答：",
            L"科学家们常说我们正处于一次生物大灭绝之中——你说说，现在的情况是什么？",
        };
        npc->repeatLines = {
            L"（方晴指了指东边）实验层权限已经给你了，隔离走廊的门能刷开了。",
        };
        npc->rewardKey = "lab_permit";
        npc->rewardMsg = L"【实验层权限】已同步——隔离走廊东侧门已解锁。";
        r3->npcs.push_back(std::move(npc));

        r3->introLines = {
            L"控温管道在头顶低鸣。透过观测窗，你能看见走廊那头“实验层”的灰色门。",
        };
    }
    world->addRoom(std::move(r3));

    // ============ 4 隔离走廊 ============
    auto r4 = std::make_unique<Room>(4, L"隔离走廊");
    {
        MapBuilder mb(68, 15);
        mb.rect('#', 26, 2, 27, 12);         // 中段隔断墩
        mb.put('M', 36, 7);
        mb.put('T', 10, 4); mb.put('T', 14, 12);   // 爪痕/血迹调查点（Landmark）
        setGrid(r4.get(), mb);
        addDoor(r4.get(), 0, 7, "exit_w", "g_3_4", "lab_permit", 3, L"西侧");
        addDoor(r4.get(), 67, 7, "exit_e", "g_4_5", "", 5, L"东侧");
        addArchive(r4.get(), 10, 4, "claw_marks", L"墙面爪痕",
                   Interactable::Kind::Landmark, {
                       L"〔爪痕〕三道并排的深痕，从地面延伸到胸口的高度。",
                       L"痕迹边缘的金属都卷了起来——某种力量失衡的生物，心情恐怕相当糟。",
                   });
        addArchive(r4.get(), 14, 12, "blood_mark", L"地面血迹",
                   Interactable::Kind::Landmark, {
                       L"〔血迹〕已经干涸发黑，拖行方向朝东。",
                       L"旁边散落着几根焦化的纤维——是制服，不是实验服。",
                   });

        auto m3 = std::make_unique<Monster>();
        m3->id = "lieji_1"; m3->name = L"裂脊獾一";
        m3->x = 36; m3->y = 7;
        m3->hp = m3->maxHp = 40;
        m3->attack = 11; m3->defense = 6;
        m3->aggroRange = 2; m3->exp = 40;
        m3->flavor = L"脊背的棘刺在呼吸间张开又合拢，它盯着你的鞋尖。";
        m3->weakMsg = L"裂脊獾一进入【基因衰弱】——棘刺收起，它在发抖。";
        r4->monsters.push_back(std::move(m3));

        r4->introLines = {
            L"走廊的灯隔两盏灭一盏，尽头传来抓挠地面的声音。",
            L"警示板写着：警戒范围较近，长距离移动会在贴近时被打断。",
        };
    }
    world->addRoom(std::move(r4));

    // ============ 5 活体实验舱 ============
    auto r5 = std::make_unique<Room>(5, L"活体实验舱");
    {
        MapBuilder mb(68, 15);
        mb.rect('#', 24, 3, 26, 8);          // 实验舱障碍
        mb.rect('*', 44, 12, 56, 13);        // 碎玻璃
        mb.put('I', 30, 11);
        setGrid(r5.get(), mb);
        addDoor(r5.get(), 0, 7, "exit_w", "g_4_5", "", 4, L"西侧");
        addDoor(r5.get(), 67, 7, "exit_e", "g_5_6", "", 6, L"东侧");
        addGround(r5.get(), 48, 6, "sample_2", L"污染样本二", "sample_2", 1);
        addArchive(r5.get(), 54, 9, "wolf_notes", L"灰狼改造档案",
                   Interactable::Kind::Archive, {
                       L"〔改造档案 · 编号 GW-14〕",
                       L"灰狼原型基因组 + 抗逆性基因 + 逆转录病毒载体。",
                       L"备注：目标是“军用侦察”。结果个体食量激增且攻击性失控。",
                       L"物种多样性与生态稳定性笔记：单一化改造会削掉适应缓冲；",
                       L"大自然从不依赖“完美个体”，它依赖多样化的种群。",
                       L"你可以净化它——但请记住，档案右侧的签名是一个真实的人。",
                   });
        addGround(r5.get(), 30, 11, "nutrient_floor", L"生物营养块", "nutrient_block", 1);
        tagObstacle(r5.get(), 25, 6);

        auto m4 = std::make_unique<Monster>();
        m4->id = "lieji_2"; m4->name = L"裂脊獾二";
        m4->x = 18; m4->y = 8;
        m4->hp = m4->maxHp = 44;
        m4->attack = 12; m4->defense = 7;
        m4->aggroRange = 2; m4->exp = 40;
        m4->flavor = L"它的右前爪缠着一条实验编号腕带——是被“改造”失败的那批。";
        m4->weakMsg = L"裂脊獾二进入【基因衰弱】——它侧躺下来，喉咙里发出呜咽。";
        r5->monsters.push_back(std::move(m4));

        r5->introLines = {
            L"实验台东倒西歪，玻璃碎片在脚下泛着寒光。笼位上残留着“GW-14”的标签。",
        };
    }
    world->addRoom(std::move(r5));

    // ============ 6 安保值班室 ============
    auto r6 = std::make_unique<Room>(6, L"安保值班室");
    {
        MapBuilder mb(68, 15);
        mb.rect('#', 12, 5, 18, 5);          // 值班台（上沿）
        mb.rect('#', 12, 9, 18, 9);          // 值班台（下沿）
        mb.put('T', 26, 7);
        mb.put('C', 32, 9);
        setGrid(r6.get(), mb);
        addDoor(r6.get(), 0, 7, "exit_w", "g_5_6", "", 5, L"西侧");
        addDoor(r6.get(), 67, 7, "exit_e", "g_6_7", "farm_key", 7, L"东侧");
        addArchive(r6.get(), 26, 7, "monitor_log", L"监控记录",
                   Interactable::Kind::Archive, {
                       L"〔监控记录 · 安保频道〕",
                       L"00:47 生物饲养区报告异常振动。“暂时无法定位。”",
                       L"01:32 厄生活动区间断——之后是设备的大面积断电。",
                       L"03:11 最后一条记录：走廊里出现大面积“转移后的残留物”。",
                       L"记录到此中断。值班室备用的储物柜被翻得很乱。",
                   });
        addContainer(r6.get(), 32, 9, "storage_cabinet", L"储物柜", "nutrient_block", 1);

        auto npc = std::make_unique<NPC>();
        npc->id = "zhaocheng";
        npc->name = L"赵诚";
        npc->x = 14; npc->y = 7;
        npc->quizIndex = 2;
        npc->firstLines = {
            L"（赵诚坐在两张拼起来的椅子上，制服第二颗扣子不见了）我在安保部十年，没见过它乱成这样。",
            L"我认识饲养员小王——他是一个会每天检查饲料又无聊又认真的人。上个月他也变成了“生物”。",
            L"饲养场在围栏区后面，钥匙可以给你，但你要小心那两只骸甲巨麋：它们冲撞起来能撞穿铁门。",
            L"路上碰到控制协议你会用到的。还有，问个常识题：",
            L"曾经数量多到可以遮天蔽日、却因为人类而彻底灭绝的北美候鸟，是哪一种？",
        };
        npc->repeatLines = {
            L"（赵诚把钥匙盘在桌上转了一圈）饲养场钥匙在我这，你的背包里已经有了。",
        };
        npc->rewardKey = "farm_key";
        npc->rewardKey2 = "neural_protocol";
        npc->rewardMsg = L"【饲养场钥匙】与【神经控制协议】已移交——饲养场大门已解锁。";
        r6->npcs.push_back(std::move(npc));

        r6->introLines = {
            L"值班室的电视屏幕被砸碎了，碎屑下面压着一盒没有拆封的午饭。",
        };
    }
    world->addRoom(std::move(r6));

    // ============ 7 饲养场入口 ============
    auto r7 = std::make_unique<Room>(7, L"饲养场入口");
    {
        MapBuilder mb(68, 15);
        mb.rect('#', 36, 4, 38, 10);         // 大型障碍（削弱冲撞）
        mb.put('C', 50, 8);
        setGrid(r7.get(), mb);
        addDoor(r7.get(), 0, 7, "exit_w", "g_6_7", "farm_key", 6, L"西侧");
        addDoor(r7.get(), 67, 7, "exit_e", "g_7_8", "", 8, L"东侧");
        addContainer(r7.get(), 50, 8, "feed_reserve", L"固定恢复来源", "nutrient_block", 1);
        tagObstacle(r7.get(), 37, 6);
        tagObstacle(r7.get(), 37, 9);

        auto m5 = std::make_unique<Monster>();
        m5->id = "haijia_1"; m5->name = L"骸甲巨麋一";
        m5->x = 22; m5->y = 8;
        m5->hp = m5->maxHp = 52;
        m5->attack = 13; m5->defense = 7;
        m5->aggroRange = 2; m5->exp = 60;
        m5->chargable = true;
        m5->flavor = L"骨甲像厚重的门板，它低吼着用蹄子刨地——是冲撞的前兆。";
        m5->weakMsg = L"骸甲巨麋一进入【基因衰弱】——骨甲上出现了裂纹，它慢慢跪坐下来。";
        r7->monsters.push_back(std::move(m5));

        r7->introLines = {
            L"地面的震动从脚下传上来：一、二、一、二。脚步声在走廊尽头回荡。",
            L"提示：骸甲巨麋的冲撞很痛——大型障碍（#）可以替你挡掉一半伤害。",
        };
    }
    world->addRoom(std::move(r7));

    // ============ 8 破损围栏区 ============
    auto r8 = std::make_unique<Room>(8, L"破损围栏区");
    {
        MapBuilder mb(68, 15);
        mb.rect('#', 18, 10, 20, 12);        // 倒塌围栏
        mb.put('T', 42, 10);
        setGrid(r8.get(), mb);
        addDoor(r8.get(), 0, 7, "exit_w", "g_7_8", "", 7, L"西侧");
        addDoor(r8.get(), 67, 7, "exit_e", "g_8_9", "", 9, L"东侧");
        addGround(r8.get(), 52, 9, "sample_3", L"污染样本三", "sample_3", 1);
        addArchive(r8.get(), 42, 10, "control_terminal", L"控制终端",
                   Interactable::Kind::Terminal, {
                       L"〔围栏区控制终端〕",
                       L"围栏完整度：31%。报警记录：7 次越界。",
                       L"这里是普通生物处置区的最后一站——双头煞、裂脊獾、骸甲巨麋都将在上层被报告。",
                       L"如需净化或控制，请在基因衰弱状态（HP<35%）下操作。",
                   });
        tagObstacle(r8.get(), 19, 11);

        auto m6 = std::make_unique<Monster>();
        m6->id = "haijia_2"; m6->name = L"骸甲巨麋二";
        m6->x = 32; m6->y = 8;
        m6->hp = m6->maxHp = 56;
        m6->attack = 14; m6->defense = 8;
        m6->aggroRange = 2; m6->exp = 60;
        m6->chargable = true;
        m6->flavor = L"它挡在三号样本采集位前面，鼻息里带着硫磺味。";
        m6->weakMsg = L"骸甲巨麋二进入【基因衰弱】——它把角抵在地上，不再动了。";
        r8->monsters.push_back(std::move(m6));

        r8->introLines = {
            L"围栏被撕开了一个大洞，外面就是三号样本采集点。风吹过来全是干草和杂质的味道。",
        };
    }
    world->addRoom(std::move(r8));

    // ============ 9 生态观察室 ============
    auto r9 = std::make_unique<Room>(9, L"生态观察室");
    {
        MapBuilder mb(68, 15);
        mb.rect('#', 26, 5, 34, 5);          // 试验台（上沿）
        mb.rect('#', 26, 9, 34, 9);          // 试验台（下沿）
        setGrid(r9.get(), mb);
        addDoor(r9.get(), 0, 7, "exit_w", "g_8_9", "", 8, L"西侧");
        addDoor(r9.get(), 67, 7, "exit_e", "g_9_10", "core_permit", 10, L"东侧");
        addArchive(r9.get(), 30, 7, "chief_notes", L"首席科学家笔记",
                   Interactable::Kind::Archive, {
                       L"〔首席科学家 · 手写扫描〕",
                       L"第 47 次申请被驳回：厄生依旧无法睡眠，它在镜子里看着自己。",
                       L"我最初设计了“可持续基因抗性”，上级修改成“军用可复制体”。同一个胚胎，两种目标。",
                       L"今天它叫 APF-X00。三年前它只是小犬。",
                       L"事故那天它撞开牢笼时，我听见它在低吼——不是在示威，是在求救。",
                       L"清理员，如果你看到这段字：它既是污染源，也是实验受害者。请决定。",
                   });

        auto npc = std::make_unique<NPC>();
        npc->id = "chenyan";
        npc->name = L"陈砚";
        npc->x = 16; npc->y = 7;
        npc->quizIndex = 3;
        npc->firstLines = {
            L"（陈砚没有回头，他看着监测屏上的波形）你来了。我是这群人里最早反对“军用化”的。",
            L"你猜得没错，我参与过这个项目——厄生的逆转录载体，有我的签名。",
            L"核心室里你需要的逆转录净化剂已经备好，中央权限也给你。",
            L"但最后一道题我还要问你（这是学部的考官要求，别怪我）：",
            L"《卡塔赫纳生物安全议定书》——这部国际公约主要管理的是哪类活动？",
        };
        npc->repeatLines = {
            L"（陈砚指了指东侧）逆转录净化剂和中央权限都给你了。核心室——就剩你一个了。",
        };
        npc->rewardKey = "retrovirus";
        npc->rewardKey2 = "core_permit";
        npc->rewardMsg = L"【逆转录净化剂】与【中央权限】已移交——中央控制区东侧门已解锁。";
        r9->npcs.push_back(std::move(npc));

        r9->introLines = {
            L"观察室的屏幕上，一只骨架模型被缓慢旋转着。下面的标签写着：APF-X00 / 厄生。",
        };
    }
    world->addRoom(std::move(r9));

    // ============ 10 中央控制室前厅 ============
    auto r10 = std::make_unique<Room>(10, L"中央控制室前厅");
    {
        MapBuilder mb(68, 15);
        mb.rect('#', 12, 4, 18, 5);          // 休息区桌台
        mb.put('C', 52, 7);
        setGrid(r10.get(), mb);
        addDoor(r10.get(), 0, 7, "exit_w", "g_9_10", "core_permit", 9, L"西侧");
        addDoor(r10.get(), 67, 7, "exit_e", "g_10_11", "", 11, L"东侧");
        addContainer(r10.get(), 52, 7, "final_supply", L"最终补给", "nutrient_block", 2);

        auto npc = std::make_unique<NPC>();
        npc->id = "sujin";
        npc->name = L"苏瑾（全息影像）";
        npc->x = 34; npc->y = 7;
        npc->blocking = false;               // 全息影像不挡路
        npc->quizIndex = 4;
        npc->firstLines = {
            L"（全息影像闪烁了一下）我是苏瑾——前首席运营官，我已经……不在了。",
            L"你听到的版本是：这是一家生态修复研究所。真实版本是：我们以修复为名，发展了军用的生物技术。",
            L"厄生并不是敌人。它是一个痛苦的实验品——这不会改变它是危险污染源的事实。",
            L"下面是战斗须知（请记住）：",
            L"第一次：外壳防御。骨甲闭合，无法直接净化——攻击它，制造破绽。",
            L"第二阶段：核心暴露。此时逆转录净化剂满足最终净化的条件之一。",
            L"第三阶段：意识崩解。此时神经控制协议满足最终控制的另一条件。",
            L"最终处置只有三个选项：[1]肃清 [2]净化（需药+核心暴露） [3]控制（需协议+意识崩解）。",
            L"最后一个常识问题：《禁止生物武器公约》的核心是什么？",
        };
        npc->repeatLines = {
            L"（全息影像重复播放指令）核心室在东侧。用你的方式终结它——然后继续活着。",
        };
        r10->npcs.push_back(std::move(npc));

        r10->introLines = {
            L"前厅的灯光自动亮起。这里没有任何普通生物：这是 BOSS 区前最后的安静。",
            L"（检查点已建立 · 生命已恢复至 60% 以上所需的补给已备妥）",
        };
    }
    world->addRoom(std::move(r10));

    // ============ 11 嵌合核心室 ============
    auto r11 = std::make_unique<Room>(11, L"嵌合核心室");
    {
        MapBuilder mb(68, 15);
        mb.rect('#', 30, 4, 38, 10);         // 培养舱残骸
        mb.clearRect(29, 7, 35, 7);          // 中央通道（BOSS 站位，连通西侧）
        setGrid(r11.get(), mb);
        addDoor(r11.get(), 0, 7, "exit_w", "g_10_11", "", 10, L"西侧");

        auto boss = std::make_unique<Monster>();
        boss->id = "eps"; boss->name = L"APF-X00「厄生」";
        boss->x = 34; boss->y = 7;
        boss->hp = boss->maxHp = 60;
        boss->attack = 15; boss->defense = 2;
        boss->aggroRange = 1; boss->exp = 150;
        boss->boss = true; boss->bossPhase = 1;
        boss->flavor = L"它像一座会呼吸的塔。三条不同属的肢体同时支撑着身体——两年来它从没合过眼。";
        boss->weakMsg = L"（厄生无法进入衰弱的停摆期——你必须推进三个阶段。）";
        r11->monsters.push_back(std::move(boss));

        r11->introLines = {
            L"门在身后合拢。警报灯转成红色。",
            L"厄生站在培养舱的废墟中央，比你想象的更安静——它一直在等你来。",
        };
    }
    world->addRoom(std::move(r11));

    return world;
}

// ---------------- 生态题 ----------------
const std::vector<Question> kQuestions = {
    {
        L"林阅",
        L"《寂静的春天》核心控诉的是什么？",
        { L"① 工业废水的排放", L"② 森林砍伐与荒漠化",
          L"③ 滥用化学杀虫剂并沿食物链富集", L"④ 城市噪音与光污染" },
        2,
        L"《寂静的春天》控诉的是滥用化学杀虫剂（如 DDT）并沿食物链富集，最终毒害鸟类与生态链。物质沿食物链逐级浓缩，越接近顶端越危险。",
    },
    {
        L"方晴",
        L"你认为当前地球正处在哪一种生物大灭绝之中？",
        { L"① 第二次，小行星撞击所致", L"② 第六次，主要驱动是人类活动",
          L"③ 第五次，纯自然气候原因", L"④ 并没有发生大灭绝" },
        1,
        L"当前处于第六次生物大灭绝，IPBES 报告指出主要驱动是人类活动（栖息地破坏、污染、过度开发等）。前五次都源于自然，这次却是我们自己。",
    },
    {
        L"赵诚",
        L"哪一种北美候鸟因人类活动而彻底灭绝？",
        { L"① 北美洲燕雀", L"② 旅鸽",
          L"③ 白头海雕", L"④ 北美黑嘴鸦" },
        1,
        L"旅鸽：19 世纪初仍有数十亿只，因过度猎杀与栖息地消失，1914 年最后一只个体『玛莎』死亡，物种灭绝。人口贪婪从来不是守恒量。",
    },
    {
        L"陈砚",
        L"《卡塔赫纳生物安全议定书》主要管理什么？",
        { L"① 全球捕鲸配额", L"② 改性活基因生物的跨境转移和环境释放",
          L"③ 臭氧层消耗物质", L"④ 生物武器的销毁" },
        1,
        L"《卡塔赫纳生物安全议定书》管理改性活生物体（LMO，如转基因生物）的跨境转移、环境释放与安全评估。它保护的不是基因本身，而是人类生态。",
    },
    {
        L"苏瑾",
        L"《禁止生物武器公约》的核心目的是什么？",
        { L"① 限制疫苗研制速度", L"② 禁止研发、生产和储存生物武器",
          L"③ 禁止一切人体实验", L"④ 保护实验动物福利" },
        1,
        L"《禁止生物武器公约》(BWC) 的核心是：禁止发展、生产、储存和使用生物武器，并要求缔约国销毁现有武器。生物技术越强，这五十年前的约定越重要。",
    },
};

} // namespace

// ============================================================
// GameData 公开接口
// ============================================================
std::unique_ptr<World> GameData::buildWorld() {
    return buildWorldImpl();
}

const std::vector<Question>& GameData::questions() {
    return kQuestions;
}

std::vector<std::wstring> GameData::endingLines(const std::string& which) {
    if (which == "eliminate") {
        return {
            L"你选择了【肃清】。",
            L"高能抑制装置压制了污染核心。厄生的生命在短暂的抽搐后终止。",
            L"污染被阻断。设施转入净化程序。",
            L"你在回程的飞机上把笔放了又拿起：",
            L"安全，难道只能通过毁灭获得吗？",
        };
    }
    if (which == "purify") {
        return {
            L"你选择了【净化】。",
            L"逆转录净化剂沿核心静脉扩散。异常增生逐渐停止。",
            L"厄生安静下来——那是它出生以来，大概第一次不疼。",
            L"你会背上长期修复的责任：接下来的七年，你都要回来给它做疗程。",
            L"你写下结案报告的第二行：『种下的灾，总要有人去还。』",
        };
    }
    return {
        L"你选择了【控制】。",
        L"神经控制协议锁定了意识回路。厄生停止反抗，污染暂时受控。",
        L"但它的基因排斥没有消失。它的痛苦没有消失。",
        L"你在备注栏留下两个字：不等于。",
        L"——控制不等于拯救。它在等一个不会到来的结局。",
    };
}

std::wstring GameData::quizResultText(const Question& q, bool correct) {
    if (correct) {
        return L"你回答正确。确认：" + q.explanation;
    }
    return L"回答错误。正确答案：" + q.options[q.correct] +
           L"——" + q.explanation;
}

// ---------------- 标题画面 ----------------
const std::vector<std::wstring>& GameData::titleArt() {
    static const std::vector<std::wstring> art = {
        L"    ███████╗ ██████╗  ██████╗ ██╗     ██████╗  ██████╗ ██╗ ██████╗ █████╗ ██╗",
        L"    ██╔════╝ ██╔══██╗ ██╔═══██╗██║     ██╔══██╗██╔═══██╗██║██╔════╝██╔══██╗██║",
        L"    █████╗   ██████╔╝ ██║   ██║██║     ██████╔╝██║   ██║██║██║     ███████║██║",
        L"    ██╔══╝   ██╔══██╗ ██║   ██║██║     ██╔══██╗██║   ██║██║██║     ██╔══██║██║",
        L"    ███████╗ ██║  ██║ ╚██████╔╝███████╗██║  ██║╚██████╔╝██║╚██████╗██║  ██║███████╗",
        L"    ╚══════╝ ╚═╝  ╚═╝  ╚═════╝ ╚══════╝╚═╝  ╚═╝ ╚═════╝ ╚═╝ ╚═════╝╚═╝  ╚═╝╚══════╝",
        L"",
        L"        █████╗ ██████╗ ███████╗        ██████╗ ███████╗ ██████╗███████╗ ██████╗",
        L"       ██╔══██╗██╔══██╗██╔════╝ ██████╗ ██╔══██╗██╔════╝██╔════╝██╔════╝██╔═══██╗",
        L"       ███████║██████╔╝███████╗ ██╔═══╝ ██████╔╝█████╗  ██║     █████╗  ██║   ██║",
        L"       ██╔══██║██╔═══╝ ╚════██║ ╚██████╗ ██╔══██╗██╔══╝  ██║     ██╔══╝  ██║   ██║",
        L"       ██║  ██║██║     ███████║  ╚═════╝ ██║  ██║███████╗╚██████╗███████╗╚██████╔╝",
        L"       ╚═╝  ╚═╝╚═╝     ╚══════╝          ╚═╝  ╚═╝╚══════╝ ╚═════╝╚══════╝ ╚═════╝",
        L"",
        L"                 ═╗  ╔═        A P F - X 0 0    ═        ═╗  ╔═",
    };
    return art;
}

const std::vector<std::wstring>& GameData::titleCaption() {
    static const std::vector<std::wstring> cap = {
        L"",
        L"                                   《生态禁区：畸合之源》",
        L"                  APF-X00 · 阿波菲斯生态研究设施，一级生态禁区 · 2031年",
        L"",
        L"  W/A/S/D 移动  ·  数字交互  ·  Enter 打开英文命令  ·  H 提示  ·  F5 存档",
        L"  mission 查看任务  ·  hint 下一步建议  ·  help 全部命令  ·  Q 退出",
        L"",
        L"  “安全，只能通过毁灭获得吗？” —— Cleaner-07",
    };
    return cap;
}

// ---------------- 序章 ----------------
const std::vector<std::wstring>& GameData::prologue() {
    static const std::vector<std::wstring> lines = {
        L"2045 年。国际生态安全署收到加密档案：",
        L"「阿波菲斯生态研究设施」——曾以生态修复、污染适应和环境恢复为名建立。",
        L"档案显示：它在 2030 年代悄悄转向了基因实验，目标：军用嵌合生命。",
        L"APF-X00「厄生」——融合多种动物基因与逆转录病毒片段的造物。",
        L"它长期处于基因排斥、异常增生，以及无法自然死亡的痛苦之中。",
        L"三个月前，一次事故：它撞破牢笼，污染扩散到整条设施生态链。",
        L"你——Cleaner-07，国际生态安全署调查员，即将进入一级生态禁区。",
        L"调查污染源，收集三份污染样本，处置厄生。",
        L"处置方式只有三种：肃清、净化、控制。",
        L"肃清：彻底阻断污染源——代价由你转述给世界。",
        L"净化：终止异常增生，承担长期修复责任。",
        L"控制：让污染暂时受控——但记住，控制不等于拯救。",
        L"生态安全署在等你交回一份「标准答案」。",
        L"但科学和良心，从来都没有标准答案。",
        L"行动开始。",
    };
    return lines;
}

const std::vector<std::wstring>& GameData::combatTutorial() {
    static const std::vector<std::wstring> lines = {
        L"【战斗教学 · 第一次遭遇】",
        L"战斗全部用数字完成：[1]攻击 [2]净化 [3]控制 [4]使用道具。",
        L"把敌人打到【基因衰弱】（HP 低于 35%）后，[2]净化或 [3]控制可直接成功——",
        L"当然，[1]攻击到 0 就是肃清。生命低于 30% 时，记得 [4] 用生物营养块。",
    };
    return lines;
}
