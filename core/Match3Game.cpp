#include "Match3Game.h"
#include "Level.h"
#include "PlayerProps.h"

Match3Game::Match3Game(QObject *parent)
    : QObject(parent)
{
    m_playerProps = new PlayerProps(this);
}

void Match3Game::startLevel(int levelId) {
    if (m_level) delete m_level;
    m_level = new Level(levelId, this);
    emit levelStarted(levelId);
}

void Match3Game::restartLevel() {
    if (m_level) m_level->reset();
}

void Match3Game::pause() {
    // TODO: pause timers / audio
}

void Match3Game::resume() {
    // TODO: resume timers / audio
}
