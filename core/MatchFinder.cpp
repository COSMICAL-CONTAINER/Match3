#include "MatchFinder.h"
#include <QDebug>

// No 'using namespace core' to avoid ambiguity with legacy symbols

static void addUnique(QVector<QPoint> &vec, const QPoint &p) {
    for (const QPoint &e : vec) if (e == p) return;
    vec.append(p);
}

core::MatchResult core::MatchFinder::findMatches(const core::BoardModel &board, const core::LastSwapInfo *lastSwap) {
    Q_UNUSED(lastSwap);
    core::MatchResult res;
    int rows = board.rows();
    int cols = board.cols();

    // horizontal runs
    for (int r=0;r<rows;++r) {
        int c=0;
        while (c<cols) {
            uint8_t color = board.tileAt(r,c);
            if (color == 0) { ++c; continue; }
            int runStart = c;
            int runLen = 1; ++c;
            while (c<cols && board.tileAt(r,c) == color) { ++runLen; ++c; }
            if (runLen >= 3) {
                for (int k=0;k<runLen;++k) addUnique(res.matched, QPoint(runStart+k, r));
                if (runLen == 4) {
                    // 横向4连：生成纵向火箭（反方向），位置尽量靠 swap 中心
                    int centerX = runStart + 1; // 默认靠中间偏左
                    if (lastSwap) {
                        int ax = lastSwap->a.x();
                        int bx = lastSwap->b.x();
                        int ay = lastSwap->a.y();
                        int by = lastSwap->b.y();
                        if (ay == r && by == r) {
                            // 交换发生在这一行内，取交换后落在这行中的那一格更靠近的格子
                            int sx = (ax == runStart || ax == runStart+runLen-1) ? bx : ax;
                            if (sx >= runStart && sx < runStart+runLen)
                                centerX = sx;
                        }
                    }
                    core::PropSuggestion s; s.pos = QPoint(centerX, r); s.type = core::PropType::RocketVertical; res.suggestions.append(s);
                } else if (runLen >= 5) {
                    core::PropSuggestion s; s.pos = QPoint(runStart+2, r); s.type = core::PropType::SuperProp; res.suggestions.append(s);
                }
            }
        }
    }

    // vertical runs
    for (int c=0;c<cols;++c) {
        int r=0;
        while (r<rows) {
            uint8_t color = board.tileAt(r,c);
            if (color == 0) { ++r; continue; }
            int runStart = r;
            int runLen = 1; ++r;
            while (r<rows && board.tileAt(r,c) == color) { ++runLen; ++r; }
            if (runLen >= 3) {
                for (int k=0;k<runLen;++k) addUnique(res.matched, QPoint(c, runStart+k));
                if (runLen == 4) {
                    // 竖向4连：生成横向火箭（反方向），位置尽量靠 swap 中心
                    int centerY = runStart + 1;
                    if (lastSwap) {
                        int ax = lastSwap->a.x();
                        int bx = lastSwap->b.x();
                        int ay = lastSwap->a.y();
                        int by = lastSwap->b.y();
                        if (ax == c && bx == c) {
                            int sy = (ay == runStart || ay == runStart+runLen-1) ? by : ay;
                            if (sy >= runStart && sy < runStart+runLen)
                                centerY = sy;
                        }
                    }
                    core::PropSuggestion s; s.pos = QPoint(c, centerY); s.type = core::PropType::RocketHorizontal; res.suggestions.append(s);
                } else if (runLen >= 5) {
                    core::PropSuggestion s; s.pos = QPoint(c, runStart+2); s.type = core::PropType::SuperProp; res.suggestions.append(s);
                }
            }
        }
    }

    // detect T/L shapes (intersection of horizontal>=3 and vertical>=3 for same color)
    QVector<QVector<int>> horLen(rows, QVector<int>(cols,0));
    QVector<QVector<int>> verLen(rows, QVector<int>(cols,0));
    for (int r=0;r<rows;++r) {
        for (int c=0;c<cols;++c) {
            uint8_t color = board.tileAt(r,c);
            if (color == 0) { horLen[r][c]=0; verLen[r][c]=0; continue; }
            // horizontal length centered at c
            int l=c; while (l-1>=0 && board.tileAt(r,l-1)==color) --l;
            int rr=c; while (rr+1<cols && board.tileAt(r,rr+1)==color) ++rr;
            horLen[r][c] = rr - l + 1;
            // vertical length centered at r
            int t=r; while (t-1>=0 && board.tileAt(t-1,c)==color) --t;
            int b=r; while (b+1<rows && board.tileAt(b+1,c)==color) ++b;
            verLen[r][c] = b - t + 1;
        }
    }

    for (int r=0;r<rows;++r) {
        for (int c=0;c<cols;++c) {
            if (horLen[r][c] >=3 && verLen[r][c] >=3) {
                uint8_t color = board.tileAt(r,c);
                if (color==0) continue;
                // add horizontal span
                int left = c; while (left-1>=0 && board.tileAt(r,left-1)==color) { addUnique(res.matched, QPoint(left-1, r)); --left; }
                int right = c; while (right+1<cols && board.tileAt(r,right+1)==color) { addUnique(res.matched, QPoint(right+1, r)); ++right; }
                // add vertical span
                int up = r; while (up-1>=0 && board.tileAt(up-1,c)==color) { addUnique(res.matched, QPoint(c, up-1)); --up; }
                int down = r; while (down+1<rows && board.tileAt(down+1,c)==color) { addUnique(res.matched, QPoint(c, down+1)); ++down; }
                addUnique(res.matched, QPoint(c,r));
                // suggest bomb at center
                core::PropSuggestion s; s.pos = QPoint(c,r); s.type = core::PropType::BombProp; res.suggestions.append(s);
            }
        }
    }

    qDebug() << "MatchFinder::findMatches -> matched:" << res.matched.size() << "suggestions:" << res.suggestions.size();
    return res;
}
