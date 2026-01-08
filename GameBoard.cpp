#include "GameBoard.h"
#include <QRandomGenerator>
#include <QDebug>
#include <QVariant>
#include <QList>
#include <QVector>
#include <QPoint>
#include <QTimer>

#include <QPoint>
// 实现 qHash 重载
inline uint qHash(const QPoint &point, uint seed = 0) {
    // 使用一个简单的哈希函数，通常是组合 x 和 y 坐标
    return qHash(point.x(), seed) ^ qHash(point.y(), seed);
}

GameBoard::GameBoard(QObject *parent, int rows, int columns)
    : QObject(parent), m_comboCnt(0), m_rows(rows), m_columns(columns), m_score(0), m_comboPending(false)
{
    m_availableColors = {"red", "green", "blue", "yellow", "purple"};
    initializeBoard();
}

// void GameBoard::initializeBoard() {
//     m_board.resize(m_rows);
//     for (int r = 0; r < m_rows; ++r) {
//         m_board[r].resize(m_columns);
//         for (int c = 0; c < m_columns; ++c)
//             m_board[r][c] = getRandomColor();
//     }
//     emit boardChanged();
//     finalizeSwap(0,0,0,0,true);
// }

void GameBoard::initializeBoard() {
    // 只进行棋盘初始化，不执行掉落或三消逻辑
    m_board.resize(m_rows);

    // 循环生成棋盘，直到没有三消
    do {
        for (int r = 0; r < m_rows; ++r) {
            m_board[r].resize(m_columns);
            for (int c = 0; c < m_columns; ++c) {
                m_board[r][c] = getRandomColor();
            }
        }
    } while (!findMatches(0, 0, 0, 0, false).isEmpty()); // 检查是否有匹配，如果有匹配则重新生成棋盘

    emit boardChanged();  // 刷新棋盘
}

// 补新方块
void GameBoard::fillNewTiles() {
    int rows = m_board.size();
    int cols = m_board[0].size();

    // 对每一列分别处理，跳过不可移动(背景)格子
    for (int c = 0; c < cols; ++c) {
        int r = rows - 1;
        while (r >= 0) {
            // 跳过不可移动(块)位置，找到一个连续的可移动区段
            if (!m_board[r][c].isEmpty() && !isMovable(m_board[r][c])) {
                r--;
                continue;
            }

            int segmentBottom = r;
            int segmentTop = r;
            while (segmentTop >= 0 && (m_board[segmentTop][c].isEmpty() || isMovable(m_board[segmentTop][c]))) {
                // 只要是空或者可移动就继续
                segmentTop--;
            }
            segmentTop++;

            // 在区段内，从顶部到底部填充空位
            for (int rr = segmentTop; rr <= segmentBottom; ++rr) {
                if (m_board[rr][c].isEmpty()) {
                    QPoint pt(rr, c);
                    // 如果该位置有挂起的道具激活，不要覆盖，留空等待激活
                    if (m_pendingActivations.contains(pt)) {
                        qDebug() << "fillNewTiles: skip filling pending activation at" << pt;
                        continue; // 保持为空，等待激活
                    }
                    m_board[rr][c] = getRandomColor();
                }
            }

            r = segmentTop - 1;
        }
    }
}

QString GameBoard::tileAt(int row, int col) const {
    if (row < 0 || row >= m_rows || col < 0 || col >= m_columns)
        return "transparent";
    return m_board[row][col];
}

void GameBoard::trySwap(int r1, int c1, int r2, int c2) {
    qDebug() << "尝试交换:" << r1 << c1 << "->" << r2 << c2;

    if (!isValidSwap(r1, c1, r2, c2)) {
        qDebug() << "无效交换";
        emit invalidSwap(r1, c1, r2, c2);   // ❌ 只告诉 QML 播动画
        return;
    }

    emit swapAnimationRequested(r1, c1, r2, c2);        // 播放请求交换动画
}


Q_INVOKABLE void GameBoard::finalizeSwap(int r1, int c1, int r2, int c2, bool isRecursion)
{
    qDebug() << "finalizeSwap called:" << QPoint(r1,c1) << "->" << QPoint(r2,c2) << "isRecursion:" << isRecursion;

    // 如果当前有组合正在等待视觉显示或处理，跳过额外的 finalize 调用以避免重复/递归
    if (m_comboPending) {
        qDebug() << "finalizeSwap: combo pending, skipping to avoid re-entry";
        return;
    }

    QString preA = ""; QString preB = "";
    if (!isRecursion) {
        preA = m_board[r1][c1];
        preB = m_board[r2][c2];
        qDebug() << "finalizeSwap: pre-swap values:" << QPoint(r1,c1) << preA << QPoint(r2,c2) << preB << "isProp:" << isProp(preA) << isProp(preB);

        // 执行交换
        std::swap(m_board[r1][c1], m_board[r2][c2]);
        qDebug() << "finalizeSwap: post-swap values:" << QPoint(r1,c1) << m_board[r1][c1] << QPoint(r2,c2) << m_board[r2][c2];
        emit boardChanged();
    }

    // 检查是否是道具之间的组合（尤其是超级+超级）
    bool aIsProp = isProp(preA);
    bool bIsProp = isProp(preB);
    if (aIsProp && bIsProp) {
        qDebug() << "finalizeSwap: both sides are props, prefer combo. preA:" << preA << "preB:" << preB;
        // 标记组合处理中，避免递归
        m_comboPending = true;
        // 超级+超级 -> 发射组合效果 105，让 QML 播涟漪动画
        if ((preA == SuperItem) && (preB == SuperItem)) {
            qDebug() << "finalizeSwap: Super+Super combo detected at" << QPoint(r2,c2);
            // 不要直接调用 superItemEffectTriggered，也不要清空格子，交由前端动画后端再触发
            schedulePropEffect(r2, c2, Combo_SuperSuperType, QString(), 0);
            return;
        }
        // 其他组合保留原逻辑（如超级+火箭、炸弹+火箭等）
        // ...existing code for other combos...
    }

    auto matches = findMatches(r1, c1, r2, c2, true);
    if (!matches.isEmpty()) {
        m_comboCnt++;

        // 如果交换产生匹配，同时交换前有道具与普通块交换（例如炸弹与颜色），优先触发道具而非普通三消
        bool preAIsProp = isProp(preA);
        bool preBIsProp = isProp(preB);
        if ((preAIsProp && !preBIsProp) || (preBIsProp && !preAIsProp)) {
            qDebug() << "finalizeSwap: match detected but pre-swap prop vs color present, prefer prop activation. preA:" << preA << "preB:" << preB;
            // 统一用调度 + 置 pending
            m_comboPending = true;
            if (preAIsProp) {
                if (preA == Rocket_UpDown) schedulePropEffect(r2, c2, Rocket_UpDownType, preB, 0);
                else if (preA == Rocket_LeftRight) schedulePropEffect(r2, c2, Rocket_LeftRightType, preB, 0);
                else if (preA == Bomb) schedulePropEffect(r2, c2, BombType, preB, 0);
                else if (preA == SuperItem) { if (!preB.isEmpty()) schedulePropEffect(r2, c2, SuperItemType, preB, 0); }
            } else {
                if (preB == Rocket_UpDown) schedulePropEffect(r1, c1, Rocket_UpDownType, preA, 0);
                else if (preB == Rocket_LeftRight) schedulePropEffect(r1, c1, Rocket_LeftRightType, preA, 0);
                else if (preB == Bomb) schedulePropEffect(r1, c1, BombType, preA, 0);
                else if (preB == SuperItem) { if (!preA.isEmpty()) schedulePropEffect(r1, c1, SuperItemType, preA, 0); }
            }
            emit boardChanged();
            return; // 交由 QML 播放道具动画并在视觉结束后调用 trigger
        }

        QVariantList variantMatches;
        for (const QPoint &pt : matches)
        {
            variantMatches.append(QVariant::fromValue(pt));
        }

        // 发送匹配动画请求
        emit matchAnimationRequested(variantMatches);

        if (m_comboCnt > 1) {
            emit comboChanged(m_comboCnt);
        }
    }
    else
    {
        m_comboCnt = 0;

        if (!isRecursion) {
            // 没有匹配：如果双方都不是道具，则回滚交换；否则按道具/组合处理
            QString a = m_board[r1][c1];
            QString b = m_board[r2][c2];

            bool aNowIsProp = isProp(a);
            bool bNowIsProp = isProp(b);

            if (!aNowIsProp && !bNowIsProp) {
                // 回滚
                std::swap(m_board[r1][c1], m_board[r2][c2]);
                emit rollbackSwap(r1, c1, r2, c2);
                emit boardChanged();
                return;
            }

            // 有道具参与但未形成匹配，检查是否应触发组合或单体道具
            if (aNowIsProp && bNowIsProp) {
                qDebug() << "finalizeSwap: both sides are props but no match, handle combo at" << QPoint(r2,c2) << "a:" << a << "b:" << b;
                m_comboPending = true;
                // 超级相关优先
                if (a == SuperItem && b == SuperItem) {
                    schedulePropEffect(r2, c2, Combo_SuperSuperType, QString(), 0);
                    emit boardChanged(); return;
                }
                if (a == SuperItem || b == SuperItem) {
                    if ((a == SuperItem && b == Bomb) || (b == SuperItem && a == Bomb)) {
                        schedulePropEffect(r2, c2, Combo_SuperBombType, QString(), 0);
                        emit boardChanged(); return;
                    }
                    if ((a == SuperItem && isRocket(b)) || (b == SuperItem && isRocket(a))) {
                        schedulePropEffect(r2, c2, Combo_SuperRocketType, QString(), 0);
                        emit boardChanged(); return;
                    }
                }
                // 火箭+火箭
                if (isRocket(a) && isRocket(b)) {
                    schedulePropEffect(r2, c2, Combo_RocketRocketType, QString(), 0);
                    emit boardChanged(); return;
                }
                // 炸弹+炸弹
                if (isBomb(a) && isBomb(b)) {
                    schedulePropEffect(r2, c2, Combo_BombBombType, QString(), 0);
                    emit boardChanged(); return;
                }
                // 炸弹+火箭
                if ((isBomb(a) && isRocket(b)) || (isRocket(a) && isBomb(b))) {
                    int rocketType = isRocket(a) ? (a==Rocket_UpDown?Rocket_UpDownType:Rocket_LeftRightType) : (b==Rocket_UpDown?Rocket_UpDownType:Rocket_LeftRightType);
                    schedulePropEffect(r2, c2, Combo_BombRocketType, QString::number(rocketType), 0);
                    emit boardChanged(); return;
                }
            } else {
                // 单体道具
                m_comboPending = true;
                if (aNowIsProp && !bNowIsProp) {
                    if (a == Rocket_UpDown) schedulePropEffect(r1, c1, Rocket_UpDownType, b, 0);
                    else if (a == Rocket_LeftRight) schedulePropEffect(r1, c1, Rocket_LeftRightType, b, 0);
                    else if (a == Bomb) schedulePropEffect(r1, c1, BombType, b, 0);
                    else if (a == SuperItem) { if (!b.isEmpty()) schedulePropEffect(r1, c1, SuperItemType, b, 0); }
                } else if (!aNowIsProp && bNowIsProp) {
                    if (b == Rocket_UpDown) schedulePropEffect(r2, c2, Rocket_UpDownType, a, 0);
                    else if (b == Rocket_LeftRight) schedulePropEffect(r2, c2, Rocket_LeftRightType, a, 0);
                    else if (b == Bomb) schedulePropEffect(r2, c2, BombType, a, 0);
                    else if (b == SuperItem) { if (!a.isEmpty()) schedulePropEffect(r2, c2, SuperItemType, a, 0); }
                }
            }
        }

        emit boardChanged();
    }
}

// 在 schedulePropEffect 执行时，清除 m_comboPending 标志以允许后续组合
void GameBoard::schedulePropEffect(int row, int col, int type, const QString &color, int delayMs)
{
    qDebug() << "schedulePropEffect: scheduling" << type << "at" << QPoint(row,col) << "delayMs:" << delayMs << "color:" << color;
    QPoint pt(row, col);
    if (m_pendingActivations.contains(pt)) {
        qDebug() << "schedulePropEffect: already pending at" << pt << ", skip";
        return;
    }
    m_pendingActivations.insert(pt);

    QTimer::singleShot(delayMs, this, [this, row, col, type, color]() {
        qDebug() << "schedulePropEffect: executing (emit propEffect)" << type << "at" << QPoint(row,col) << "color:" << color;
        // 不再在此处修改 m_comboPending，交由各效果触发完成时重置

        // 组合道具
        if (type == Combo_RocketRocketType) { emit propEffect(row, col, Combo_RocketRocketType, QString()); return; }
        if (type == Combo_BombBombType)     { emit propEffect(row, col, Combo_BombBombType,     QString()); return; }
        if (type == Combo_BombRocketType)   { emit propEffect(row, col, Combo_BombRocketType,   color);     return; }
        if (type == Combo_SuperBombType)    { emit propEffect(row, col, Combo_SuperBombType,    QString()); return; }
        if (type == Combo_SuperRocketType)  { emit propEffect(row, col, Combo_SuperRocketType,  QString()); return; }
        if (type == Combo_SuperSuperType)   { emit propEffect(row, col, Combo_SuperSuperType,   QString()); return; }

        // 普通道具
        if (type == Rocket_UpDownType || type == Rocket_LeftRightType) { emit propEffect(row, col, type, QString()); return; }
        if (type == BombType)                                            { emit propEffect(row, col, BombType, QString()); return; }
        if (type == SuperItemType)                                       { emit propEffect(row, col, SuperItemType, color); return; }
    });
}

QString GameBoard::chooseNearbyColor(int row, int col) const
{
    // 优先从上下左右四个格子随机选一个常规颜色（避免越界）
    QVector<QString> candidates;
    const QPoint neigh[4] = { QPoint(-1,0), QPoint(1,0), QPoint(0,-1), QPoint(0,1) };
    for (int i = 0; i < 4; ++i) {
        int r = row + neigh[i].x();
        int c = col + neigh[i].y();
        if (r < 0 || r >= m_rows || c < 0 || c >= m_columns) continue;
        QString v = m_board[r][c];
        if (isColor(v)) candidates.append(v);
    }
    if (!candidates.isEmpty()) {
        int idx = QRandomGenerator::global()->bounded(candidates.size());
        qDebug() << "chooseNearbyColor: picked neighbor color" << candidates[idx] << "around" << QPoint(row,col);
        return candidates[idx];
    }

    // 回退策略：全盘随机选一种颜色（排除道具/空）
    QVector<QString> allColors;
    for (int r = 0; r < m_rows; ++r) {
        for (int c = 0; c < m_columns; ++c) {
            QString v = m_board[r][c];
            if (isColor(v)) allColors.append(v);
        }
    }
    if (!allColors.isEmpty()) {
        int idx = QRandomGenerator::global()->bounded(allColors.size());
        qDebug() << "chooseNearbyColor: fallback pick from board" << allColors[idx];
        return allColors[idx];
    }

    // 最后回退到可用颜色列表中的第一个
    qDebug() << "chooseNearbyColor: no color found nearby or on board, using default";
    return m_availableColors.isEmpty() ? QString() : m_availableColors[0];
}

// 修改 rocketEffectTriggered：在清除行/列时，若遇到其他道具（炸弹/超级/火箭），将调度其激活（延迟 500ms）
void GameBoard::rocketEffectTriggered(int row, int col, int type)
{
    // 重要：动画完成进入逻辑阶段，解除组合挂起状态
    m_comboPending = false;
    // 确保清除延迟激活标记，避免重复调度阻塞
    m_pendingActivations.remove(QPoint(row, col));

    qDebug() << "rocketEffectTriggered: start" << QPoint(row,col) << "type" << type;

    // 先消耗触发该效果的火箭，避免其在后续掉落/匹配流程中再次被激活
    if (row >= 0 && row < m_rows && col >= 0 && col < m_columns) {
        if (!m_board[row][col].isEmpty()) {
            qDebug() << "rocketEffectTriggered: consume activator at" << QPoint(row,col) << "value=" << m_board[row][col];
        }
        m_board[row][col] = "";
        m_pendingActivations.remove(QPoint(row,col));
    }

    QVector<QPoint> clearedNow; // 立即清除的普通颜色格子
    // 对被调度激活的道具位置我们不在此刻清空，交由 schedulePropEffect 在稍后执行
    if(type == Rocket_UpDownType)
    {
        // 清列
        for (int r = 0; r < m_rows; ++r) {
            // 跳过触发来源自身位置
            if (r == row) continue; // FIX: 原为 (r == row && c == col)
            QString v = m_board[r][col];
            if (v.isEmpty()) continue;

            if (isBomb(v)) {
                schedulePropEffect(r, col, BombType, QString(), 500);
            } else if (isRocket(v)) {
                int rocketType = (v == Rocket_UpDown) ? Rocket_UpDownType : Rocket_LeftRightType;
                schedulePropEffect(r, col, rocketType, QString(), 500);
            } else if (v == SuperItem) {
                // 给超级道具选择一个附近颜色作为参数
                QString choose = chooseNearbyColor(r, col);
                schedulePropEffect(r, col, SuperItemType, choose, 500);
            } else {
                // 普通颜色，直接清空
                m_board[r][col] = "";
                clearedNow.append(QPoint(r, col));
            }
        }
    }
    else if(type == Rocket_LeftRightType)
    {
        // 清行
        for (int c = 0; c < m_columns; ++c) {
            // 跳过触发来源自身位置
            if (c == col) continue; // FIX: 原为 (r == row && c == col)
            QString v = m_board[row][c];
            if (v.isEmpty()) continue;

            if (isBomb(v)) {
                schedulePropEffect(row, c, BombType, QString(), 500);
            } else if (isRocket(v)) {
                int rocketType = (v == Rocket_UpDown) ? Rocket_UpDownType : Rocket_LeftRightType;
                schedulePropEffect(row, c, rocketType, QString(), 500);
            } else if (v == SuperItem) {
                QString choose = chooseNearbyColor(row, c);
                schedulePropEffect(row, c, SuperItemType, choose, 500);
            } else {
                m_board[row][c] = "";
                clearedNow.append(QPoint(row, c));
            }
        }
    }

    qDebug() << "rocketEffectTriggered: clearedNow count" << clearedNow.size();
    updateScore(clearedNow.size() * 10);
    emit boardChanged();
    processDrop();
}

// 修改 bombEffectTriggered：在清除圆形区域时，若遇到其他道具调度其激活
void GameBoard::bombEffectTriggered(int row, int col) {
    // 重要：动画完成进入逻辑阶段，解除组合挂起状态
    m_comboPending = false;
    // 确保清除延迟激活标记
    m_pendingActivations.remove(QPoint(row, col));

    int radius = 2;  // 半径为 2 的圆形区域（含中心）

    // 同样先消耗触发炸弹本体，避免重复激活
    if (row >= 0 && row < m_rows && col >= 0 && col < m_columns) {
        if (!m_board[row][col].isEmpty()) {
            qDebug() << "bombEffectTriggered: consume activator at" << QPoint(row,col) << "value=" << m_board[row][col];
        }
        m_board[row][col] = "";
        m_pendingActivations.remove(QPoint(row,col));
    }

    QVector<QPoint> clearedNow;
    QVector<QPoint> expected;

    // 先计算期望清除的格子（基于整数平方距离），用于对比调试
    for (int r = row - radius; r <= row + radius; ++r) {
        for (int c = col - radius; c <= col + radius; ++c) {
            if (r >= 0 && r < m_rows && c >= 0 && c < m_columns) {
                int dr = r - row, dc = c - col;
                if (dr*dr + dc*dc <= radius*radius) expected.append(QPoint(r,c));
            }
        }
    }

    // 清除圆形范围内的方块（使用整数判断）
    for (const QPoint &pt : expected) {
        int r = pt.x();
        int c = pt.y();
        // 跳过触发来源自身位置（该炸弹自身已被清空）
        if (r == row && c == col) continue;

        QString v = m_board[r][c];
        if (v.isEmpty()) continue;

        // 如果是道具，调度其激活（不立即清空）
        if (isBomb(v)) {
            schedulePropEffect(r, c, BombType, QString(), 500);
        } else if (isRocket(v)) {
            int rocketType = (v == Rocket_UpDown) ? Rocket_UpDownType : Rocket_LeftRightType;
            schedulePropEffect(r, c, rocketType, QString(), 500);
        } else if (v == SuperItem) {
            QString choose = chooseNearbyColor(r, c);
            schedulePropEffect(r, c, SuperItemType, choose, 500);
        } else {
            m_board[r][c] = "";
            clearedNow.append(pt);
        }
    }

    // 记录未被清除但应被清除的位置（理论上应该为空）
    QVector<QPoint> missed;
    for (const QPoint &pt : expected) {
        if (!m_board[pt.x()][pt.y()].isEmpty()) {
            missed.append(pt);
        }
    }

    qDebug() << "bombEffectTriggered: bomb at" << QPoint(row,col) << "expected cells:" << expected.size() << "clearedNow:" << clearedNow.size() << "missed:" << missed;
    if (!missed.isEmpty()) {
        qDebug() << "bombEffectTriggered: missed positions (should be empty):" << missed;
    }

    updateScore(clearedNow.size() * 10);

    emit boardChanged();  // 发出更新棋盘信号
    processDrop();
}

// 补充：道具激活后立即查找并处理新產生的三消
void GameBoard::checkAndProcessNewMatches()
{
    qDebug() << "checkAndProcessNewMatches: checking for new matches after prop effects";
    // 这里的查找不需要考虑 r1, c1, r2, c2，因为道具激活后可能在任意位置產生新的三消
    QVector<QPoint> newMatches = findMatches(0, 0, 0, 0, true);
    if (!newMatches.isEmpty()) {
        qDebug() << "checkAndProcessNewMatches: found" << newMatches.size() << "new matched tiles, removing and creating props";
        removeMatchedTiles(newMatches);
        // 在移除匹配后，创建由 findMatches 记录的道具（如果有）
        creatProp();
        emit boardChanged();
        // 处理下落（视觉由 QML 播放，随后 QML 调用 commitDrop）
        processDrop();
    } else {
        qDebug() << "checkAndProcessNewMatches: no new matches";
    }
}

QString GameBoard::getRandomColor() const {
    int idx = QRandomGenerator::global()->bounded(m_availableColors.size());
    return m_availableColors[idx];
}

// 修复const问题
bool GameBoard::isValidSwap(int r1, int c1, int r2, int c2) {
    // 检查边界
    if (r1 < 0 || r1 >= m_rows || c1 < 0 || c1 >= m_columns ||
        r2 < 0 || r2 >= m_rows || c2 < 0 || c2 >= m_columns) {
        qDebug() << "isValidSwap: out of bounds" << QPoint(r1,c1) << QPoint(r2,c2);
        return false;
    }

    // 检查是否相邻
    if (!((qAbs(r1 - r2) == 1 && c1 == c2) || (qAbs(c1 - c2) == 1 && r1 == r2))) {
        qDebug() << "isValidSwap: not adjacent" << QPoint(r1,c1) << QPoint(r2,c2);
        return false;
    }

    // 检查是否可以移动（不能与空格或背景格子交换）
    QString a = m_board[r1][c1];
    QString b = m_board[r2][c2];
    if (!isMovable(a) || !isMovable(b)) {
        qDebug() << "isValidSwap: one side not movable or empty" << QPoint(r1,c1) << a << QPoint(r2,c2) << b;
        return false;
    }

    return true;
}

bool GameBoard::isProp(QString color)
{
    if(color == Rocket_UpDown    ||
       color == Rocket_LeftRight ||
       color == Bomb             ||
       color == SuperItem)
        return true;
    else
        return false;
}

QVector<PropTypedef> GameBoard::findRocketMatches(int r1, int c1, int r2, int c2)
{
    rocketMatches.clear();
    // 横向查找
    for (int r = 0; r < m_rows; ++r) {
        for (int c = 0; c < m_columns - 3; ++c) {
            QString color = m_board[r][c];
            if (!isColor(color)) continue;

            // 查找连续的 4 个相同颜色的方块
            bool isMatch = true;
            for (int i = 1; i < 4; ++i) {
                if (m_board[r][c + i] != color) {
                    isMatch = false;
                    break;
                }
            }

            if (isMatch) {
                if(r == r1 && c == c1)
                {
                    qDebug() << QString("(%1, %2) 创建竖向火箭").arg(r2).arg(c2);
                    rocketMatches.append({Rocket_UpDownType, QPoint(r2, c2)});
                }
                else if(r == r2 && c == c1)
                {
                    qDebug() << QString("(%1, %2) 创建竖向火箭").arg(r1).arg(c1);
                    rocketMatches.append({Rocket_UpDownType, QPoint(r1, c1)});
                }
                else
                {
                    // 自然掉落
                    qDebug() << QString("(%1, %2) 创建竖向火箭").arg(r).arg(c+1);
                    rocketMatches.append({Rocket_UpDownType, QPoint(r, c + 1)});
                }
            }
        }
    }

    // 纵向查找
    for (int c = 0; c < m_columns; ++c) {
        for (int r = 0; r < m_rows - 3; ++r) {
            QString color = m_board[r][c];
            if (!isColor(color)) continue;

            // 查找连续的 4 个相同颜色的方块
            bool isMatch = true;
            for (int i = 1; i < 4; ++i) {
                if (m_board[r + i][c] != color) {
                    isMatch = false;
                    break;
                }
            }

            if (isMatch) {
                if(r == r1 && c == c1 )
                {
                    qDebug() << QString("(%1, %2) 创建横向火箭").arg(r2).arg(c2);
                    rocketMatches.append({Rocket_LeftRightType, QPoint(r2, c2)});
                }
                else if(r == r2 && c == c1)
                {
                    qDebug() << QString("(%1, %2) 创建横向火箭").arg(r1).arg(c1);
                    rocketMatches.append({Rocket_LeftRightType, QPoint(r1, c1)});
                }
                else
                {
                    // 自然掉落
                    qDebug() << QString("(%1, %2) 创建竖向火箭").arg(r+1).arg(c);
                    rocketMatches.append({Rocket_LeftRightType, QPoint(r + 1, c)});
                }
            }
        }
    }

    return rocketMatches;
}

QVector<PropTypedef> GameBoard::findBombMatches()
{
    bombMatches.clear();
    for (int r = 1; r < m_rows - 1; ++r) {
        for (int c = 1; c < m_columns - 1; ++c) {
            QString color = m_board[r][c];
            if (!isColor(color)) continue;

            // Helper function to avoid repeated boundary checks
            auto isInBounds = [this](int r, int c) {
                return r >= 0 && r < m_rows && c >= 0 && c < m_columns;
            };

            // 正向 T 字形 - 向上
            if (isInBounds(r-1, c) && isInBounds(r-2, c) && isInBounds(r, c-1) && isInBounds(r, c+1)) {
                if (m_board[r][c-1] == color && m_board[r][c+1] == color) {
                    if (m_board[r-1][c] == color && m_board[r-2][c] == color) {
                        bombMatches.append({BombType, QPoint(r, c)});  // 中心点
                        qDebug() << QString("(%1, %2) %3 正向 T 字形 - 向上").arg(r).arg(c).arg(color);
                    }
                }
            }

            // 正向 T 字形 - 向下
            if (isInBounds(r+1, c) && isInBounds(r+2, c) && isInBounds(r, c-1) && isInBounds(r, c+1)) {
                if (m_board[r][c-1] == color && m_board[r][c+1] == color) {
                    if (m_board[r+1][c] == color && m_board[r+2][c] == color) {
                        bombMatches.append({BombType, QPoint(r, c)});  // 中心点
                        qDebug() << QString("(%1, %2) %3 正向 T 字形 - 向下").arg(r).arg(c).arg(color);
                    }
                }
            }

            // 倒 T 字形 - 向左
            if (isInBounds(r-1, c) && isInBounds(r+1, c) && isInBounds(r, c-1) && isInBounds(r, c-2)) {
                if (m_board[r-1][c] == color && m_board[r+1][c] == color) {
                    if (m_board[r][c-1] == color && m_board[r][c-2] == color) {
                        bombMatches.append({BombType, QPoint(r, c)});  // 中心点
                        qDebug() << QString("(%1, %2) %3 倒 T 字形 向左").arg(r).arg(c).arg(color);
                    }
                }
            }

            // 倒 T 字形 - 向右
            if (isInBounds(r-1, c) && isInBounds(r+1, c) && isInBounds(r, c+1) && isInBounds(r, c+2)) {
                if (m_board[r-1][c] == color && m_board[r+1][c] == color) {
                    if (m_board[r][c+1] == color && m_board[r][c+2] == color) {
                        bombMatches.append({BombType, QPoint(r, c)});  // 中心点
                        qDebug() << QString("(%1, %2) %3 倒 T 字形 - 向右").arg(r).arg(c).arg(color);
                    }
                }
            }

            // L 字形 - 交点在右上角
            if (isInBounds(r+1, c) && isInBounds(r+2, c) && isInBounds(r, c-1) && isInBounds(r, c-2)) {
                if (m_board[r][c-1] == color && m_board[r][c-2] == color) {
                    if (m_board[r+1][c] == color && m_board[r+2][c] == color) {
                        bombMatches.append({BombType, QPoint(r, c)});  // 中心点
                        qDebug() << QString("(%1, %2) %3 L 字形 - 交点在右上角").arg(r).arg(c).arg(color);
                    }
                }
            }

            // L 字形 - 交点在左上角
            if (isInBounds(r+1, c) && isInBounds(r+2, c) && isInBounds(r, c+1) && isInBounds(r, c+2)) {
                if (m_board[r][c+1] == color && m_board[r][c+2] == color) {
                    if (m_board[r+1][c] == color && m_board[r+2][c] == color) {
                        bombMatches.append({BombType, QPoint(r, c)});  // 中心点
                        qDebug() << QString("(%1, %2) %3 L 字形 - 交点在左上角").arg(r).arg(c).arg(color);
                    }
                }
            }

            // L 字形 - 交点在左下角
            if (isInBounds(r-1, c) && isInBounds(r-2, c) && isInBounds(r, c+1) && isInBounds(r, c+2)) {
                if (m_board[r-1][c] == color && m_board[r-2][c] == color) {
                    if (m_board[r][c+1] == color && m_board[r][c+2] == color) {
                        bombMatches.append({BombType, QPoint(r, c)});  // 中心点
                        qDebug() << QString("(%1, %2) %3 L 字形 - 交点在左下角").arg(r).arg(c).arg(color);
                    }
                }
            }

            // L 字形 - 交点在右下角
            if (isInBounds(r-1, c) && isInBounds(r-2, c) && isInBounds(r, c-1) && isInBounds(r, c-2)) {
                if (m_board[r-1][c] == color && m_board[r-2][c] == color) {
                    if (m_board[r][c-1] == color && m_board[r][c-2] == color) {
                        bombMatches.append({BombType, QPoint(r, c)});  // 中心点
                        qDebug() << QString("(%1, %2) %3 L 字形 - 交点在右下角").arg(r).arg(c).arg(color);
                    }
                }
            }
        }
    }

    return bombMatches;
}


QVector<PropTypedef> GameBoard::findSuperItemMatches() {

    superItemMatches.clear();

    // 横向查找连续 5 个相同颜色的方块
    for (int r = 0; r < m_rows; ++r) {
        for (int c = 0; c < m_columns - 4; ++c) {
            QString color = m_board[r][c];
            if (!isColor(color)) continue;

            bool isMatch = true;
            for (int i = 1; i < 5; ++i) {
                if (m_board[r][c + i] != color) {
                    isMatch = false;
                    break;
                }
            }
            if (isMatch) {
                // 不在检测阶段直接修改棋盘，记录位置交由 creatProp 处理
                superItemMatches.append({SuperItemType, QPoint(r, c + 2)}); // 超级道具放置位置是连续五个方块中的第三个方块
                qDebug() << QString("(%1, %2)超级道具").arg(r).arg(c+2);
            }
        }
    }

    // 纵向查找连续 5 个相同颜色的方块
    for (int c = 0; c < m_columns; ++c) {
        for (int r = 0; r < m_rows - 4; ++r) {
            QString color = m_board[r][c];
            if (!isColor(color)) continue;

            bool isMatch = true;
            for (int i = 1; i < 5; ++i) {
                if (m_board[r + i][c] != color) {
                    isMatch = false;
                    break;
                }
            }
            if (isMatch) {
                superItemMatches.append({SuperItemType, QPoint(r + 2, c)}); // 超级道具放置位置是连续五个方块中的第三个方块
                qDebug() << QString("(%1, %2)超级道具").arg(r+2).arg(c);
            }
        }
    }

    return superItemMatches;
}


QVector<QPoint> GameBoard::findMatches(int r1, int c1, int r2, int c2, bool argflag) {
    QVector<QPoint> matches;
    QSet<QPoint> matchedSet;
    // 横向匹配 - 普通三消
    for (int r = 0; r < m_rows; ++r) {
        for (int c = 0; c < m_columns - 2; ++c) {
            QString color = m_board[r][c];
            if (!isColor(color)) continue;

            int matchLength = 1;
            while (c + matchLength < m_columns && m_board[r][c + matchLength] == color && isColor(m_board[r][c + matchLength])) {
                matchLength++;
            }
            if (matchLength >= 3) {
                // 普通三消
                for (int i = 0; i < matchLength; ++i) {
                    matchedSet.insert(QPoint(r, c + i));
                }
            }
        }
    }

    // 纵向匹配 - 普通三消
    for (int c = 0; c < m_columns; ++c) {
        for (int r = 0; r < m_rows - 2; ++r) {
            QString color = m_board[r][c];
            if (!isColor(color)) continue;

            int matchLength = 1;
            while (r + matchLength < m_rows && m_board[r + matchLength][c] == color && isColor(m_board[r + matchLength][c])) {
                matchLength++;
            }
            if (matchLength >= 3) {
                // 普通三消
                for (int i = 0; i < matchLength; ++i) {
                    matchedSet.insert(QPoint(r + i, c));
                }
            }
        }
    }

    if(argflag == true)
    {
        // 优先查找五个（超级道具），避免五连同时生成其他道具
        findSuperItemMatches();

        // 辅助：检查某点是否已被超级道具占用或属于五连匹配集合
        auto isReservedBySuper = [this, &matchedSet](const QPoint &pt)->bool{
            if (matchedSet.contains(pt)) return true; // 如果该点属于匹配集合（例如五连），则保留给超级道具
            for (const PropTypedef &p : superItemMatches) {
                if (p.point == pt) return true;
            }
            return false;
        };

        // 查找横向竖向火箭匹配，但跳过会与超级道具冲突的位置
        // 修改 findRocketMatches：改为接受预检查 - 但为避免大改，这里调用后清洗 rocketMatches
        findRocketMatches(r1, c1, r2, c2);
        if (!superItemMatches.isEmpty() && !rocketMatches.isEmpty()) {
            QVector<PropTypedef> filtered;
            for (const PropTypedef &p : rocketMatches) {
                if (!isReservedBySuper(p.point)) filtered.append(p);
                else qDebug() << "findMatches: skip rocket at" << p.point << "due to superItem priority";
            }
            rocketMatches = filtered;
        }

        // 查找 T 字形和 L 字形的炸弹匹配，同样避免与超级道具冲突
        findBombMatches();
        if (!superItemMatches.isEmpty() && !bombMatches.isEmpty()) {
            QVector<PropTypedef> filtered2;
            for (const PropTypedef &p : bombMatches) {
                if (!isReservedBySuper(p.point)) filtered2.append(p);
                else qDebug() << "findMatches: skip bomb at" << p.point << "due to superItem priority";
            }
            bombMatches = filtered2;
        }

        // 发射动画信号
        if (!rocketMatches.isEmpty()) {
            emit rocketCreateRequested(rocketMatches); // 生成火箭动画
        }

        if (!bombMatches.isEmpty()) {
            emit bombCreateRequested(bombMatches); // 生成炸弹动画
        }

        if (!superItemMatches.isEmpty()) {
            emit superItemCreateRequested(superItemMatches); // 生成超级道具动画
        }
    }
    // 返回所有匹配的方块
    matches.append(matchedSet.values().toVector());
    return matches;
}



QVector<QVector<QPoint>> GameBoard::calculateDropPaths() const {
    QVector<QVector<QPoint>> dropPaths;

    for (int c = 0; c < m_columns; ++c) {
        int r = m_rows - 1;
        while (r >= 0) {
            // 跳过不可移动(块)位置 或 已被挂起的激活位置（视为阻挡）
            if (!m_board[r][c].isEmpty() && (!isMovable(m_board[r][c]) || m_pendingActivations.contains(QPoint(r, c)))) {
                r--;
                continue;
            }

            int segmentBottom = r;
            int segmentTop = r;
            while (segmentTop >= 0 && (m_board[segmentTop][c].isEmpty() || (isMovable(m_board[segmentTop][c]) && !m_pendingActivations.contains(QPoint(segmentTop, c))))) {
                // 只要是空或者可移动且不是挂起激活点就继续
                segmentTop--;
            }
            segmentTop++;

            int writeRow = segmentBottom;
            for (int rr = segmentBottom; rr >= segmentTop; --rr) {
                // 只有非挂起且可移动的格子会下落
                if (isMovable(m_board[rr][c]) && !m_pendingActivations.contains(QPoint(rr, c))) {
                    if (rr != writeRow) {
                        QVector<QPoint> path;
                        for (int pp = rr; pp <= writeRow; ++pp) {
                            path.append(QPoint(pp, c));
                        }
                        dropPaths.append(path);
                    }
                    writeRow--;
                }
            }

            r = segmentTop - 1;
        }
    }
    return dropPaths;
}

// 把上方的格子往下掉
void GameBoard::applyGravity() {
    qDebug() << "applyGravity: start";
    for (int c = 0; c < m_columns; ++c) {
        int r = m_rows - 1;
        while (r >= 0) {
            // 如果遇到不可移动的背景块或挂起激活点，跳过
            if (!m_board[r][c].isEmpty() && (!isMovable(m_board[r][c]) || m_pendingActivations.contains(QPoint(r, c)))) {
                r--;
                continue;
            }

            int segmentBottom = r;
            int segmentTop = r;
            while (segmentTop >= 0 && (m_board[segmentTop][c].isEmpty() || (isMovable(m_board[segmentTop][c]) && !m_pendingActivations.contains(QPoint(segmentTop, c))))) {
                segmentTop--;
            }
            segmentTop++;

            int writeRow = segmentBottom;
            for (int rr = segmentBottom; rr >= segmentTop; --rr) {
                // 只有非挂起且可移动的格子会下落
                if (isMovable(m_board[rr][c]) && !m_pendingActivations.contains(QPoint(rr, c))) {
                    if (writeRow != rr) {
                        qDebug() << "applyGravity: move(" << rr << "," << c << ") -> (" << writeRow << "," << c << ") value=" << m_board[rr][c];
                        // 如果源格有挂起激活（理论上不应出现，但防御性处理），把挂起位置迁移到目标格
                        QPoint src(rr, c);
                        QPoint dst(writeRow, c);
                        bool srcPending = m_pendingActivations.contains(src);
                        if (srcPending) {
                            m_pendingActivations.remove(src);
                            m_pendingActivations.insert(dst);
                            qDebug() << "applyGravity: moved pending activation" << src << "->" << dst;
                        }

                        m_board[writeRow][c] = m_board[rr][c];
                        m_board[rr][c] = "";
                    } else {
                        // no-op move, stays in place
                    }
                    writeRow--;
                }
            }

            // 将剩余位置清空(这些位置是在区段顶部)，但不要覆盖挂起的激活点
            for (int rr = writeRow; rr >= segmentTop; --rr) {
                if (m_pendingActivations.contains(QPoint(rr, c))) {
                    // 如果挂起位置当前为空且没有实际方块，那么这个挂起可能已被移动或失效，移除以避免长期阻塞
                    if (m_board[rr][c].isEmpty()) {
                        qDebug() << "applyGravity: removing stale pending activation at" << QPoint(rr,c);
                        m_pendingActivations.remove(QPoint(rr, c));
                    }
                    // 保持挂起点为空，等待激活执行（如果仍有效）
                    continue;
                }
                if (!m_board[rr][c].isEmpty()) {
                    qDebug() << "applyGravity: clear cell (" << rr << "," << c << ")";
                }
                m_board[rr][c] = "";
            }

            r = segmentTop - 1;
        }
    }
    qDebug() << "applyGravity: end";
}

void GameBoard::removeMatchedTiles(const QVector<QPoint> &matches) {
    for (const QPoint &pt : matches) {
        m_board[pt.x()][pt.y()] = ""; // 标记为空
        // 如果有挂起的激活点，移除它（该位置已被清除，不应再阻塞填充）
        m_pendingActivations.remove(pt);
    }
    updateScore(matches.size() * 10);
}

void GameBoard::creatProp()
{
    // 创建火箭
    for (const PropTypedef &pt : rocketMatches) {
        m_board[pt.point.x()][pt.point.y()] = (pt.type == 1 ? Rocket_UpDown : Rocket_LeftRight);
        qDebug() << "creatProp: rocket at" << pt.point << "type" << pt.type;
    }

    // 创建炸弹
    for (const PropTypedef &pt : bombMatches) {
        m_board[pt.point.x()][pt.point.y()] = Bomb;
        qDebug() << "creatProp: bomb at" << pt.point;
    }

    // 创建超级道具
    for (const PropTypedef &pt : superItemMatches) {
        m_board[pt.point.x()][pt.point.y()] = SuperItem;
        qDebug() << "creatProp: superItem at" << pt.point;
    }
}

// 新增帮助函数实现
bool GameBoard::isColor(const QString &color) const {
    return m_availableColors.contains(color);
}

bool GameBoard::isMovable(const QString &color) const {
    if (color.isEmpty()) return false;
    if (color == Rocket_UpDown || color == Rocket_LeftRight || color == Bomb || color == SuperItem) return true;
    return m_availableColors.contains(color);
}

void GameBoard::updateScore(int points) {
    m_score += points;
    emit scoreChanged(m_score);
}

Q_INVOKABLE void GameBoard::commitDrop()
{
    qDebug() << "commitDrop: before applyGravity";
    // 打印当前板状态（用于对比）
    for (int r = 0; r < m_rows; ++r) {
        QString rowStr;
        for (int c = 0; c < m_columns; ++c) {
            rowStr += (m_board[r][c].isEmpty() ? "__" : m_board[r][c]) + QString(" ");
        }
        qDebug() << "commitDrop before: row" << r << ":" << rowStr;
    }

    applyGravity();

    qDebug() << "commitDrop: after applyGravity";
    for (int r = 0; r < m_rows; ++r) {
        QString rowStr;
        for (int c = 0; c < m_columns; ++c) {
            rowStr += (m_board[r][c].isEmpty() ? "__" : m_board[r][c]) + QString(" ");
        }
        qDebug() << "commitDrop after gravity: row" << r << ":" << rowStr;
    }

    fillNewTiles();

    qDebug() << "commitDrop: after fillNewTiles";
    for (int r = 0; r < m_rows; ++r) {
        QString rowStr;
        for (int c = 0; c < m_columns; ++c) {
            rowStr += (m_board[r][c].isEmpty() ? "__" : m_board[r][c]) + QString(" ");
        }
        qDebug() << "commitDrop after fill: row" << r << ":" << rowStr;
    }

    emit boardChanged();
    // 检查是否还有三消
    finalizeSwap(0,0,0,0,true);
}

// 新增 helper 实现
bool GameBoard::isRocket(const QString &color) const {
    return color == Rocket_UpDown || color == Rocket_LeftRight;
}

bool GameBoard::isBomb(const QString &color) const {
    return color == Bomb;
}

// 新增：两个火箭组合触发（在 QML 播放合成动画结束后调用）
void GameBoard::rocketRocketTriggered(int row, int col)
{
    // 重要：动画完成进入逻辑阶段，解除组合挂起状态
    m_comboPending = false;
    m_pendingActivations.remove(QPoint(row, col));

    qDebug() << "rocketRocketTriggered: cross clear at" << QPoint(row, col);
    QVector<QPoint> clearedNow;
    // 清除整行并整列，遇到道具则调度激活
    for (int c = 0; c < m_columns; c++) {
        QString v = m_board[row][c];
        if (v.isEmpty()) continue;
        if (isBomb(v)) {
            schedulePropEffect(row, c, BombType, QString(), 500);
        } else if (isRocket(v)) {
            schedulePropEffect(row, c, (v==Rocket_UpDown?Rocket_UpDownType:Rocket_LeftRightType), QString(), 500);
        } else if (v == SuperItem) {
            QString chosen = chooseNearbyColor(row, c);
            schedulePropEffect(row, c, SuperItemType, chosen, 500);
        } else {
            clearedNow.append(QPoint(row, c));
            m_board[row][c] = "";
            m_pendingActivations.remove(QPoint(row, c));
        }
    }
    for (int r = 0; r < m_rows; r++) {
        QString v = m_board[r][col];
        if (v.isEmpty()) continue;
        if (isBomb(v)) {
            schedulePropEffect(r, col, BombType, QString(), 500);
        } else if (isRocket(v)) {
            schedulePropEffect(r, col, (v==Rocket_UpDown?Rocket_UpDownType:Rocket_LeftRightType), QString(), 500);
        } else if (v == SuperItem) {
            QString chosen = chooseNearbyColor(r, col);
            schedulePropEffect(r, col, SuperItemType, chosen, 500);
        } else {
            // 中心可能重复，避免重复计入
            if (r != row) clearedNow.append(QPoint(r, col));
            m_board[r][col] = "";
            m_pendingActivations.remove(QPoint(r, col));
        }
    }
    qDebug() << "rocketRocketTriggered: cleared cells:" << clearedNow.size();
    updateScore(clearedNow.size() * 10);
    emit boardChanged();
    processDrop();
}

// 新增：两个炸弹组合触发（更大半径爆炸）
void GameBoard::bombBombTriggered(int row, int col)
{
    // 重要：动画完成进入逻辑阶段，解除组合挂起状态
    m_comboPending = false;
    m_pendingActivations.remove(QPoint(row, col));

    int radius = 4; // 半径为4
    QVector<QPoint> expected;
    QVector<QPoint> clearedNow;
    for (int r = row - radius; r <= row + radius; r++) {
        for (int c = col - radius; c <= col + radius; c++) {
            if (r >= 0 && r < m_rows && c >= 0 && c < m_columns) {
                int dr = r - row;
                int dc = c - col;
                if (dr*dr + dc*dc <= radius*radius) {
                    expected.append(QPoint(r,c));
                    QString v = m_board[r][c];
                    if (v.isEmpty()) continue;

                    if (isBomb(v)) {
                        schedulePropEffect(r, c, BombType, QString(), 500);
                    } else if (isRocket(v)) {
                        schedulePropEffect(r, c, (v==Rocket_UpDown?Rocket_UpDownType:Rocket_LeftRightType), QString(), 500);
                    } else if (v == SuperItem) {
                        QString chosen = chooseNearbyColor(r, c);
                        schedulePropEffect(r, c, SuperItemType, chosen, 500);
                    } else {
                        clearedNow.append(QPoint(r,c));
                        m_board[r][c] = "";
                        m_pendingActivations.remove(QPoint(r,c));
                    }
                }
            }
        }
    }
    // 计算未被清除但应被清除的位置，用于调试
    QVector<QPoint> missed;
    for (const QPoint &pt : expected) {
        if (!m_board[pt.x()][pt.y()].isEmpty()) {
            missed.append(pt);
        }
    }
    qDebug() << "bombEffectTriggered: big bomb at" << QPoint(row,col) << "expected cells:" << expected.size() << "clearedNow:" << clearedNow.size() << "missed:" << missed;
    if (!missed.isEmpty()) {
        qDebug() << "bombEffectTriggered: missed positions (should be empty):" << missed;
    }

    updateScore(clearedNow.size() * 10);

    emit boardChanged();  // 发出更新棋盘信号
    processDrop();
}

// 新增：炸弹与火箭组合触发
void GameBoard::bombRocketTriggered(int row, int col, int rocketType)
{
    // 重要：动画完成进入逻辑阶段，解除组合挂起状态
    m_comboPending = false;
    m_pendingActivations.remove(QPoint(row, col));

    QVector<QPoint> clearedNow;
    if (rocketType == Rocket_UpDownType) {
        // 清除以 col 为中心的三列
        for (int dc = -1; dc <= 1; ++dc) {
            int cc = col + dc;
            if (cc < 0 || cc >= m_columns) continue;
            for (int r = 0; r < m_rows; ++r) {
                QString v = m_board[r][cc];
                if (v.isEmpty()) continue;

                if (isBomb(v)) {
                    schedulePropEffect(r, cc, BombType, QString(), 500);
                } else if (isRocket(v)) {
                    schedulePropEffect(r, cc, (v==Rocket_UpDown?Rocket_UpDownType:Rocket_LeftRightType), QString(), 500);
                } else if (v == SuperItem) {
                    QString chosen = chooseNearbyColor(r, cc);
                    schedulePropEffect(r, cc, SuperItemType, chosen, 500);
                } else {
                    clearedNow.append(QPoint(r, cc));
                    m_board[r][cc] = "";
                    m_pendingActivations.remove(QPoint(r, cc));
                }
            }
        }
        qDebug() << "bombRocketTriggered: rocket(vertical) + bomb at" << QPoint(row,col) << "cleared cols" << col-1 << col << col+1 << "=> total:" << clearedNow.size();
    } else if (rocketType == Rocket_LeftRightType) {
        // 清除以 row 为中心的三行
        for (int dr = -1; dr <= 1; ++dr) {
            int rr = row + dr;
            if (rr < 0 || rr >= m_rows) continue;
            for (int c = 0; c < m_columns; ++c) {
                QString v = m_board[rr][c];
                if (v.isEmpty()) continue;

                if (isBomb(v)) {
                    schedulePropEffect(rr, c, BombType, QString(), 500);
                } else if (isRocket(v)) {
                    schedulePropEffect(rr, c, (v==Rocket_UpDown?Rocket_UpDownType:Rocket_LeftRightType), QString(), 500);
                } else if (v == SuperItem) {
                    QString chosen = chooseNearbyColor(rr, c);
                    schedulePropEffect(rr, c, SuperItemType, chosen, 500);
                } else {
                    clearedNow.append(QPoint(rr, c));
                    m_board[rr][c] = "";
                    m_pendingActivations.remove(QPoint(rr, c));
                }
            }
        }
        qDebug() << "bombRocketTriggered: rocket(horizontal) + bomb at" << QPoint(row,col) << "cleared rows" << row-1 << row << row+1 << "=> total:" << clearedNow.size();
    } else {
        qDebug() << "bombRocketTriggered: unknown rocketType" << rocketType << "at" << QPoint(row,col);
    }

    updateScore(clearedNow.size() * 10);
    emit boardChanged();
    processDrop();
}

// 新增：超级道具 + 炸弹 组合触发
void GameBoard::superBombTriggered(int row, int col)
{
    // 重要：动画完成进入逻辑阶段，解除组合挂起状态
    m_comboPending = false;
    m_pendingActivations.remove(QPoint(row, col));

    qDebug() << "superBombTriggered: at" << QPoint(row,col);
    // 从超级道具位置的四邻中选择一个颜色
    QString chosen = chooseNearbyColor(row, col);
    if (chosen.isEmpty()) {
        qDebug() << "superBombTriggered: no color chosen, abort";
        return;
    }

    QVector<QPoint> converted;
    // 将场上所有该颜色格子替换为炸弹
    for (int r = 0; r < m_rows; ++r) {
        for (int c = 0; c < m_columns; ++c) {
            if (m_board[r][c] == chosen) {
                m_board[r][c] = Bomb;
                converted.append(QPoint(r,c));
                // 如果之前该位置有挂起激活，移除（此处已变为炸弹并会被 schedule）
                m_pendingActivations.remove(QPoint(r,c));
            }
        }
    }

    // 清除超级道具自身位置（防止残留）
    if (row >= 0 && row < m_rows && col >= 0 && col < m_columns) {
        m_board[row][col] = "";
        m_pendingActivations.remove(QPoint(row,col));
    }

    qDebug() << "superBombTriggered: chosen color" << chosen << "converted to bombs:" << converted.size();
    emit boardChanged();

    // 将所有新炸弹激活（延迟以便前端显示）
    for (const QPoint &pt : converted) {
        schedulePropEffect(pt.x(), pt.y(), BombType, QString(), 500);
    }

    // 计分
    updateScore(converted.size() * 10);
}

// 新增：超级道具 + 火箭 组合触发
void GameBoard::superRocketTriggered(int row, int col)
{
    // 重要：动画完成进入逻辑阶段，解除组合挂起状态
    m_comboPending = false;
    m_pendingActivations.remove(QPoint(row, col));

    qDebug() << "superRocketTriggered: at" << QPoint(row,col);
    QString chosen = chooseNearbyColor(row, col);
    if (chosen.isEmpty()) {
        qDebug() << "superRocketTriggered: no color chosen, abort";
        return;
    }

    QVector<QPoint> converted;
    // 将场上所有该颜色格子替换为随机方向的火箭
    for (int r = 0; r < m_rows; ++r) {
        for (int c = 0; c < m_columns; ++c) {
            if (m_board[r][c] == chosen) {
                // 随机选择竖向或横向火箭
                bool vertical = QRandomGenerator::global()->bounded(2) == 0;
                m_board[r][c] = vertical ? Rocket_UpDown : Rocket_LeftRight;
                converted.append(QPoint(r,c));
                m_pendingActivations.remove(QPoint(r,c));
            }
        }
    }

    // 清除超级道具自身位置
    if (row >= 0 && row < m_rows && col >= 0 && col < m_columns) {
        m_board[row][col] = "";
        m_pendingActivations.remove(QPoint(row,col));
    }

    qDebug() << "superRocketTriggered: chosen color" << chosen << "converted to rockets:" << converted.size();
    emit boardChanged();

    // 激活所有新火箭（延迟以便前端显示）
    for (const QPoint &pt : converted) {
        QString v = m_board[pt.x()][pt.y()];
        int type = (v == Rocket_UpDown) ? Rocket_UpDownType : Rocket_LeftRightType;
        schedulePropEffect(pt.x(), pt.y(), type, QString(), 500);
    }

    updateScore(converted.size() * 10);
}

Q_INVOKABLE void GameBoard::processDrop()
{
    qDebug() << "processDrop: calculating drop paths";
    QVector<QVector<QPoint>> paths = calculateDropPaths();

    QVariantList variantPaths;
    for (const QVector<QPoint> &path : paths) {
        QVariantList singlePath;
        for (const QPoint &pt : path) {
            singlePath.append(QVariant::fromValue(pt));
        }
        variantPaths.append(singlePath);
    }

    if (!variantPaths.isEmpty()) {
        qDebug() << "processDrop: emitting dropAnimationRequested with paths count" << variantPaths.size();
        emit dropAnimationRequested(variantPaths);
    } else {
        qDebug() << "processDrop: no drops, checking if fill needed or matches";
        // 如果没有下落路径，先检查是否有空格需要补充（例如被道具清空但没有上方方块落下）
        bool hasEmpty = false;
        for (int r = 0; r < m_rows && !hasEmpty; ++r) {
            for (int c = 0; c < m_columns; ++c) {
                if (m_board[r][c].isEmpty()) { hasEmpty = true; break; }
            }
        }

        if (hasEmpty) {
            qDebug() << "processDrop: found empty cells, calling commitDrop to apply gravity and fill";
            // 直接调用 commitDrop 执行下落与补位（QML 无下落动画时后端需立即完成数据变更）
            commitDrop();
        } else {
            qDebug() << "processDrop: no drops and no empty cells, checking matches";
            // 如果没有下落路径且没有空格，则检查是否存在匹配需要处理
            finalizeSwap(0, 0, 0, 0, true);
        }
    }
}

Q_INVOKABLE void GameBoard::processMatches()
{
    qDebug() << "processMatches: scanning for matches";
    QVector<QPoint> matches = findMatches(0, 0, 0, 0, true);
    if (!matches.isEmpty()) {
        qDebug() << "processMatches: found" << matches.size() << "matched tiles, removing and creating props";
        removeMatchedTiles(matches);
        // 在移除匹配后，创建由 findMatches 记录的道具（如果有）
        creatProp();
        emit boardChanged();
        // 处理下落（视觉由 QML 播放，随后 QML 调用 commitDrop）
        processDrop();
    } else {
        qDebug() << "processMatches: no matches";
    }
}

// 新增：实现超级道具的普通效果（清除全盘指定颜色）
void GameBoard::superItemEffectTriggered(int row, int col, QString inputColor)
{
    // 重要：动画完成进入逻辑阶段，解除组合挂起状态
    m_comboPending = false;
    // 清除延迟激活标记
    m_pendingActivations.remove(QPoint(row, col));

    qDebug() << "superItemEffectTriggered: at" << QPoint(row, col) << "color:" << inputColor;

    // 消耗激活器自身
    if (row >= 0 && row < m_rows && col >= 0 && col < m_columns) {
        m_pendingActivations.remove(QPoint(row, col));
        if (!m_board[row][col].isEmpty()) {
            qDebug() << "superItemEffectTriggered: consume activator at" << QPoint(row, col) << "value=" << m_board[row][col];
        }
        m_board[row][col] = "";
    }

    // 选择要清除的颜色
    QString chosen = inputColor;
    if (chosen.isEmpty()) {
        chosen = chooseNearbyColor(row, col);
    }
    if (chosen.isEmpty()) {
        qDebug() << "superItemEffectTriggered: no color chosen, abort";
        return;
    }

    int cleared = 0;
    // 清除全盘该颜色的普通方块（保留道具）
    for (int r = 0; r < m_rows; ++r) {
        for (int c = 0; c < m_columns; ++c) {
            QString v = m_board[r][c];
            if (v == chosen && isColor(v)) {
                m_board[r][c] = "";
                m_pendingActivations.remove(QPoint(r, c));
                cleared++;
            }
        }
    }

    qDebug() << "superItemEffectTriggered: cleared color" << chosen << "count:" << cleared;
    updateScore(cleared * 10);
    emit boardChanged();
    processDrop();
}

// 新增：开始游戏（初始化棋盘并重置分数/连击）
void GameBoard::startGame()
{
    initializeBoard();
}

// 新增：重置游戏（清空分数并重新生成棋盘）
void GameBoard::resetGame()
{
    qDebug() << "resetGame";
    m_score = 0;
    m_comboCnt = 0;
    emit scoreChanged(m_score);
    initializeBoard();
}

// 新增：打乱棋盘（保持道具与不可移动块不变，仅打乱普通颜色；并确保无初始三消）
void GameBoard::shuffleBoard()
{
    qDebug() << "shuffleBoard";
    // 收集所有可移动的普通颜色位置
    QVector<QPoint> colorCells;
    QVector<QString> colors;
    for (int r = 0; r < m_rows; ++r) {

        for (int c = 0; c < m_columns; ++c) {
            QString v = m_board[r][c];
            if (isColor(v)) {
                colorCells.append(QPoint(r, c));
                colors.append(v);
            }
        }
    }
    if (colors.isEmpty()) {
        emit boardChanged();
        return;
    }

    // 随机打乱颜色列表
    for (int i = colors.size() - 1; i > 0; --i) {
        int j = QRandomGenerator::global()->bounded(i + 1);
        std::swap(colors[i], colors[j]);
    }

    // 赋回到棋盘
    for (int i = 0; i < colorCells.size(); ++i) {
        const QPoint &pt = colorCells[i];
        m_board[pt.x()][pt.y()] = colors[i];
    }

    // 如仍有匹配则重新生成直到无匹配（防御性，避免初始消除）
    int guard = 0;
    while (!findMatches(0, 0, 0, 0, false).isEmpty() && guard < 50) {
        // 简单重新随机填充普通颜色
        for (const QPoint &pt : colorCells) {
            m_board[pt.x()][pt.y()] = getRandomColor();
        }
        guard++;
    }

    emit boardChanged();
}

// 新增：超级道具合成触发（占位符，实际逻辑已在其他地方实现）
Q_INVOKABLE void GameBoard::superSuperTriggered(int row, int col)
{
    // 重要：动画完成进入逻辑阶段，解除组合挂起状态
    m_comboPending = false;
    // 保障存在于链接单元的最小实现，实际逻辑已在另一处完善
    qDebug() << "superSuperTriggered (stub): at" << QPoint(row, col);
    QVector<QPoint> clearedNow;
    for (int r = 0; r < m_rows; ++r) {
        for (int c = 0; c < m_columns; ++c) {
            if (!m_board[r][c].isEmpty()) {
                m_board[r][c].clear();
                clearedNow.append(QPoint(r,c));
            }
        }
    }
    updateScore(clearedNow.size() * 10);
    emit boardChanged();
    processDrop();
}

