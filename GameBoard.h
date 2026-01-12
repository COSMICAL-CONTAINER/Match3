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
    Q_PROPERTY(int step READ step NOTIFY stepChanged)
    Q_PROPERTY(int init_step READ init_step WRITE setInitStep NOTIFY init_stepChanged) // 将 init_step 设为可写
    Q_PROPERTY(int comboCount READ comboCount NOTIFY comboChanged)
    Q_PROPERTY(QVariantList stats READ stats NOTIFY statsChanged)

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

    // 统计接口：返回 15 项统计（六色 + 三普通道具触发 + 六组合道具触发）
    Q_INVOKABLE int statAt(int n) const { return (n>=0 && n<15) ? m_stats[n] : 0; }
    Q_INVOKABLE void addStatAt(int n) { if (n>=0 && n<15) { m_stats[n]++; emit statsChanged(stats()); } }
    QVariantList stats() const {
        QVariantList list; list.reserve(15);
        for (int i=0;i<15;++i) list.append(m_stats[i]);
        return list;
    }

    int rows() const { return m_rows; }
    int columns() const { return m_columns; }
    int score() const { return m_score; }
    int step() const { return m_step;}
    int init_step() const { return m_init_step;}
    int comboCount() const { return m_comboCnt; }

    void setInitStep(int v) { m_init_step = v; emit init_stepChanged(m_init_step); }

signals:
    void boardChanged();
    void scoreChanged(int newScore);
    void stepChanged(int step);
    void init_stepChanged(int init_step);
    void comboChanged(int comboCount);  // 发送连击数
    // 新增：统计变化通知
    void statsChanged(const QVariantList &stats);

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
    int m_init_step = 25;
    int m_comboCnt;
    int m_rows;
    int m_columns;
    int m_score;
    int m_step;
    bool m_comboPending; // 防止组合触发期间重复处理
    QVector<QVector<QString>> m_board;
    QVector<QString> m_availableColors;
    QVector<PropTypedef> rocketMatches;   // 用于记录火箭匹配
    QVector<PropTypedef> bombMatches;     // 用于记录炸弹匹配
    QVector<PropTypedef> superItemMatches; // 用于记录超级道具匹配
    QSet<QPoint> m_pendingActivations; // 记录已调度但尚未执行的道具激活位置

    // 新增：记录最近一次组合参与的两个位置，避免组合后再次单体激活
    QSet<QPoint> m_comboParticipants;

    // 延迟激活动作：用于在 4 连/5 连先生成新道具后，再激活旧道具
    struct DeferredActivation {
        int row = -1;
        int col = -1;
        int type = 0;
        QString color;
        bool valid = false;
    } m_deferredActivation;

    // 新增：统计数组
    // 0-5= 颜色
    // 6  = 火箭触发
    // 7  = 炸弹触发
    // 8  = 超级触发
    // 9  = 火箭+火箭
    // 10 = 炸弹+炸弹
    // 11 = 炸弹+火箭
    // 12 = 超级触发+火箭
    // 13 = 超级触发+炸弹
    // 14 = 超级触发+超级触发
    int m_stats[15] = {0};

    // 新增：保留由一次交换产生的道具集合，避免随后自动检测再重复发射创建信号
    bool m_reservedPropsPending = false;
    QVector<PropTypedef> m_reservedRocketMatches;
    QVector<PropTypedef> m_reservedBombMatches;
    QVector<PropTypedef> m_reservedSuperItemMatches;

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
    void updateStep(int step);

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

    // 新增：组合参与点的标记清理
    inline void markComboParticipants(int r1, int c1, int r2, int c2) {
        m_comboParticipants.insert(QPoint(r1, c1));
        m_comboParticipants.insert(QPoint(r2, c2));
    }
    inline void clearComboParticipants() { m_comboParticipants.clear(); }

};

#endif // GAMEBOARD_H
