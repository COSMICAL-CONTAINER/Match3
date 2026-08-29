#pragma once
#include "BoardItem.h"

// BoardObstacle: 棋盘障碍（覆盖在 Tile 之上），需要按规则移除
class BoardObstacle : public BoardItem {
public:
    BoardObstacle() { kind = Kind::Obstacle; }
    int hp() const { return m_hp; }
    void damage(int v) { m_hp -= v; }
    bool isCleared() const { return m_hp <= 0; }

private:
    int m_hp = 1; // 可扩展为多层障碍
};
