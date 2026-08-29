#pragma once
#include <QObject>
#include <QVariantMap>

class Board;
class GoalCounter;

// Level: 代表一个关卡实例，持有 Board 与胜利条件/计数器
class Level : public QObject {
    Q_OBJECT
public:
    explicit Level(int id, QObject *parent=nullptr);
    ~Level();

    void loadFromDefinition(const QVariantMap &def); // 加载关卡定义（布局、目标、初始物品）
    void reset();

    bool checkWin() const;
    bool checkLose() const;

    Board* board() const { return m_board; }
    int id() const { return m_id; }

signals:
    void win();
    void lose();

private:
    int m_id;
    Board* m_board = nullptr;
    // GoalCounter* m_goals = nullptr; // 可在后续扩展
    int m_moveLimit = 0;
};
