#ifndef GAMEBOARD_H
#define GAMEBOARD_H

#include <QObject>
#include <QVector>
#include <QString>
#include <QPoint>
#include <QVariant>
#include <QSet>

#define Rocket_UpDown    "Rocket_1"
#define Rocket_LeftRight "Rocket_2"
#define Bomb             "Bomb"
#define SuperItem        "SuperItem"

#define Rocket_UpDownType    1
#define Rocket_LeftRightType 2
#define BombType             3
#define SuperItemType        4

// 新增组合道具类型，用于在 propEffect 中编码复合激活（QML 识别并播放合成动画）
#define Combo_RocketRocketType 100
#define Combo_BombBombType     101
#define Combo_BombRocketType   102
#define Combo_SuperBombType    103 // 超级道具 + 炸弹
#define Combo_SuperRocketType  104 // 超级道具 + 火箭
#define Combo_SuperSuperType   105 // 超级道具 + 超级道具（两次全图涟漪清除）

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
    Q_INVOKABLE void commitDrop();

    // 道具处理
    Q_INVOKABLE void rocketEffectTriggered(int row, int col, int type);  // 火箭激活信号
    Q_INVOKABLE void bombEffectTriggered(int row, int col);              // 炸弹激活信号
    Q_INVOKABLE void superItemEffectTriggered(int row, int col, QString color);

    // 新增：组合道具触发（由 QML 在合成动画结束后调用）
    Q_INVOKABLE void rocketRocketTriggered(int row, int col);
    Q_INVOKABLE void bombBombTriggered(int row, int col);
    Q_INVOKABLE void bombRocketTriggered(int row, int col, int rocketType);
    Q_INVOKABLE void superBombTriggered(int row, int col);
    Q_INVOKABLE void superRocketTriggered(int row, int col);
    Q_INVOKABLE void superSuperTriggered(int row, int col); // 超级+超级触发

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
    bool m_comboPending; // 防止组合触发期间重复处理
    QVector<QVector<QString>> m_board;
    QVector<QString> m_availableColors;
    QVector<PropTypedef> rocketMatches;   // 用于记录火箭匹配
    QVector<PropTypedef> bombMatches;     // 用于记录炸弹匹配
    QVector<PropTypedef> superItemMatches; // 用于记录超级道具匹配
    QSet<QPoint> m_pendingActivations; // 记录已调度但尚未执行的道具激活位置

    // 延迟激活动作：用于在 4 连/5 连先生成新道具后，再激活旧道具
    struct DeferredActivation {
        int row = -1;
        int col = -1;
        int type = 0;
        QString color;
        bool valid = false;
    } m_deferredActivation;

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

    // 新增帮助函数
    bool isColor(const QString &color) const;    // 判断是否为常规颜色格子
    bool isMovable(const QString &color) const;  // 判断该格子是否应当参与下落（颜色或道具）
    bool isRocket(const QString &color) const;  // 判断是否为火箭道具
    bool isBomb(const QString &color) const;    // 判断是否为炸弹道具

    // 调度/连锁相关辅助
    void schedulePropEffect(int row, int col, int type, const QString &color, int delayMs);
    QString chooseNearbyColor(int row, int col) const;

    // 新增：道具激活后检查并处理新三消
    void checkAndProcessNewMatches();
};

#endif // GAMEBOARD_H
