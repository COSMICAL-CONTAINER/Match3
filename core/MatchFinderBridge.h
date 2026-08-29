#pragma once

#include <QVector>
#include <QPoint>
#include <QVariant>

class Board; struct LastSwapInfo;

// Legacy global MatchFinder API expected by existing core code (Board.cpp)
class MatchFinder {
public:
    struct MatchResult { QVector<QPoint> tiles; QVariant meta; };
    static QVector<MatchResult> findMatches(const Board* board, const LastSwapInfo* swap=nullptr);
};
