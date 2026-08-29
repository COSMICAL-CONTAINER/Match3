#pragma once
#include "BoardItemBase.h"
#include <QString>

// BoardTile: 普通可消除格子（颜色）
class BoardTile : public BoardItem {
public:
    BoardTile();
    explicit BoardTile(const QString &color);

    QString color() const { return m_color; }
    void setColor(const QString &c) { m_color = c; }

    bool isMovable() const; // 是否参与下落/匹配

private:
    QString m_color;
};
