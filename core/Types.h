#pragma once

#include <QPoint>
#include <QVector>
#include <QString>

namespace core {

enum class PropType {
    None = 0,
    RocketHorizontal = 1,
    RocketVertical = 2,
    BombProp = 3,
    SuperProp = 4
};

struct PropSuggestion {
    QPoint pos;    // position to create the prop
    PropType type = PropType::None;
    QString color; // used for super-item (optional)
};

struct MatchResult {
    QVector<QPoint> matched;             // list of matched tile coordinates
    QVector<PropSuggestion> suggestions; // suggested props to create
};

struct LastSwapInfo {
    QPoint a;
    QPoint b;
    LastSwapInfo() {}
    LastSwapInfo(int r1, int c1, int r2, int c2) : a(c1, r1), b(c2, r2) {}
};

} // namespace core
