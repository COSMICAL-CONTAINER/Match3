#ifndef GAMEBOARD_H
#define GAMEBOARD_H

#include <QObject>
#include <QVector>
#include <QString>
#include <QPoint>
#include <QVariant>

class GameBoard : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int rows READ rows CONSTANT)
    Q_PROPERTY(int columns READ columns CONSTANT)
    Q_PROPERTY(int score READ score NOTIFY scoreChanged)

public:
    explicit GameBoard(QObject *parent = nullptr, int rows = 8, int columns = 8);

    // 游戏控制接口
    Q_INVOKABLE void startGame();
    Q_INVOKABLE void resetGame();
    Q_INVOKABLE void shuffleBoard();

    // 核心操作
    Q_INVOKABLE QString tileAt(int row, int col) const;
    Q_INVOKABLE void trySwap(int r1, int c1, int r2, int c2);
    Q_INVOKABLE void finalizeSwap(int r1, int c1, int r2, int c2);

    int rows() const { return m_rows; }
    int columns() const { return m_columns; }
    int score() const { return m_score; }

signals:
    void boardChanged();
    void scoreChanged(int newScore);

    // 动画信号 - 使用QVariantList替代复杂类型
    void swapAnimationRequested(int r1, int c1, int r2, int c2);
    void matchAnimationRequested(const QVariantList &matchedTiles);
    void dropAnimationRequested(const QVariantList &dropPaths);

    void invalidSwap(int r1, int c1, int r2, int c2);
    void rollbackSwap(int r1, int c1, int r2, int c2);
    void gameOver();

private:
    void initializeBoard();
    bool isValidSwap(int r1, int c1, int r2, int c2); // 移除const
    QVector<QPoint> findMatches() const;
    QVector<QVector<QPoint>> calculateDropPaths() const;
    void removeMatchedTiles(const QVector<QPoint> &matches);
    QString getRandomColor() const;
    void updateScore(int points);

private:
    int m_rows;
    int m_columns;
    int m_score;
    QVector<QVector<QString>> m_board;
    QVector<QString> m_availableColors;
};

#endif // GAMEBOARD_H
