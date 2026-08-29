#pragma once

#include "BoardModel.h"
#include "MatchFinder.h"
#include "Types.h"

namespace core {

enum class ComboType {
    None = 0,
    RocketRocket = 100,
    BombBomb     = 101,
    BombRocket   = 102,
    SuperBomb    = 103,
    SuperRocket  = 104,
    SuperSuper   = 105
};

class GameEngine {
public:
    GameEngine(int rows=8, int cols=8, int numColors=6);

    QString tileAt(int r, int c) const;
    QString getRandomColor() const;

    bool canSwap(int r1,int c1,int r2,int c2) const;
    MatchResult simulateSwapAndFindMatches(int r1,int c1,int r2,int c2) const;

    enum class FinalizeKind {
        None = 0,       // 非法交换或未产生任何效果
        NormalMatch,    // 普通交换产生匹配（三消及以上）
        SingleProp,     // 单体道具激活（道具 + 普通块）
        ComboProp       // 组合道具（预留，后续实现）
    };

    struct FinalizeResult {
        MatchResult result;               // 普通三消的匹配结果
        QVector<BoardModel::Drop> drops;  // 若某些路径直接做了清场，可在此返回掉落信息（目前 SingleProp 不使用）
        FinalizeKind kind = FinalizeKind::None;

        // 道具相关信息（用于 SingleProp/ComboProp） :
        int propRow = -1;
        int propCol = -1;
        PropType propType = PropType::None;
        uint8_t propColorIndex = 0; // 超级道具用 : 1~numColors 的基础颜色索引

        ComboType comboType = ComboType::None;
        int comboRocketType = 0; // 1=纵向火箭,2=横向火箭（用于炸弹+火箭）
    };

    // applies swap only if matches found; returns matched tiles and drop paths
    FinalizeResult finalizeSwap(int r1,int c1,int r2,int c2);

    // 掉落/连锁阶段：在“不发生交换”的情况下探测当前棋盘是否存在匹配。
    // 若存在匹配，返回 kind=NormalMatch 且 result 填充 matched。
    FinalizeResult finalizeNoSwap();

    // remove matches and return drops so frontend can animate
    QVector<BoardModel::Drop> removeMatches(const MatchResult &mr);

    // process exactly one cascade step: find matches on current board, if any
    // remove them, apply gravity+refill and return ONLY this step's drops.
    // If no matches, returns an empty vector.
    QVector<BoardModel::Drop> processOneCascadeStep();

    // process all matches in cascade until no more matches; collect all drops
    QVector<BoardModel::Drop> processAllMatches();

    void startGame();
    void resetGame();
    void shuffleBoard();

    int score() const { return m_score; }
    int step() const { return m_step; }

    // 新增 : 单体道具激活接口
    // 火箭 : type = PropType::RocketHorizontal / RocketVertical
    QVector<BoardModel::Drop> activateRocket(int row, int col, PropType type);
    // 炸弹 : 以 (row,col) 为中心的圆形范围清除
    QVector<BoardModel::Drop> activateBomb(int row, int col);
    // 超级道具 : 清除全盘指定颜色（colorIndex 为 1~numColors 的基础颜色索引）
    QVector<BoardModel::Drop> activateSuper(int row, int col, uint8_t colorIndex);

    // 组合道具：火箭 + 火箭
    QVector<BoardModel::Drop> activateComboRocketRocket(int row, int col);
    QVector<BoardModel::Drop> activateComboBombBomb(int row, int col);
    QVector<BoardModel::Drop> activateComboBombRocket(int row, int col, int legacyRocketType);
    QVector<BoardModel::Drop> activateComboSuperBomb(int row, int col);
    QVector<BoardModel::Drop> activateComboSuperRocket(int row, int col);
    QVector<BoardModel::Drop> activateComboSuperSuper(int row, int col);

    // 新增：用于两阶段组合的第二阶段执行接口（仅供 GameBoardCompat/QML 调用）
    QVector<BoardModel::Drop> executeComboSuperBomb(int row, int col);
    QVector<BoardModel::Drop> executeComboSuperRocket(int row, int col);

private:
    // 道具链式触发辅助（爆炸/火箭命中其它道具会继续触发）
    uint8_t pickAnyBasicColorNear(int row, int col) const;

    struct PropTrigger {
        int r = -1;
        int c = -1;
        uint8_t code = 0;
        uint8_t superColor = 0;
        bool isCombo = false;
        ComboType comboType = ComboType::None;
        int legacyRocketType = 0;
    };

    void runPropChain(QVector<PropTrigger> &queue);

    BoardModel m_board;
    int m_score = 0;
    int m_step = 0;
};

} // namespace core
