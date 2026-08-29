#pragma once
#include <QObject>
#include <QVector>
#include <QString>
#include <QPoint>
#include <QVariantList>
#include <cstdint>
#include "GameBoard.h"

// forward declare core::GameEngine without polluting class scope
namespace core { class GameEngine; class BoardModel; enum class PropType; }

// 兼容层：提供与原 GameBoard 兼容的最小接口，基于新的 core 结构可逐步替换
class GameBoardCompat : public QObject {
    Q_OBJECT
    Q_PROPERTY(int rows READ rows CONSTANT)
    Q_PROPERTY(int columns READ columns CONSTANT)
    Q_PROPERTY(int score READ score NOTIFY scoreChanged)
    Q_PROPERTY(int step READ step NOTIFY stepChanged)
    Q_PROPERTY(int init_step READ init_step WRITE setInitStep NOTIFY init_stepChanged)
    Q_PROPERTY(int comboCount READ comboCount NOTIFY comboChanged)
public:
    explicit GameBoardCompat(QObject *parent = nullptr, int rows = 8, int columns = 8);

    // compact color type stored as uint8_t
    enum class Color : uint8_t { Transparent = 0, Red = 1, Green = 2, Blue = 3, Yellow = 4, Purple = 5, Brown = 6 };

    int rows() const { return m_rows; }
    int columns() const { return m_columns; }
    int score() const { return m_score; }
    int step() const { return m_step; }
    int init_step() const { return m_init_step; }
    void setInitStep(int v) { m_init_step = v; emit init_stepChanged(m_init_step); }
    int comboCount() const { return m_comboCnt; }

    QVariantList stats() const {
        QVariantList list; list.reserve(15);
        for (int i=0;i<15;++i) list.append(m_stats[i]);
        return list;
    }

    // 新增：道具连锁触发预演（仅用于动画，不会修改棋盘）
    // 返回: [{row:int,col:int,type:int,color:string}, ...]，type 使用 GameBoard.h 常量
    Q_INVOKABLE QVariantList previewPropChainFrom(int row, int col, int type, const QString &colorKey = "");

    Q_INVOKABLE QString tileAt(int row, int col) const;
    Q_INVOKABLE QString getTileColor(int row, int col) const; // compatibility for QML
    Q_INVOKABLE QString getRandomColorQml() const; // compatibility helper for QML drop visuals
    Q_INVOKABLE void startGame();
    Q_INVOKABLE void resetGame();
    Q_INVOKABLE void shuffleBoard();
    Q_INVOKABLE void trySwap(int r1,int c1,int r2,int c2);
    Q_INVOKABLE void finalizeSwap(int r1,int c1,int r2,int c2, bool isRecursion = false);

    // Compatibility stubs used by QML
    Q_INVOKABLE void processMatches();
    Q_INVOKABLE void processDrop();
    Q_INVOKABLE void commitDrop();

    // QML 动画结束后回调：单体道具激活
    Q_INVOKABLE void rocketEffectTriggered(int row, int col, int type);
    Q_INVOKABLE void bombEffectTriggered(int row, int col);
    Q_INVOKABLE void superItemEffectTriggered(int row, int col, QString color);

    // QML 动画结束后回调：组合道具（火箭+火箭）激活
    Q_INVOKABLE void comboRocketRocketEffectTriggered(int row, int col);
    Q_INVOKABLE void comboBombBombEffectTriggered(int row, int col);
    Q_INVOKABLE void comboBombRocketEffectTriggered(int row, int col, int rocketType);
    Q_INVOKABLE void comboSuperBombEffectTriggered(int row, int col);
    Q_INVOKABLE void comboSuperRocketEffectTriggered(int row, int col);
    Q_INVOKABLE void comboSuperSuperEffectTriggered(int row, int col);

    // 新增：超级组合的第二阶段执行接口
    Q_INVOKABLE void executeComboSuperBomb(int row, int col);
    Q_INVOKABLE void executeComboSuperRocket(int row, int col);

    Q_INVOKABLE QVariantList debugFindPossibleSwaps() const;

signals:
    void boardChanged();
    void scoreChanged(int newScore);
    void stepChanged(int step);
    void init_stepChanged(int init_step);
    void comboChanged(int comboCount);
    void statsChanged(const QVariantList &stats);

    // existing animation/flow signals
    void swapAnimationRequested(int r1, int c1, int r2, int c2);
    void matchAnimationRequested(const QVariantList &matchedTiles);
    void invalidSwap(int r1, int c1, int r2, int c2);
    void rollbackSwap(int r1, int c1, int r2, int c2);
    void propEffect(int row, int col, int type, QString color);
    void propEffectRequested(int row, int col, int type, const QString &color);
    
    // drop animation signal: list of maps {fromR,fromC,toR,toC,color,isNew}
    void dropAnimationRequested(const QVariantList &dropPaths);

    // compatibility signals used by QML
    void rocketCreateRequested(const QVariantList &matches);
    void bombCreateRequested(const QVariantList &matches);
    void superItemCreateRequested(const QVariantList &matches);
    void gameOver();

private:
    void initializeBoard();
    QVector<QPoint> findMatchesForSwap(int r1,int c1,int r2,int c2) const;

    int m_rows;
    int m_columns;
    int m_score = 0;
    int m_step = 0;
    int m_init_step = 25;
    int m_comboCnt = 0;
    int m_stats[15] = {0};
    // store colors compactly as uint8_t values corresponding to Color enum
    QVector<QVector<uint8_t>> m_board;
    int m_numColors = 6; // number of playable colors

    // last matched tiles (populated in finalizeSwap) so processMatches can remove them
    QVector<QPoint> m_lastMatches;

    // Legacy GameBoard instance: delegate to original implementation for full behavior
    GameBoard *m_legacy = nullptr;

    // Core engine: new backend implementation (doesn't modify GameBoard.cpp)
    core::GameEngine *m_engine = nullptr;
};
