#pragma once
#include <QObject>
#include <QPoint>
#include <QVariant>

class Level;
class PlayerProps;

// Match3Game: 高层控制器，负责流程、关卡切换、保存/加载、与 UI 的信号交互
class Match3Game : public QObject {
    Q_OBJECT
public:
    explicit Match3Game(QObject *parent = nullptr);

    // 游戏流程
    Q_INVOKABLE void startLevel(int levelId);
    Q_INVOKABLE void restartLevel();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void resume();

    Level* currentLevel() const { return m_level; }
    PlayerProps* playerProps() const { return m_playerProps; }

signals:
    void levelStarted(int levelId);
    void levelCompleted(int levelId, bool win);
    void requestSave();
    void requestLoad();

private:
    Level* m_level = nullptr;
    PlayerProps* m_playerProps = nullptr;
};
