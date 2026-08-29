#include "BoardTile.h"

BoardTile::BoardTile() { kind = Kind::Tile; }

BoardTile::BoardTile(const QString &color) : m_color(color) { kind = Kind::Tile; }

bool BoardTile::isMovable() const {
    return !m_color.isEmpty();
}
