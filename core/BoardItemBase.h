#pragma once
#include <QString>
#include <QPoint>

// BoardItem: 棋盘单元基础类（外观资源、位置、类型信息）
class BoardItem {
public:
    enum class Kind { Empty, Tile, Prop, Obstacle };
    virtual ~BoardItem() = default;

    Kind kind = Kind::Empty;
    QString resource; // UI 层用于渲染的资源标识（png/gif 的 qrc 路径或 key）
    QPoint pos{-1,-1};
};
