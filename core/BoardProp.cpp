#include "BoardProp.h"

BoardProp::BoardProp() { kind = Kind::Prop; }

BoardProp::BoardProp(PropType t) : m_type(t) { kind = Kind::Prop; }

void BoardProp::activate(int row, int col) {
    Q_UNUSED(row); Q_UNUSED(col);
    // TODO: implement activation behavior
}
