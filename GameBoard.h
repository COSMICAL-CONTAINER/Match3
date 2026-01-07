#ifndef GAMEBOARD_H
#define GAMEBOARD_H

#include <QObject>
#include <QVector>
#include <QString>
#include <QPoint>
#include <QVariant>

#define Rocket_UpDown    "Rocket_1"
#define Rocket_LeftRight "Rocket_2"
#define Bomb             "Bomb"
#define SuperItem        "SuperItem"

#define Rocket_UpDownType    1
#define Rocket_LeftRightType 2
#define BombType             3
#define SuperItemType        4

typedef struct
{
    int type;
    QPoint point;
}PropTypedef;

class GameBoard : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int rows READ rows CONSTANT)
    Q_PROPERTY(int columns READ columns CONSTANT)
    Q_PROPERTY(int score READ score NOTIFY scoreChanged)
    Q_PROPERTY(int comboCount READ comboCount NOTIFY comboChanged)

public:
    explicit GameBoard(QObject *parent = nullptr, int rows = 8, int columns = 8);

    // 游戏控制接口
    Q_INVOKABLE void startGame();
    Q_INVOKABLE void resetGame();
    Q_INVOKABLE void shuffleBoard();

    // 核心操作
    Q_INVOKABLE QString tileAt(int row, int col) const;
    Q_INVOKABLE void trySwap(int r1, int c1, int r2, int c2);
    Q_INVOKABLE void finalizeSwap(int r1, int c1, int r2, int c2, bool isRecursion);
    Q_INVOKABLE void processMatches();
    Q_INVOKABLE void processDrop();

    // 道具处理
    Q_INVOKABLE void rocketEffectTriggered(int row, int col, int type);  // 火箭激活信号
    Q_INVOKABLE void bombEffectTriggered(int row, int col);              // 炸弹激活信号
    Q_INVOKABLE void superItemEffectTriggered(int row, int col, QString color);

    int rows() const { return m_rows; }
    int columns() const { return m_columns; }
    int score() const { return m_score; }
    int comboCount() const { return m_comboCnt; }

signals:
    void boardChanged();
    void scoreChanged(int newScore);
    void comboChanged(int comboCount);  // 发送连击数

    // 基础动画信号
    void swapAnimationRequested(int r1, int c1, int r2, int c2);
    void matchAnimationRequested(const QVariantList &matchedTiles);
    void dropAnimationRequested(const QVariantList &dropPaths);

    void invalidSwap(int r1, int c1, int r2, int c2);
    void rollbackSwap(int r1, int c1, int r2, int c2);
    void gameOver();

    // 道具添加请求前端动画信号
    void rocketCreateRequested(const QVector<PropTypedef> &rocketMatches);
    void bombCreateRequested(const QVector<PropTypedef> &bombMatches);
    void superItemCreateRequested(const QVector<PropTypedef> &superItemMatches);

    // 交换后道具激活信号
    void propEffect(int row, int col, int type, QString color);

private:
    int m_comboCnt;
    int m_rows;
    int m_columns;
    int m_score;
    QVector<QVector<QString>> m_board;
    QVector<QString> m_availableColors;
    QVector<PropTypedef> rocketMatches;   // 用于记录火箭匹配
    QVector<PropTypedef> bombMatches;     // 用于记录炸弹匹配
    QVector<PropTypedef> superItemMatches; // 用于记录超级道具匹配

    void initializeBoard();
    void fillNewTiles();
    bool isProp(QString color);
    bool isValidSwap(int r1, int c1, int r2, int c2); // 移除const
    QVector<QPoint> findMatches(int r1, int c1, int r2, int c2, bool argflag);
    QVector<QVector<QPoint>> calculateDropPaths() const;
    void removeMatchedTiles(const QVector<QPoint> &matches);
    void creatProp();
    void applyGravity();
    QString getRandomColor() const;
    void updateScore(int points);

    // 新增私有方法
    QVector<PropTypedef> findBombMatches();
    QVector<PropTypedef> findRocketMatches(int r1, int c1, int r2, int c2);
    QVector<PropTypedef> findSuperItemMatches();
    void clearRow(int row);    // 清除行
    void clearColumn(int col); // 清除列
};

#endif // GAMEBOARD_H
