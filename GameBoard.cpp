#include "GameBoard.h"
#include <QDebug>
#include <cstdlib>  // for std::rand, std::srand
#include <ctime>    // for std::time

GameBoard::GameBoard(QObject *parent, int rows, int columns)
    : QObject(parent), m_rows(rows), m_columns(columns)
{
    m_board.resize(m_rows);

    GameInit();
}

void GameBoard::GameInit()
{
    // 初始化随机种子
    std::srand(static_cast<unsigned>(std::time(nullptr)));

    // 可用颜色列表（不含空气）
    QVector<QString> colors = { "red", "green", "blue", "yellow", "purple" };

    for (int r = 0; r < m_rows; ++r) {
        m_board[r].resize(m_columns);
        for (int c = 0; c < m_columns; ++c) {
            int idx = std::rand() % colors.size();
            m_board[r][c] = colors[idx]; // 随机颜色
        }
    }
    // 检查消除
    QVector<QVector<bool>> toRemove(m_rows, QVector<bool>(m_columns, false));

    while (checkMatches(toRemove))
    {
        // 执行消除，生成空格
        for (int r = 0; r < m_rows; ++r)
            for (int c = 0; c < m_columns; ++c)
                if (toRemove[r][c])
                    m_board[r][c] = "white";
        dropTiles();
        generateNewTiles();
        for (int r = 0; r < m_rows; ++r)
            toRemove[r].fill(false); // 清零，不重新分配
    }
}

QString GameBoard::tileAt(int row, int col) const
{
    if (row < 0 || row >= m_rows || col < 0 || col >= m_columns) return NULL;
    return m_board[row][col];
}

void GameBoard::trySwap(int r1, int c1, int r2, int c2)
{
    // 边界检查
    if (r2 < 0 || r2 >= m_rows || c2 < 0 || c2 >= m_columns) return;
    // if (m_board[r1][c1] == "white" || m_board[r2][c2] == "white") return;

    // 保存棋盘快照
    QVector<QVector<QString>> backup = m_board;

    // 执行交换
    std::swap(m_board[r1][c1], m_board[r2][c2]);

    // 检查消除
    QVector<QVector<bool>> toRemove(m_rows, QVector<bool>(m_columns, false));
    if (!checkMatches(toRemove))
    {
        m_board = backup; // 无效交换
        emit invalidSwap(r1, c1, r2, c2);
        return;
    }

    bool hasMatch = true;
    while (hasMatch)
    {
        hasMatch = checkMatches(toRemove);
        if (hasMatch)
        {
            for (int r = 0; r < m_rows; ++r)
                for (int c = 0; c < m_columns; ++c)
                    if (toRemove[r][c])
                        m_board[r][c] = "white";

            dropTiles();
            generateNewTiles();
            emit boardChanged();

            for (int r = 0; r < m_rows; ++r)
                toRemove[r].fill(false); // 清零，不重新分配
        }
    }
}


bool GameBoard::checkMatches(QVector<QVector<bool>> &toRemove)
{
    // 横向检查
    for (int r = 0; r < m_rows; ++r) {
        for (int c = 0; c <= m_columns - 3; ++c) {
            QString color = m_board[r][c];
            if (color != "white" &&
                color == m_board[r][c+1] &&
                color == m_board[r][c+2])
            {
                toRemove[r][c] = toRemove[r][c+1] = toRemove[r][c+2] = true;
            }
        }
    }

    // 纵向检查
    for (int c = 0; c < m_columns; ++c) {
        for (int r = 0; r <= m_rows - 3; ++r) {
            QString color = m_board[r][c];
            if (color != "white" &&
                color == m_board[r+1][c] &&
                color == m_board[r+2][c])
            {
                toRemove[r][c] = toRemove[r+1][c] = toRemove[r+2][c] = true;
            }
        }
    }

    // 如果有检测到的地方
    for (int r = 0; r < m_rows; ++r)
        for (int c = 0; c < m_columns; ++c)
            if (toRemove[r][c])
                return true;
    return false;
}

void GameBoard::dropTiles() {
    for (int c = 0; c < m_columns; ++c) {
        int writeRow = m_rows - 1;

        // 压下非空 tile
        for (int r = m_rows - 1; r >= 0; --r) {
            if (m_board[r][c] != "white") {
                if (writeRow != r) {
                    m_board[writeRow][c] = m_board[r][c];
                    m_board[r][c] = "white";
                    // 发信号通知 QML 更新 tile 下落和颜色
                    emit tileDropped(r, c, writeRow, c, m_board[writeRow][c]);
                }
                writeRow--;
            }
        }

        // 顶部生成新的 tile（白色）并发信号
        for (int r = writeRow; r >= 0; --r) {
            m_board[r][c] = "white";
            // qDebug() << "r:" + QString::number(r) << " c:" + QString::number(c);
            emit tileDropped(-1, c, r, c, m_board[r][c]);
        }
    }
}

void GameBoard::generateNewTiles() {
    for (int c = 0; c < m_columns; ++c) {
        for (int r = 0; r < m_rows; ++r) {
            if (m_board[r][c] == "white") {
                m_board[r][c] = "white"; // 可以改成随机颜色
                emit tileDropped(-1, c, r, c, m_board[r][c]);
            }
        }
    }
}



#include <algorithm>
#include <random>

void GameBoard::shuffleTiles()
{
    // 先把棋盘所有 tile 收集到一个一维数组
    std::vector<QString> tiles;
    for (int r = 0; r < m_rows; ++r) {
        for (int c = 0; c < m_columns; ++c) {
            tiles.push_back(m_board[r][c]);
        }
    }

    // 随机打乱
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(tiles.begin(), tiles.end(), g);

    // 再重新填回棋盘
    int idx = 0;
    for (int r = 0; r < m_rows; ++r) {
        for (int c = 0; c < m_columns; ++c) {
            m_board[r][c] = tiles[idx++];
            emit tileDropped(-1, c, r, c, m_board[r][c]); // 通知 QML 更新
        }
    }

    // 检查消除
    QVector<QVector<bool>> toRemove(m_rows, QVector<bool>(m_columns, false));
    while (checkMatches(toRemove))
    {
        // 执行消除，生成空格
        for (int r = 0; r < m_rows; ++r)
            for (int c = 0; c < m_columns; ++c)
                if (toRemove[r][c])
                    m_board[r][c] = "white";
        dropTiles();
        generateNewTiles();
        for (int r = 0; r < m_rows; ++r)
            toRemove[r].fill(false); // 清零，不重新分配
    }
}
