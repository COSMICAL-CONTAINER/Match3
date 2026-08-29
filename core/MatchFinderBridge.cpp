#include "MatchFinderBridge.h"
#include "Board.h"
#include "BoardItemBase.h"
#include "BoardTile.h"
#include "core/BoardModel.h"
#include "core/Types.h"
#include "core/MatchFinder.h"

#include <QVariant>

QVector<MatchFinder::MatchResult> MatchFinder::findMatches(const Board* board, const LastSwapInfo* swap) {
    Q_UNUSED(swap);
    QVector<MatchResult> out;
    if (!board) return out;
    core::BoardModel bm(board->rows(), board->cols(), 6);
    auto colorToIndex = [](const QString &s)->uint8_t {
        if (s == "red") return 1;
        if (s == "green") return 2;
        if (s == "blue") return 3;
        if (s == "yellow") return 4;
        if (s == "purple") return 5;
        if (s == "brown") return 6;
        return 0;
    };
    for (int r=0;r<board->rows();++r) {
        for (int c=0;c<board->cols();++c) {
            BoardItem* it = board->cellAt(r,c);
            if (!it) { bm.setTile(r,c,0); continue; }
            if (it->kind != BoardItem::Kind::Tile) { bm.setTile(r,c,0); continue; }
            auto tile = static_cast<BoardTile*>(it);
            bm.setTile(r,c, colorToIndex(tile->color()));
        }
    }
    core::MatchResult cmr = core::MatchFinder::findMatches(bm, nullptr);
    if (!cmr.matched.isEmpty()) {
        MatchResult legacy;
        for (const QPoint &p : cmr.matched) legacy.tiles.append(QPoint(p.y(), p.x()));
        out.append(legacy);
    }
    return out;
}
