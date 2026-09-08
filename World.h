// World.h —— 世界：用 unique_ptr 拥有 12 个独立房间
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Room.h"

class World {
public:
    void addRoom(std::unique_ptr<Room> r) { rooms_.push_back(std::move(r)); }
    Room* room(int id);
    const Room* room(int id) const;
    std::vector<std::unique_ptr<Room>>& rooms() { return rooms_; }

    // 全局解锁（同时解锁两端的门）
    void unlockDoor(const std::string& doorId);

    // 从 from 房间走向 target 房间的第一个有效方向标签（"东侧"/"当前房间"）
    std::wstring dirToward(int from, int target) const;

    // 房间名
    std::wstring roomName(int id) const;

private:
    std::vector<std::unique_ptr<Room>> rooms_;
};
