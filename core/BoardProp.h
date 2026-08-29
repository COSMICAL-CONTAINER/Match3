#pragma once
#include "BoardItemBase.h"
#include <QString>
#include <QVariant>

// BoardProp: 棋盘上的特殊道具（火箭、炸弹、超级道具等），包含类型与激活接口
class BoardProp : public BoardItem {
public:
    enum class PropType { RocketUpDown, RocketLeftRight, Bomb, SuperItem };

    BoardProp();
    BoardProp(PropType t);

    PropType type() const { return m_type; }
    void setType(PropType t) { m_type = t; }

    // 激活道具时的行为由 Board 或 PropFactory 调用
    void activate(int row, int col);

private:
    PropType m_type{PropType::Bomb};
    QVariant m_meta; // 可用于记录组合信息或额外参数
};
