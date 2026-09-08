// Item.cpp —— 物品表定义
#include "Item.h"

namespace {
    const std::vector<ItemDef> kItems = {
        // 关键物品（Key）：不可丢弃/死亡不丢失/消耗逻辑不可删除
        { "greenhouse_card", L"温室门禁卡", L"开启温室区防护门的授权卡。", ItemDef::Key, 0 },
        { "lab_permit",      L"实验层权限", L"进入隔离走廊与活体实验舱的权限许可。", ItemDef::Key, 0 },
        { "farm_key",        L"饲养场钥匙", L"饲养场入口的机械钥匙，同时附带神经控制协议。", ItemDef::Key, 0 },
        { "core_permit",     L"中央权限",   L"进入中央控制区（B4层）的最高授权。", ItemDef::Key, 0 },
        { "neural_protocol", L"神经控制协议", L"用于驯化嵌合生物的神经植入协议，稳定条件已标注。", ItemDef::Key, 0 },
        { "retrovirus",      L"逆转录净化剂", L"针对厄生基因排斥的净化制剂，需配合核心暴露使用。", ItemDef::Key, 0 },
        // 污染样本（Sample）：固定获得，不可丢失
        { "sample_1", L"污染样本一", L"诱变培育舱内的异常组织样本。", ItemDef::Sample, 0 },
        { "sample_2", L"污染样本二", L"活体实验舱内的污染皮毛样本。", ItemDef::Sample, 0 },
        { "sample_3", L"污染样本三", L"破损围栏区的土壤与血液混合样本。", ItemDef::Sample, 0 },
        // 消耗品
        { "nutrient_block", L"生物营养块", L"高密度营养凝胶，恢复 80 点生命。", ItemDef::Consumable, 80 },
    };
}

const std::vector<ItemDef>& allItemDefs() {
    return kItems;
}

const ItemDef* findItemDef(const std::string& id) {
    for (const ItemDef& item : kItems) {
        if (item.id == id) return &item;
    }
    return nullptr;
}
