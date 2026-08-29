#include "Board.h"
#include "BoardItemBase.h"
#include "BoardTile.h"
#include "MatchFinder.h"
#include "MatchFinderBridge.h"
#include <QDebug>
#include <QRandomGenerator>

Board::Board(int rows, int cols, QObject *parent)
    : QObject(parent), m_rows(rows), m_cols(cols)
{
    m_cells.resize(m_rows);
    for (int r = 0; r < m_rows; ++r) {
        m_cells[r].resize(m_cols);
        for (int c = 0; c < m_cols; ++c) {
            m_cells[r][c] = new BoardTile();
        }
    }
    m_mask.resize(m_rows);
    for (int r = 0; r < m_rows; ++r) {
        m_mask[r].resize(m_cols);
        for (int c = 0; c < m_cols; ++c) m_mask[r][c] = true;
    }

    // 初始按格填充，避免产生初始三消：对于每个格子随机选择颜色，但排除会与左/上形成三连的候选
    QVector<QString> colors = {"red","green","blue","yellow","purple","brown"};
    for (int r = 0; r < m_rows; ++r) {
        for (int c = 0; c < m_cols; ++c) {
            BoardTile* t = static_cast<BoardTile*>(m_cells[r][c]);
            // 尝试若干次随机候选，优先随机以保证盘面多样性
            QString chosen;
            const int maxAttempts = 10;
            for (int attempt = 0; attempt < maxAttempts; ++attempt) {
                int idx = QRandomGenerator::global()->bounded(colors.size());
                QString cand = colors[idx];
                bool bad = false;
                // 检查横向：左边两个是否都与候选相同
                if (c >= 2) {
                    auto left1 = static_cast<BoardTile*>(m_cells[r][c-1])->color();
                    auto left2 = static_cast<BoardTile*>(m_cells[r][c-2])->color();
                    if (!left1.isEmpty() && !left2.isEmpty() && left1 == cand && left2 == cand) bad = true;
                }
                // 检查纵向：上面两个是否都与候选相同
                if (r >= 2) {
                    auto up1 = static_cast<BoardTile*>(m_cells[r-1][c])->color();
                    auto up2 = static_cast<BoardTile*>(m_cells[r-2][c])->color();
                    if (!up1.isEmpty() && !up2.isEmpty() && up1 == cand && up2 == cand) bad = true;
                }
                if (!bad) { chosen = cand; break; }
            }
            // 如果随机尝试未找到合适颜色，顺序查找第一个不产生三连的颜色
            if (chosen.isEmpty()) {
                for (const QString &cand : colors) {
                    bool bad = false;
                    if (c >= 2) {
                        auto left1 = static_cast<BoardTile*>(m_cells[r][c-1])->color();
                        auto left2 = static_cast<BoardTile*>(m_cells[r][c-2])->color();
                        if (!left1.isEmpty() && !left2.isEmpty() && left1 == cand && left2 == cand) bad = true;
                    }
                    if (r >= 2) {
                        auto up1 = static_cast<BoardTile*>(m_cells[r-1][c])->color();
                        auto up2 = static_cast<BoardTile*>(m_cells[r-2][c])->color();
                        if (!up1.isEmpty() && !up2.isEmpty() && up1 == cand && up2 == cand) bad = true;
                    }
                    if (!bad) { chosen = cand; break; }
                }
            }
            // 最后兜底：若仍为空则随机选一个
            if (chosen.isEmpty()) chosen = colors[QRandomGenerator::global()->bounded(colors.size())];
            t->setColor(chosen);
        }
    }

    emit tilesChanged();
}

Board::~Board() {
    for (int r=0;r<m_rows;++r) {
        for (int c=0;c<m_cols;++c) {
            delete m_cells[r][c];
            m_cells[r][c] = nullptr;
        }
    }
}

int Board::rows() const { return m_rows; }
int Board::cols() const { return m_cols; }

void Board::setShapeMask(const QVector<QVector<bool>>& mask) {
    if (mask.size() != m_rows) return;
    m_mask = mask;
}

BoardItem* Board::cellAt(int r, int c) const {
    if (r < 0 || r >= m_rows || c < 0 || c >= m_cols) return nullptr;
    return m_cells[r][c];
}

bool Board::swapTiles(int r1, int c1, int r2, int c2, LastSwapInfo* outSwap) {
    if (r1<0||r1>=m_rows||r2<0||r2>=m_rows||c1<0||c1>=m_cols||c2<0||c2>=m_cols) return false;
    std::swap(m_cells[r1][c1], m_cells[r2][c2]);
    if (outSwap) {
        outSwap->a = QPoint(r1,c1);
        outSwap->b = QPoint(r2,c2);
        outSwap->valid = true;
    }
    emit tilesChanged();
    return true;
}

QVector<QPoint> Board::findMatches(const LastSwapInfo* swap) const {
    // Delegate to MatchFinder to keep algorithm separate
    auto results = MatchFinder::findMatches(this, swap);
    QVector<QPoint> out;
    for (const auto &mr : results) {
        for (const QPoint &p : mr.tiles) out.append(p);
    }
    return out;
}

QVector<QPair<int,int>> Board::applyGravity() {
    QVector<QPair<int,int>> moved;
    // For each column, compact non-empty tiles down and record movements
    for (int c=0;c<m_cols;++c) {
        int write = m_rows-1;
        for (int r=m_rows-1;r>=0;--r) {
            BoardItem* it = m_cells[r][c];
            if (!it) continue;
            if (it->kind != BoardItem::Kind::Tile) continue; // only tiles move in this minimal impl
            BoardTile* t = static_cast<BoardTile*>(it);
            if (t->color().isEmpty()) continue; // empty
            if (write != r) {
                // move pointer
                std::swap(m_cells[write][c], m_cells[r][c]);
                moved.append(qMakePair(write, c));
            }
            write--;
        }
        // fill remaining with new tiles
        for (int rr = write; rr >= 0; --rr) {
            delete m_cells[rr][c];
            m_cells[rr][c] = new BoardTile();
            moved.append(qMakePair(rr, c));
        }
    }
    emit tilesChanged();
    return moved;
}

void Board::fillNewTiles() {
    // Ensure no tile has empty color; set random basic colors
    QVector<QString> colors = {"red","green","blue","yellow","purple","brown"};
    for (int r=0;r<m_rows;++r) for (int c=0;c<m_cols;++c) {
        BoardItem* it = m_cells[r][c];
        if (!it) continue;
        if (it->kind != BoardItem::Kind::Tile) continue;
        BoardTile* t = static_cast<BoardTile*>(it);
        if (t->color().isEmpty()) {
            int idx = QRandomGenerator::global()->bounded(colors.size());
            t->setColor(colors[idx]);
        }
    }
    emit tilesChanged();
}