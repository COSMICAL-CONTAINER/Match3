#include "BoardModel.h"
#include <QRandomGenerator>
#include <QDebug>

using namespace core;

BoardModel::BoardModel(int rows, int cols, int numColors)
    : m_rows(rows), m_cols(cols), m_numColors(numColors), m_board(rows, QVector<uint8_t>(cols, 0))
{
    // 这里只负责分配棋盘，具体初始化由 GameEngine::startGame/initializeBoard 控制，
    // 避免一创建就产生随机三消，再由外部重刷。
}

int BoardModel::rows() const { return m_rows; }
int BoardModel::cols() const { return m_cols; }
int BoardModel::numColors() const { return m_numColors; }

uint8_t BoardModel::tileAt(int r, int c) const {
    if (r<0 || r>=m_rows || c<0 || c>=m_cols) return 0;
    return m_board[r][c];
}

void BoardModel::setTile(int r, int c, uint8_t color) {
    if (r<0 || r>=m_rows || c<0 || c>=m_cols) return;
    m_board[r][c] = color;
}

void BoardModel::swapTiles(int r1,int c1,int r2,int c2) {
    if (r1<0||r1>=m_rows||r2<0||r2>=m_rows||c1<0||c1>=m_cols||c2<0||c2>=m_cols) return;
    std::swap(m_board[r1][c1], m_board[r2][c2]);
}

QVector<BoardModel::Drop> BoardModel::applyGravityAndRefill() {
    QVector<Drop> drops;
    drops.reserve(m_rows * m_cols);

    // For each column, collapse non-zero tiles downwards. Track movements as drops.
    for (int c=0;c<m_cols;++c) {
        int write = m_rows-1;
        // move existing tiles down, recording their from->to
        for (int r=m_rows-1;r>=0;--r) {
            if (m_board[r][c] != 0) {
                if (write != r) {
                    // record drop from (r,c) to (write,c)
                    Drop d; d.fromR = r; d.fromC = c; d.toR = write; d.toC = c; d.color = m_board[r][c]; d.isNew = false;
                    drops.append(d);
                    m_board[write][c] = m_board[r][c];
                    m_board[r][c] = 0;
                }
                --write;
            }
        }
        // refill remaining (these are new tiles appearing at top)
        for (int r=write;r>=0;--r) {
            uint8_t color = (uint8_t)(QRandomGenerator::global()->bounded(1, m_numColors+1));
            m_board[r][c] = color;
            Drop d; d.fromR = -1; d.fromC = c; d.toR = r; d.toC = c; d.color = color; d.isNew = true;
            drops.append(d);
        }
    }

    return drops;
}

QString BoardModel::colorName(uint8_t color) const {
    switch (color) {
        case 1: return "red";
        case 2: return "green";
        case 3: return "blue";
        case 4: return "yellow";
        case 5: return "purple";
        case 6: return "brown";
        case 7: return "Rocket_1";   // 纵向火箭
        case 8: return "Rocket_2";   // 横向火箭
        case 9: return "Bomb";       // 炸弹
        case 10: return "SuperItem"; // 超级道具
        default: return "transparent";
    }
}

QString BoardModel::toString() const {
    QString s;
    for (int r=0;r<m_rows;++r) {
        for (int c=0;c<m_cols;++c) {
            s += QString::number(m_board[r][c]);
        }
        s += '\n';
    }
    return s;
}
