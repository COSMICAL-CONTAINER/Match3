#pragma once

#include <QVector>
#include <QString>
#include <QPoint>
#include "Types.h"

namespace core {

class BoardModel {
public:
    BoardModel(int rows=8, int cols=8, int numColors=6);

    int rows() const;
    int cols() const;
    int numColors() const;

    uint8_t tileAt(int r, int c) const;
    void setTile(int r, int c, uint8_t color);

    void swapTiles(int r1,int c1,int r2,int c2);

    // Drop description for animations: from -> to coordinates and color; isNew indicates newly generated tiles
    struct Drop {
        int fromR;
        int fromC;
        int toR;
        int toC;
        uint8_t color;
        bool isNew;
    };

    // apply gravity, refill and return list of drops describing how tiles moved/appeared
    QVector<Drop> applyGravityAndRefill();

    QString colorName(uint8_t color) const;
    QString toString() const; // debug

private:
    int m_rows;
    int m_cols;
    int m_numColors;
    QVector<QVector<uint8_t>> m_board;
};

} // namespace core
