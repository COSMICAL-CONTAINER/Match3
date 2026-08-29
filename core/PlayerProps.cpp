#include "PlayerProps.h"

PlayerProps::PlayerProps(QObject *parent)
    : QObject(parent)
{
}

bool PlayerProps::useHammer() {
    if (m_inv.hammer <= 0) return false;
    m_inv.hammer--; emit inventoryChanged(); return true;
}

bool PlayerProps::useRowRocket() {
    if (m_inv.rowRocket <= 0) return false;
    m_inv.rowRocket--; emit inventoryChanged(); return true;
}

bool PlayerProps::useColRocket() {
    if (m_inv.colRocket <= 0) return false;
    m_inv.colRocket--; emit inventoryChanged(); return true;
}

bool PlayerProps::useShuffle() {
    if (m_inv.shuffle <= 0) return false;
    m_inv.shuffle--; emit inventoryChanged(); return true;
}

void PlayerProps::addHammer(int n) { m_inv.hammer += n; emit inventoryChanged(); }
void PlayerProps::addRowRocket(int n) { m_inv.rowRocket += n; emit inventoryChanged(); }
void PlayerProps::addColRocket(int n) { m_inv.colRocket += n; emit inventoryChanged(); }
void PlayerProps::addShuffle(int n) { m_inv.shuffle += n; emit inventoryChanged(); }
