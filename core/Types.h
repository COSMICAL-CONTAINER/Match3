#pragma once

#include <QPoint>
#include <QVector>
#include <QString>

namespace core {

enum class PropType {
    None = 0,
    RocketHorizontal = 1,
    RocketVertical = 2,
    BombProp = 3,
    SuperProp = 4
};

struct PropSuggestion {
    QPoint pos;    // position to create the prop
    PropType type = PropType::None;
    QString color; // used for super-item (optional)
};

struct MatchResult {
    QVector<QPoint> matched;             // list of matched tile coordinates
    QVector<PropSuggestion> suggestions; // suggested props to create
};

struct LastSwapInfo {
    QPoint a;
    QPoint b;
    LastSwapInfo() {}
    LastSwapInfo(int r1, int c1, int r2, int c2) : a(c1, r1), b(c2, r2) {}
};

} // namespace core

// ===== 旧版道具类型常量（QML 动画协议使用，保持与历史 GameBoard.h 一致）=====
static constexpr int Rocket_UpDownType    = 1;
static constexpr int Rocket_LeftRightType = 2;
static constexpr int BombType             = 3;
static constexpr int SuperItemType        = 4;

// 组合道具类型，用于在 propEffect 中编码复合激活（QML 识别并播放合成动画）
static constexpr int Combo_RocketRocketType = 100;
static constexpr int Combo_BombBombType     = 101;
static constexpr int Combo_BombRocketType   = 102;
static constexpr int Combo_SuperBombType    = 103; // 超级道具 + 炸弹
static constexpr int Combo_SuperRocketType  = 104; // 超级道具 + 火箭
static constexpr int Combo_SuperSuperType   = 105; // 超级道具 + 超级道具
