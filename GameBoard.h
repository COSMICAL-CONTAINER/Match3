#ifndef GAMEBOARD_H
#define GAMEBOARD_H

#include <QObject>
#include <QVector>
#include <QString>

class GameBoard : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int rows READ rows CONSTANT)
    Q_PROPERTY(int columns READ columns CONSTANT)
public:
    explicit GameBoard(QObject *parent = nullptr, int rows = 8, int columns = 8);

    Q_INVOKABLE void GameInit();
    Q_INVOKABLE QString tileAt(int row, int col) const;
    Q_INVOKABLE void trySwap(int r1, int c1, int r2, int c2);
    Q_INVOKABLE void dropTiles();          // 下落逻辑
    Q_INVOKABLE void shuffleTiles();

    int rows() const { return m_rows; }
    int columns() const { return m_columns; }

signals:
    void boardChanged();
    void invalidSwap(int row1, int col1, int row2, int col2);
    void tileDropped(int fromRow, int fromCol, int toRow, int toCol, const QString &color);
    // 在真正移除之前，先告诉 QML 哪些单元要被移除（用于播放消除动画）
    void aboutToRemoveTiles(const QVariantList &positions);
public slots:
    // QML 动画完成后调用，真正执行移除/下落并继续链式消除
    // Q_INVOKABLE void applyRemovals();

private:
    QVector<QVector<bool>> m_pendingToRemove; // 临时保存要被移除的位置，等待 QML 动画确认
    bool m_waitingForQml = false;
    QVector<QString> m_colorList = { "red", "green", "blue", "yellow", "purple" }; // 可重用
    bool checkMatches(QVector<QVector<bool>> &toRemove);       // 检查消除

    void generateNewTiles();   // 生成新方块（目前全部白色）

    int m_rows;
    int m_columns;
    QVector<QVector<QString>> m_board;
};



#endif // GAMEBOARD_H


