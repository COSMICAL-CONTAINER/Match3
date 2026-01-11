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
    m_step = m_init_step;
    m_availableColors = {"red", "green", "blue", "yellow", "purple", "brown"};
    initializeBoard();
}

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

// qml交换动画完成之后调用此函数
Q_INVOKABLE void GameBoard::finalizeSwap(int r1, int c1, int r2, int c2, bool isRecursion){
    qDebug() << "finalizeSwap called:" << QPoint(r1,c1) << "->" << QPoint(r2,c2) << "isRecursion:" << isRecursion;

    // 若标记挂起但队列已空，自动解除，避免后续阻塞
    if (m_comboPending && m_pendingActivations.isEmpty()) {
        qDebug() << "finalizeSwap: comboPending true but no pendingActivations, auto-reset";
        m_comboPending = false;
    }
    if (m_comboPending) {
        qDebug() << "finalizeSwap: combo pending, skipping to avoid re-entry";
        return;
    }

    bool stepDeducted = false;
    QString preA = ""; QString preB = "";
    if (!isRecursion) {
        preA = m_board[r1][c1];
        preB = m_board[r2][c2];
        qDebug() << "finalizeSwap: pre-swap values:" << QPoint(r1,c1) << preA << QPoint(r2,c2) << preB << "isProp:" << isProp(preA) << isProp(preB);
        std::swap(m_board[r1][c1], m_board[r2][c2]);
        qDebug() << "finalizeSwap: post-swap values:" << QPoint(r1,c1) << m_board[r1][c1] << QPoint(r2,c2) << m_board[r2][c2];
        emit boardChanged();
    }

    // 1) 首先判定：双道具组合（只触发组合效果，绝不触发单体）
    const bool aIsProp = isProp(preA);
    const bool bIsProp = isProp(preB);
    if (aIsProp && bIsProp) {
        qDebug() << "finalizeSwap: both sides are props, trigger combo only. preA:" << preA << "preB:" << preB;
        m_comboPending = true;
        if (!isRecursion && !stepDeducted) { updateStep(-1); stepDeducted = true; }
        // 标记本次组合的两个参与点，后续组合效果与调度层都会跳过它们
        markComboParticipants(r1, c1, r2, c2);

        // 超级+超级
        if (preA == SuperItem && preB == SuperItem) {
            schedulePropEffect(r2, c2, Combo_SuperSuperType, QString(), 0);
            return;
        }
        // 超级+炸弹
        if ((preA == SuperItem && preB == Bomb) || (preB == SuperItem && preA == Bomb)) {
            schedulePropEffect(r2, c2, Combo_SuperBombType, QString(), 0);
            return;
        }
        // 超级+火箭
        if ((preA == SuperItem && isRocket(preB)) || (preB == SuperItem && isRocket(preA))) {
            schedulePropEffect(r2, c2, Combo_SuperRocketType, QString(), 0);
            return;
        }
        // 火箭+火箭
        if (isRocket(preA) && isRocket(preB)) {
            schedulePropEffect(r2, c2, Combo_RocketRocketType, QString(), 0);
            return;
        }
        // 炸弹+炸弹
        if (isBomb(preA) && isBomb(preB)) {
            schedulePropEffect(r2, c2, Combo_BombBombType, QString(), 0);
            return;
        }
        // 炸弹+火箭（记录火箭类型）
        if ((isBomb(preA) && isRocket(preB)) || (isRocket(preA) && isBomb(preB))) {
            int rocketType = isRocket(preA) ? (preA==Rocket_UpDown?Rocket_UpDownType:Rocket_LeftRightType)
                                            : (preB==Rocket_UpDown?Rocket_UpDownType:Rocket_LeftRightType);
            schedulePropEffect(r2, c2, Combo_BombRocketType, QString::number(rocketType), 0);
            return;
        }
        // 未覆盖的道具组合，直接返回避免单体触发
        return;
    }

    // 2) 其次判定：道具 + 普通颜色（只触发单体对应效果）
    if ((aIsProp && !bIsProp) || (!aIsProp && bIsProp)) {
        qDebug() << "finalizeSwap: prop + color, trigger single effect. preA:" << preA << "preB:" << preB;
        m_comboPending = true;
        if (!isRecursion && !stepDeducted) { updateStep(-1); stepDeducted = true; }
        if (aIsProp) {
            if (preA == Rocket_UpDown)      schedulePropEffect(r2, c2, Rocket_UpDownType, preB, 0);
            else if (preA == Rocket_LeftRight) schedulePropEffect(r2, c2, Rocket_LeftRightType, preB, 0);
            else if (preA == Bomb)          schedulePropEffect(r2, c2, BombType, preB, 0);
            else if (preA == SuperItem && !preB.isEmpty()) schedulePropEffect(r2, c2, SuperItemType, preB, 0);
        } else {
            if (preB == Rocket_UpDown)      schedulePropEffect(r1, c1, Rocket_UpDownType, preA, 0);
            else if (preB == Rocket_LeftRight) schedulePropEffect(r1, c1, Rocket_LeftRightType, preA, 0);
            else if (preB == Bomb)          schedulePropEffect(r1, c1, BombType, preA, 0);
            else if (preB == SuperItem && !preA.isEmpty()) schedulePropEffect(r1, c1, SuperItemType, preA, 0);
        }
        emit boardChanged();
        return;
    }

    // 3) 再判定：普通匹配（三消/四消/五消），有效行动扣步
    auto matches = findMatches(r1, c1, r2, c2, true);
    if (!matches.isEmpty()) {
        m_comboCnt++;
        if (!isRecursion && !stepDeducted) { updateStep(-1); stepDeducted = true; }
        QVariantList variantMatches;
        for (const QPoint &pt : matches) { variantMatches.append(QVariant::fromValue(pt)); }
        emit matchAnimationRequested(variantMatches);
        if (m_comboCnt > 1) emit comboChanged(m_comboCnt);
        return;
    }

    // 4) 无效交换：回滚且不扣步
    m_comboCnt = 0;
    if (!isRecursion) {
        std::swap(m_board[r1][c1], m_board[r2][c2]);
        emit rollbackSwap(r1, c1, r2, c2);
        emit boardChanged();
    }
}

// 在 schedulePropEffect 执行时，清除 m_comboPending 标志以允许后续组合
void GameBoard::schedulePropEffect(int row, int col, int type, const QString &color, int delayMs)
{
    qDebug() << "schedulePropEffect: scheduling" << type << "at" << QPoint(row,col) << "delayMs:" << delayMs << "color:" << color;
    QPoint pt(row, col);

    // 若该点是组合参与者且 type 属于单体（1~4），则跳过，防止组合完成后再次单独激活
    if (m_comboParticipants.contains(pt) && type >= 1 && type <= 4) {
        qDebug() << "schedulePropEffect: skip single effect at combo participant" << pt << "type:" << type;
        return;
    }

    if (m_pendingActivations.contains(pt)) {
        qDebug() << "schedulePropEffect: already pending at" << pt << ", skip";
        return;
    }
    m_pendingActivations.insert(pt);

    QTimer::singleShot(delayMs, this, [this, row, col, type, color]() {
        // 执行前先移除该点的 pending，防止残留导致后续阻塞
        m_pendingActivations.remove(QPoint(row, col));
        qDebug() << "schedulePropEffect: executing (emit propEffect)" << type << "at" << QPoint(row,col) << "color:" << color;
        emit propEffect(row, col, type, color);
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
    m_comboParticipants.remove(QPoint(row, col));

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

            // 若该位置处于待激活集合中，则跳过，避免组合后再次单体触发
            if (m_pendingActivations.contains(QPoint(r, col))) continue;

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

            if (m_pendingActivations.contains(QPoint(row, c))) continue;

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
    // 单体火箭激活次数
    addStatAt(6);
    emit boardChanged();
    processDrop();
}

// 修改 bombEffectTriggered：在清除圆形区域时，若遇到其他道具调度其激活
void GameBoard::bombEffectTriggered(int row, int col) {
    // 重要：动画完成进入逻辑阶段，解除组合挂起状态
    m_comboPending = false;
    m_pendingActivations.remove(QPoint(row, col));
    m_comboParticipants.remove(QPoint(row, col));

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
    // 单体炸弹激活次数
    addStatAt(7);
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

// 判断是否是道具
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
                // qDebug() << QString("(%1, %2) 创建竖向火箭").arg(r+c2-c).arg(c);
                // rocketMatches.append({Rocket_UpDownType, QPoint(r+c2-c, c)});
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

        // 计算：所有被“五连”覆盖到的格子集合（用于彻底屏蔽同一条五连中产生的火箭）
        QSet<QPoint> superCoveredCells;
        for (const PropTypedef &sp : superItemMatches) {
            int cr = sp.point.x();
            int cc = sp.point.y();
            if (cr < 0 || cr >= m_rows || cc < 0 || cc >= m_columns) continue;
            QString color = m_board[cr][cc];
            if (!isColor(color)) continue;
            // 横向扩展
            int left = cc, right = cc;
            while (left-1 >= 0 && m_board[cr][left-1] == color) left--;
            while (right+1 < m_columns && m_board[cr][right+1] == color) right++;
            if (right - left + 1 >= 5) {
                int start = cc - 2; if (start < left) start = left; if (start > right - 4) start = right - 4;
                for (int c = start; c < start + 5; ++c) superCoveredCells.insert(QPoint(cr, c));
            }
            // 纵向扩展
            int top = cr, bottom = cr;
            while (top-1 >= 0 && m_board[top-1][cc] == color) top--;
            while (bottom+1 < m_rows && m_board[bottom+1][cc] == color) bottom++;
            if (bottom - top + 1 >= 5) {
                int start = cr - 2; if (start < top) start = top; if (start > bottom - 4) start = bottom - 4;
                for (int r = start; r < start + 5; ++r) superCoveredCells.insert(QPoint(r, cc));
            }
        }

        // 辅助：检查某点是否已被超级道具占用（仅判断超级道具位置，避免误排除普通三/四消可生成道具的位置）
        auto isReservedBySuper = [this](const QPoint &pt)->bool{
            for (const PropTypedef &p : superItemMatches) {
                if (p.point == pt) return true;
            }
            return false;
        };

        // 查找横向竖向火箭匹配
        findRocketMatches(r1, c1, r2, c2);
        if (!rocketMatches.isEmpty()) {
            QVector<PropTypedef> filtered;
            for (const PropTypedef &p : rocketMatches) {
                // 若火箭位置落在五连覆盖的任一格子上，则丢弃，避免与超级道具并存
                if (superCoveredCells.contains(p.point)) {
                    qDebug() << "findMatches: skip rocket at (covered by 5-in-a-row)" << p.point;
                    continue;
                }
                // 同时避免与超级道具中心位置冲突
                if (isReservedBySuper(p.point)) {
                    qDebug() << "findMatches: skip rocket at" << p.point << "due to superItem priority";
                    continue;
                }
                filtered.append(p);
            }
            rocketMatches = filtered;
        }

        // 查找 T/L 炸弹匹配，同样避免与超级道具冲突
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
            emit rocketCreateRequested(rocketMatches);
        }
        if (!bombMatches.isEmpty()) {
            emit bombCreateRequested(bombMatches);
        }
        if (!superItemMatches.isEmpty()) {
            emit superItemCreateRequested(superItemMatches);
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
        QString v = m_board[pt.x()][pt.y()];
        // 根据颜色递增统计（0-5）
        if (v == "red") addStatAt(0);
        else if (v == "green") addStatAt(1);
        else if (v == "blue") addStatAt(2);
        else if (v == "yellow") addStatAt(3);
        else if (v == "purple") addStatAt(4);
        else if (v == "brown") addStatAt(5);
        m_board[pt.x()][pt.y()] = "";
        m_pendingActivations.remove(pt);
    }
    updateScore(matches.size() * 10);
}

// 创建道具
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

void GameBoard::updateStep(int step){
    static bool isEnd = false;
    if(step == -1){
        if(m_step == 1){
            m_step -= 1;
            isEnd = true;
        } else {
            m_step -= 1;
        }

    } else {
        m_step = step;
    }
    emit stepChanged(m_step);
    if(isEnd){
        isEnd = false;
        emit gameOver();
    }
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
    // 组合开始时立即解除挂起并清空所有 pending，避免后续 finalizeSwap 阻塞
    m_comboPending = false;
    m_pendingActivations.clear();

    // 先消耗两个参与点
    for (const QPoint &p : m_comboParticipants) {
        if (!m_board[p.x()][p.y()].isEmpty()) {
            qDebug() << "rocketRocketTriggered: consume combo participant at" << p << "value=" << m_board[p.x()][p.y()];
        }
        m_board[p.x()][p.y()] = "";
        m_pendingActivations.remove(p);
    }

    QVector<QPoint> clearedNow;
    // 在 row, col 交叉清除
    for (int c = 0; c < m_columns; c++) {
        QPoint pt(row, c);
        if (pt == QPoint(row, col)) continue; // 跳过参与点
        QString v = m_board[row][c];
        if (v.isEmpty()) continue;
        if (m_comboParticipants.contains(pt)) continue;
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
        QPoint pt(r, col);
        if (pt == QPoint(row, col)) continue; // 跳过参与点
        QString v = m_board[r][col];
        if (v.isEmpty()) continue;
        if (m_comboParticipants.contains(pt)) continue;
        if (isBomb(v)) {
            schedulePropEffect(r, col, BombType, QString(), 500);
        } else if (isRocket(v)) {
            schedulePropEffect(r, col, (v==Rocket_UpDown?Rocket_UpDownType:Rocket_LeftRightType), QString(), 500);
        } else if (v == SuperItem) {
            QString chosen = chooseNearbyColor(r, col);
            schedulePropEffect(r, col, SuperItemType, chosen, 500);
        } else {
            if (r != row) clearedNow.append(QPoint(r, col));
            m_board[r][col] = "";
            m_pendingActivations.remove(QPoint(r, col));
        }
    }
    qDebug() << "rocketRocketTriggered: cleared cells:" << clearedNow.size();
    updateScore(clearedNow.size() * 10);
    // 组合激活统计
    addStatAt(9);
    emit boardChanged();
    processDrop();
    clearComboParticipants();
}

// 新增：两个炸弹组合触发（更大半径爆炸）
void GameBoard::bombBombTriggered(int row, int col)
{
    m_comboPending = false;
    m_pendingActivations.clear();

    // 先消耗两个参与点
    for (const QPoint &p : m_comboParticipants) {
        if (!m_board[p.x()][p.y()].isEmpty()) {
            qDebug() << "bombBombTriggered: consume combo participant at" << p << "value=" << m_board[p.x()][p.y()];
        }
        m_board[p.x()][p.y()] = "";
        m_pendingActivations.remove(p);
    }

    int radius = 4;
    QVector<QPoint> expected;
    QVector<QPoint> clearedNow;

    // 同样先消耗触发炸弹本体，避免重复激活
    if (row >= 0 && row < m_rows && col >= 0 && col < m_columns) {
        if (!m_board[row][col].isEmpty()) {
            qDebug() << "bombBombTriggered: consume activator at" << QPoint(row,col) << "value=" << m_board[row][col];
        }
        m_board[row][col] = "";
        m_pendingActivations.remove(QPoint(row,col));
    }

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

    qDebug() << "bombBombTriggered: bomb at" << QPoint(row,col) << "expected cells:" << expected.size() << "clearedNow:" << clearedNow.size() << "missed:" << missed;
    if (!missed.isEmpty()) {
        qDebug() << "bombBombTriggered: missed positions (should be empty):" << missed;
    }

    updateScore(clearedNow.size() * 10);
    // 组合激活统计
    addStatAt(10);
    emit boardChanged();
    processDrop();
    clearComboParticipants();
}

// 新增：炸弹与火箭组合触发
void GameBoard::bombRocketTriggered(int row, int col, int rocketType)
{
    m_comboPending = false;
    m_pendingActivations.clear();

    // 先消耗两个参与点
    for (const QPoint &p : m_comboParticipants) {
        if (!m_board[p.x()][p.y()].isEmpty()) {
            qDebug() << "bombRocketTriggered: consume combo participant at" << p << "value=" << m_board[p.x()][p.y()];
        }
        m_board[p.x()][p.y()] = "";
        m_pendingActivations.remove(p);
    }

    QVector<QPoint> clearedNow;
    if (rocketType == Rocket_UpDownType) {
        for (int dc = -1; dc <= 1; ++dc) {
            int cc = col + dc;
            if (cc < 0 || cc >= m_columns) continue;
            for (int r = 0; r < m_rows; ++r) {
                QPoint pt(r, cc);
                if (pt == QPoint(row, col)) continue; // 跳过参与点
                QString v = m_board[r][cc];
                if (v.isEmpty()) continue;
                if (m_comboParticipants.contains(pt)) continue; // 跳过最近组合参与集合

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
        for (int dr = -1; dr <= 1; ++dr) {
            int rr = row + dr;
            if (rr < 0 || rr >= m_rows) continue;
            for (int c = 0; c < m_columns; ++c) {
                QPoint pt(rr, c);
                if (pt == QPoint(row, col)) continue; // 跳过参与点
                QString v = m_board[rr][c];
                if (v.isEmpty()) continue;
                if (m_comboParticipants.contains(pt)) continue; // 跳过最近组合参与集合

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
    }

    updateScore(clearedNow.size() * 10);
    // 组合激活统计
    addStatAt(11);
    emit boardChanged();
    processDrop();
    clearComboParticipants();
}

// 新增：超级道具 + 火箭 组合触发
void GameBoard::superRocketTriggered(int row, int col)
{
    // 重要：动画完成进入逻辑阶段，解除组合挂起状态
    m_comboPending = false;
    m_pendingActivations.remove(QPoint(row, col));
    clearComboParticipants();

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

    // 次数统计
    addStatAt(12);
    updateScore(converted.size() * 10);
}


// 新增：超级道具 + 炸弹 组合触发
void GameBoard::superBombTriggered(int row, int col)
{
    // 重要：动画完成进入逻辑阶段，解除组合挂起状态
    m_comboPending = false;
    m_pendingActivations.remove(QPoint(row, col));
    clearComboParticipants();

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

    // 次数统计
    addStatAt(13);
    // 计分
    updateScore(converted.size() * 10);
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

    // 次数统计
    addStatAt(14);
    updateScore(clearedNow.size() * 10);
    emit boardChanged();
    processDrop();
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
    // 单体超级激活次数
    addStatAt(8);
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
    m_step = m_init_step;
    m_score = 0;
    m_comboCnt = 0;
    for(int i = 0; i < 12; ++i)
    {
        m_stats[i] = 0;
    }
    emit scoreChanged(m_score);
    emit stepChanged(m_step);
    emit statsChanged(stats());
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


