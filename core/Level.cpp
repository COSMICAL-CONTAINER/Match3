#include "Level.h"
#include "Board.h"
#include <QDebug>

Level::Level(int id, QObject *parent)
    : QObject(parent), m_id(id)
{
    // 默认 8x8 棋盘
    m_board = new Board(8,8, this);
}

Level::~Level() {
    if (m_board) delete m_board;
}

void Level::loadFromDefinition(const QVariantMap &def) {
    Q_UNUSED(def);
    // TODO: parse layout, goals, initial tiles
}

void Level::reset() {
    // TODO: reset board state
}

bool Level::checkWin() const {
    Q_UNUSED(this);
    return false;
}

bool Level::checkLose() const {
    Q_UNUSED(this);
    return false;
}
