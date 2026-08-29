#pragma once
#include <QObject>
#include <QVector>
#include <QPoint>
#include <optional>
#include <QString>

// LastSwapInfo: 记录一次交换的上下文，便于 MatchFinder 与 Board 协同决策
struct LastSwapInfo {
    QPoint a{-1,-1};
    QPoint b{-1,-1};
    bool valid{false};
    QString preA; // 交换前 a 的内容（可选）
    QString preB; // 交换前 b 的内容（可选）
};

class BoardItem;

// Board: 管理格子、有效掩码、掉落、掉落路径、序列化等核心逻辑
class Board : public QObject {
    Q_OBJECT
public:
    explicit Board(int rows = 8, int cols = 8, QObject *parent=nullptr);
    ~Board();

    int rows() const;
    int cols() const;

    void setShapeMask(const QVector<QVector<bool>>& mask); // 支持非矩形地图

    BoardItem* cellAt(int r, int c) const;

    // 核心操作
    bool swapTiles(int r1,int c1,int r2,int c2, LastSwapInfo* outSwap=nullptr);
    QVector<QPoint> findMatches(const LastSwapInfo* swap = nullptr) const; // 简化签名
    QVector<QPair<int,int>> applyGravity();
    void fillNewTiles();

signals:
    void tilesChanged();
    void propCreated(int row, int col, int type);

private:
    int m_rows;
    int m_cols;
    QVector<QVector<BoardItem*>> m_cells;
    QVector<QVector<bool>> m_mask; // true 表示该位置有效
};
