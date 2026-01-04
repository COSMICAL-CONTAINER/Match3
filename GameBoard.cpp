#include "GameBoard.h"
#include <QRandomGenerator>
#include <QDebug>
#include <QVariant>
#include <QList>
#include <QVector>
#include <QPoint>
#include <QTimer>

GameBoard::GameBoard(QObject *parent, int rows, int columns)
    : QObject(parent), m_rows(rows), m_columns(columns), m_score(0)
{
    m_availableColors = {"red", "green", "blue", "yellow", "purple"};
    initializeBoard();
}

void GameBoard::initializeBoard() {
    m_board.resize(m_rows);
    for (int r = 0; r < m_rows; ++r) {
        m_board[r].resize(m_columns);
        for (int c = 0; c < m_columns; ++c)
            m_board[r][c] = getRandomColor();
    }
    emit boardChanged();
}

QString GameBoard::tileAt(int row, int col) const {
    if (row < 0 || row >= m_rows || col < 0 || col >= m_columns)
        return "transparent";
    return m_board[row][col];
}

// void GameBoard::trySwap(int r1, int c1, int r2, int c2) {
//     qDebug() << "尝试交换:" << r1 << c1 << "->" << r2 << c2;

//     if (!isValidSwap(r1, c1, r2, c2)) {
//         qDebug() << "无效交换";
//         emit invalidSwap(r1, c1, r2, c2);
//         return;
//     }

//     // 执行交换
//     std::swap(m_board[r1][c1], m_board[r2][c2]);
//     emit swapAnimationRequested(r1, c1, r2, c2);

//     // 检查匹配
//     auto matches = findMatches();
//     if (!matches.isEmpty()) {
//         qDebug() << "找到匹配，数量:" << matches.size();

//         // 转换为QVariantList用于QML传输
//         QVariantList variantMatches;
//         for (const QPoint &pt : matches) {
//             variantMatches.append(QVariant::fromValue(pt));
//         }
//         emit matchAnimationRequested(variantMatches);

//         // 移除匹配的方块
//         removeMatchedTiles(matches);
//         emit boardChanged();

//         // 计算掉落路径
//         auto drops = calculateDropPaths();
//         QVariantList variantDrops;
//         for (const auto &path : drops) {
//             QVariantList pathList;
//             for (const QPoint &pt : path) {
//                 qDebug() << pt;
//                 pathList.append(QVariant::fromValue(pt));
//             }
//             qDebug() << "\n";
//             variantDrops.append(QVariant::fromValue(pathList));
//         }
//         emit dropAnimationRequested(variantDrops);

//     }
//     else
//     {
//         // 没有匹配，交换回来
//         qDebug() << "无匹配，交换回原位置";
//         std::swap(m_board[r1][c1], m_board[r2][c2]);
//         emit invalidSwap(r1, c1, r2, c2);
//     }
// }

void GameBoard::trySwap(int r1, int c1, int r2, int c2) {
    qDebug() << "尝试交换:" << r1 << c1 << "->" << r2 << c2;

    if (!isValidSwap(r1, c1, r2, c2)) {
        qDebug() << "无效交换";
        emit invalidSwap(r1, c1, r2, c2);   // ❌ 只告诉 QML 播动画
        return;
    }

    emit swapAnimationRequested(r1, c1, r2, c2);  // ✅ 播放有效交换动画
}


Q_INVOKABLE void GameBoard::finalizeSwap(int r1, int c1, int r2, int c2) {
    // 交换
    std::swap(m_board[r1][c1], m_board[r2][c2]);

    // 检查匹配
    auto matches = findMatches();
    if (!matches.isEmpty()) {
        qDebug() << "找到匹配，数量:" << matches.size();
        QVariantList variantMatches;
        for (const QPoint &pt : matches)
            variantMatches.append(QVariant::fromValue(pt));
        emit matchAnimationRequested(variantMatches);

        removeMatchedTiles(matches);
        emit boardChanged();

        auto drops = calculateDropPaths();
        QVariantList variantDrops;
        for (const auto &path : drops) {
            QVariantList pathList;
            for (const QPoint &pt : path)
                pathList.append(QVariant::fromValue(pt));
            variantDrops.append(QVariant::fromValue(pathList));
        }
        emit dropAnimationRequested(variantDrops);
    }
    else
    {
        // 没有匹配 → 回滚动画
        std::swap(m_board[r1][c1], m_board[r2][c2]); // 恢复
        emit rollbackSwap(r1, c1, r2, c2);           // ✅ 新信号，只负责动画回滚
    }
}


void GameBoard::shuffleBoard() {
    qDebug() << "重新洗牌";
    initializeBoard();
}

void GameBoard::startGame() {
    qDebug() << "开始游戏";
    m_score = 0;
    initializeBoard();
    emit scoreChanged(m_score);
}

void GameBoard::resetGame() {
    qDebug() << "重新开始游戏";
    startGame();
}

QString GameBoard::getRandomColor() const {
    int idx = QRandomGenerator::global()->bounded(m_availableColors.size());
    return m_availableColors[idx];
}

// 修复const问题
bool GameBoard::isValidSwap(int r1, int c1, int r2, int c2) {
    // 检查边界
    if (r1 < 0 || r1 >= m_rows || c1 < 0 || c1 >= m_columns ||
        r2 < 0 || r2 >= m_rows || c2 < 0 || c2 >= m_columns) {
        return false;
    }

    // 检查是否相邻
    if ((qAbs(r1 - r2) == 1 && c1 == c2) || (qAbs(c1 - c2) == 1 && r1 == r2)) {
        return true;
    }

    return false;
}

QVector<QPoint> GameBoard::findMatches() const {
    QVector<QPoint> matches;
    QSet<QPoint> matchedSet;

    // 横向匹配
    for (int r = 0; r < m_rows; ++r) {
        for (int c = 0; c < m_columns - 2; ++c) {
            QString color = m_board[r][c];
            if (!color.isEmpty() &&
                color == m_board[r][c+1] &&
                color == m_board[r][c+2]) {
                matchedSet.insert(QPoint(r, c));
                matchedSet.insert(QPoint(r, c+1));
                matchedSet.insert(QPoint(r, c+2));
            }
        }
    }

    // 纵向匹配
    for (int c = 0; c < m_columns; ++c) {
        for (int r = 0; r < m_rows - 2; ++r) {
            QString color = m_board[r][c];
            if (!color.isEmpty() &&
                color == m_board[r+1][c] &&
                color == m_board[r+2][c]) {
                matchedSet.insert(QPoint(r, c));
                matchedSet.insert(QPoint(r+1, c));
                matchedSet.insert(QPoint(r+2, c));
            }
        }
    }

    return matchedSet.values().toVector();
}

// QVector<QVector<QPoint>> GameBoard::calculateDropPaths() const {
//     QVector<QVector<QPoint>> dropPaths;

//     // 简化实现：为每个需要掉落的方块创建路径
//     for (int c = 0; c < m_columns; ++c) {
//         for (int r = m_rows - 1; r >= 0; --r) {
//             if (m_board[r][c].isEmpty()) {
//                 // 查找上方第一个非空方块
//                 for (int above = r - 1; above >= 0; --above) {
//                     if (!m_board[above][c].isEmpty()) {
//                         QVector<QPoint> path;
//                         path.append(QPoint(above, c));
//                         path.append(QPoint(r, c));
//                         dropPaths.append(path);
//                         break;
//                     }
//                 }
//             }
//         }
//     }

//     return dropPaths;
// }

QVector<QVector<QPoint>> GameBoard::calculateDropPaths() const {
    QVector<QVector<QPoint>> dropPaths;

    for (int c = 0; c < m_columns; ++c) {
        int writeRow = m_rows - 1;

        for (int r = m_rows - 1; r >= 0; --r) {
            if (!m_board[r][c].isEmpty()) {
                if (r != writeRow) {
                    QVector<QPoint> path;
                    // 生成完整路径 (从 r 到 writeRow)
                    for (int rr = r; rr <= writeRow; ++rr) {
                        path.append(QPoint(rr, c));
                    }
                    dropPaths.append(path);
                }
                writeRow--;
            }
        }
    }
    return dropPaths;
}


void GameBoard::removeMatchedTiles(const QVector<QPoint> &matches) {
    for (const QPoint &pt : matches) {
        m_board[pt.x()][pt.y()] = ""; // 标记为空
    }
    updateScore(matches.size() * 10);
}

void GameBoard::updateScore(int points) {
    m_score += points;
    emit scoreChanged(m_score);
}
