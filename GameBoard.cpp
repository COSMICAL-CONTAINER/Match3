#include "GameBoard.h"
#include <QRandomGenerator>
#include <QDebug>
#include <QVariant>
#include <QList>
#include <QVector>
#include <QPoint>
#include <QTimer>

GameBoard::GameBoard(QObject *parent, int rows, int columns)
    : QObject(parent), m_comboCnt(0), m_rows(rows), m_columns(columns), m_score(0)
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

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            if (m_board[r][c] == "") {
                m_board[r][c] = SuperItem;
                // m_board[r][c] = "";
                m_board[r][c] = getRandomColor(); // 5 种颜色
            }
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
        emit invalidSwap(r1, c1, r2, c2);   // 只告诉 QML 播动画
        return;
    }

    emit swapAnimationRequested(r1, c1, r2, c2);        // 播放请求交换动画
}


Q_INVOKABLE void GameBoard::finalizeSwap(int r1, int c1, int r2, int c2, bool isRecursion)
{
    if (!isRecursion)
    {
        std::swap(m_board[r1][c1], m_board[r2][c2]);
        emit boardChanged();
    }

    auto matches = findMatches(r1, c1, r2, c2, true);
    if (!matches.isEmpty())
    {
        m_comboCnt++;
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

        if( m_board[r1][c1] == Rocket_UpDown    ||
            m_board[r1][c1] == Rocket_LeftRight ||
            m_board[r1][c1] == Bomb             ||
            m_board[r1][c1] == SuperItem        ||
            m_board[r2][c2] == Rocket_UpDown    ||
            m_board[r2][c2] == Rocket_LeftRight ||
            m_board[r2][c2] == Bomb             ||
            m_board[r2][c2] == SuperItem)
        {
            // 判断是不是交换的道具(注意是交换完之后的地方激活)
            if (m_board[r1][c1] == Rocket_UpDown) {
                emit propEffect(r1, c1, Rocket_UpDownType, m_board[r2][c2]);     // 发射竖直火箭
            }
            if (m_board[r1][c1] == Rocket_LeftRight) {
                emit propEffect(r1, c1, Rocket_LeftRightType, m_board[r2][c2]);  // 发射横向火箭
            }
            if (m_board[r1][c1] == Bomb) {
                emit propEffect(r1, c1, BombType, m_board[r2][c2]);              // 引发炸弹
            }
            if (m_board[r1][c1] == SuperItem) {
                if(m_board[r2][c2] != "")
                    emit propEffect(r1, c1, SuperItemType, m_board[r2][c2]);         // 触发超级道具
                else
                {
                    // 没有匹配 → 回滚 → 结束
                    std::swap(m_board[r1][c1], m_board[r2][c2]);
                    emit rollbackSwap(r1, c1, r2, c2);
                }
            }
            if (m_board[r2][c2] == Rocket_UpDown) {
                emit propEffect(r2, c2, Rocket_UpDownType, m_board[r1][c1]);     // 发射竖直火箭
            }
            if (m_board[r2][c2] == Rocket_LeftRight) {
                emit propEffect(r2, c2, Rocket_LeftRightType, m_board[r1][c1]);  // 发射横向火箭
            }
            if (m_board[r2][c2] == Bomb) {
                emit propEffect(r2, c2, BombType, m_board[r1][c1]);              // 引发炸弹
            }
            if (m_board[r2][c2] == SuperItem) {
                if(m_board[r1][c1] != "")
                    emit propEffect(r2, c2, SuperItemType, m_board[r1][c1]);         // 触发超级道具
                else
                {
                    // 没有匹配 → 回滚 → 结束
                    std::swap(m_board[r1][c1], m_board[r2][c2]);
                    emit rollbackSwap(r1, c1, r2, c2);
                }
            }
        }
        else if(!isRecursion)
        {
            // 没有匹配 → 回滚 → 结束
            std::swap(m_board[r1][c1], m_board[r2][c2]);
            emit rollbackSwap(r1, c1, r2, c2);
        }

        emit boardChanged();
    }
}
#include <QThread>
void GameBoard::processDrop()
{
    // 掉落
    applyGravity();
    // 通知 QML 刷新
    emit boardChanged();

    // 计算掉落路径，发给动画
    auto drops = calculateDropPaths();
    QVariantList variantDrops;
    for (const auto &path : drops) {
        QVariantList pathList;
        for (const QPoint &pt : path)
            pathList.append(QVariant::fromValue(pt));
        variantDrops.append(QVariant::fromValue(pathList));
    }
    emit dropAnimationRequested(variantDrops);

    QEventLoop loop; // 创建一个新的事件循环
    QTimer::singleShot(100, &loop, SLOT(quit())); // 创建一个单次定时器，其槽函数为事件循环的退出函数
    loop.exec(); // 开始执行事件循环。程序会在此处暂停，直到定时器到期，从而退出本循环。

    // 填充新方块
    fillNewTiles();

    // 通知 QML 刷新
    emit boardChanged();
}

// 动画完成后调用
Q_INVOKABLE void GameBoard::processMatches() {
    auto matches = findMatches(0, 0, 0, 0, false);
    if (matches.isEmpty())
    {
        m_comboCnt = 0;
        return ;
    }

    // 真正移除
    removeMatchedTiles(matches);

    // 创建道具
    creatProp();

    emit boardChanged();

    // 执行掉落
    processDrop();

    // 查看掉落完毕后是否还有别的需要处理
    finalizeSwap(0,0,0,0,true);
}

Q_INVOKABLE void GameBoard::rocketEffectTriggered(int row, int col, int type)
{
    if(type == 1)
    {
        clearColumn(col);
    }
    else if(type == 2)
    {
        clearRow(row);
    }
    emit boardChanged();
    finalizeSwap(0,0,0,0,true);
}

Q_INVOKABLE void GameBoard::bombEffectTriggered(int row, int col) {
    int radius = 2;  // 半径为 3 的圆形区域

    // 清除圆形范围内的方块
    for (int r = row - radius; r <= row + radius; ++r) {
        for (int c = col - radius; c <= col + radius; ++c) {
            // 检查是否越界
            if (r >= 0 && r < m_rows && c >= 0 && c < m_columns) {
                // 计算当前点到炸弹中心的距离，如果在半径范围内，清除该点
                if (qSqrt(qPow(r - row, 2) + qPow(c - col, 2)) <= radius) {
                    m_board[r][c] = "";  // 将该位置清空
                }
            }
        }
    }
    emit boardChanged();  // 发出更新棋盘信号
    finalizeSwap(0,0,0,0,true);
}

Q_INVOKABLE void GameBoard::superItemEffectTriggered(int row, int col, QString color)
{
    // 遍历整个棋盘，找到指定颜色的方块并清空
    for (int r = 0; r < m_rows; ++r) {
        for (int c = 0; c < m_columns; ++c) {
            if (m_board[r][c] == color) {
                m_board[r][c] = "";  // 将符合颜色的方块清空
            }
        }
    }

    // 别忘记清空道具啊
    m_board[row][col] = "";

    emit boardChanged();  // 发出更新棋盘信号
    finalizeSwap(0, 0, 0, 0, true);  // 调用交换完成处理
}

void GameBoard::clearRow(int row) {
    for (int c = 0; c < m_columns; ++c) {
        m_board[row][c] = "";  // 将该行清空
    }
    emit boardChanged();
}

void GameBoard::clearColumn(int col) {
    for (int r = 0; r < m_rows; ++r) {
        m_board[r][col] = "";  // 将该列清空
    }
    emit boardChanged();
}


void GameBoard::shuffleBoard() {
    qDebug() << "重新洗牌";
    initializeBoard();
}

void GameBoard::startGame() {
    qDebug() << "开始游戏";
    m_score = 0;
    initializeBoard();
    emit scoreChanged(m_score);
}

void GameBoard::resetGame() {
    qDebug() << "重新开始游戏";
    startGame();
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
        return false;
    }

    // 检查是否相邻
    if ((qAbs(r1 - r2) == 1 && c1 == c2) || (qAbs(c1 - c2) == 1 && r1 == r2)) {
        return true;
    }

    return false;
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
            if (color.isEmpty() || isProp(color)) continue;

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
                else if(r == r2 && c == c2)
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
            if (color.isEmpty() || isProp(color)) continue;

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
                else if(r == r2 && c == c2)
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
            if (color.isEmpty() || isProp(color)) continue;

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
                        m_board[r][c] = Bomb;
                        qDebug() << QString("(%1, %2) %3 倒 T 字形 向左").arg(r).arg(c).arg(color);
                    }
                }
            }

            // 倒 T 字形 - 向右
            if (isInBounds(r-1, c) && isInBounds(r+1, c) && isInBounds(r, c+1) && isInBounds(r, c+2)) {
                if (m_board[r-1][c] == color && m_board[r+1][c] == color) {
                    if (m_board[r][c+1] == color && m_board[r][c+2] == color) {
                        bombMatches.append({BombType, QPoint(r, c)});  // 中心点
                        m_board[r][c] = Bomb;
                        qDebug() << QString("(%1, %2) %3 倒 T 字形 向右").arg(r).arg(c).arg(color);
                    }
                }
            }

            // L 字形 - 交点在右上角
            if (isInBounds(r+1, c) && isInBounds(r+2, c) && isInBounds(r, c-1) && isInBounds(r, c-2)) {
                if (m_board[r][c-1] == color && m_board[r][c-2] == color) {
                    if (m_board[r+1][c] == color && m_board[r+2][c] == color) {
                        bombMatches.append({BombType, QPoint(r, c)});  // 中心点
                        m_board[r][c] = Bomb;
                        qDebug() << QString("(%1, %2) %3 L 字形 - 交点在右上角").arg(r).arg(c).arg(color);
                    }
                }
            }

            // L 字形 - 交点在左上角
            if (isInBounds(r+1, c) && isInBounds(r+2, c) && isInBounds(r, c+1) && isInBounds(r, c+2)) {
                if (m_board[r][c+1] == color && m_board[r][c+2] == color) {
                    if (m_board[r+1][c] == color && m_board[r+2][c] == color) {
                        bombMatches.append({BombType, QPoint(r, c)});  // 中心点
                        m_board[r][c] = Bomb;
                        qDebug() << QString("(%1, %2) %3 L 字形 - 交点在左上角").arg(r).arg(c).arg(color);
                    }
                }
            }

            // L 字形 - 交点在左下角
            if (isInBounds(r-1, c) && isInBounds(r-2, c) && isInBounds(r, c+1) && isInBounds(r, c+2)) {
                if (m_board[r-1][c] == color && m_board[r-2][c] == color) {
                    if (m_board[r][c+1] == color && m_board[r][c+2] == color) {
                        bombMatches.append({BombType, QPoint(r, c)});  // 中心点
                        m_board[r][c] = Bomb;
                        qDebug() << QString("(%1, %2) %3 L 字形 - 交点在左下角").arg(r).arg(c).arg(color);
                    }
                }
            }

            // L 字形 - 交点在右下角
            if (isInBounds(r-1, c) && isInBounds(r-2, c) && isInBounds(r, c-1) && isInBounds(r, c-2)) {
                if (m_board[r-1][c] == color && m_board[r-2][c] == color) {
                    if (m_board[r][c-1] == color && m_board[r][c-2] == color) {
                        bombMatches.append({BombType, QPoint(r, c)});  // 中心点
                        m_board[r][c] = Bomb;
                        qDebug() << QString("(%1, %2) %3 L 字形 - 交点在右下角").arg(r).arg(c).arg(color);
                    }
                }
            }
        }
    }

    return bombMatches;
}



QVector<PropTypedef> GameBoard::findSuperItemMatches() {

    // 横向查找连续 5 个相同颜色的方块
    for (int r = 0; r < m_rows; ++r) {
        for (int c = 0; c < m_columns - 4; ++c) {
            QString color = m_board[r][c];
            if (color.isEmpty() || isProp(color)) continue;

            bool isMatch = true;
            for (int i = 1; i < 5; ++i) {
                if (m_board[r][c + i] != color) {
                    isMatch = false;
                    break;
                }
            }
            if (isMatch) {
                m_board[r][c+2] = SuperItem;
                superItemMatches.append({SuperItemType, QPoint(r, c + 2)}); // 超级道具放置位置是连续五个方块中的第三个方块
                qDebug() << QString("(%1, %2)超级道具").arg(r).arg(c+2);
            }
        }
    }

    // 纵向查找连续 5 个相同颜色的方块
    for (int c = 0; c < m_columns; ++c) {
        for (int r = 0; r < m_rows - 4; ++r) {
            QString color = m_board[r][c];
            if (color.isEmpty() || isProp(color)) continue;

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
            if (color.isEmpty() || isProp(color)) continue;

            int matchLength = 1;
            while (c + matchLength < m_columns && m_board[r][c + matchLength] == color && m_board[r][c + matchLength] != "") {
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
            if (color.isEmpty() || isProp(color)) continue;

            int matchLength = 1;
            while (r + matchLength < m_rows && m_board[r + matchLength][c] == color && m_board[r + matchLength][c] != "") {
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
        // 查找横向竖向火箭匹配
        findRocketMatches(r1, c1, r2, c2);

        // 查找 T 字形和 L 字形的炸弹匹配
        findBombMatches();

        // 查找五个
        findSuperItemMatches();

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
        int writeRow = m_rows - 1;

        for (int r = m_rows - 1; r >= 0; --r) {
            if (!m_board[r][c].isEmpty()) {
                if (r != writeRow) {
                    QVector<QPoint> path;
                    // 生成完整路径 (从 r 到 writeRow)
                    for (int rr = r; rr <= writeRow; ++rr) {
                        path.append(QPoint(rr, c));
                    }
                    dropPaths.append(path);
                }
                writeRow--;
            }
        }
    }
    return dropPaths;
}

// 把上方的格子往下掉
void GameBoard::applyGravity() {
    for (int c = 0; c < m_columns; ++c) {
        int writeRow = m_rows - 1;
        for (int r = m_rows - 1; r >= 0; --r) {
            if (m_board[r][c] != "") {
                m_board[writeRow][c] = m_board[r][c];
                if (writeRow != r) m_board[r][c] = "";
                writeRow--;
            }
        }
    }
}

void GameBoard::removeMatchedTiles(const QVector<QPoint> &matches) {
    for (const QPoint &pt : matches) {
        m_board[pt.x()][pt.y()] = ""; // 标记为空
    }
    updateScore(matches.size() * 10);
}

void GameBoard::creatProp()
{
    for (const PropTypedef &pt : rocketMatches) {
        m_board[pt.point.x()][pt.point.y()] = (pt.type == 1 ? Rocket_UpDown : Rocket_LeftRight);
        qDebug() << "火箭";
    }

    for (const PropTypedef &pt : bombMatches) {
        m_board[pt.point.x()][pt.point.y()] = Bomb;
        qDebug() << "炸弹";
    }

    for (const PropTypedef &pt : superItemMatches) {
        m_board[pt.point.x()][pt.point.y()] = SuperItem;
        qDebug() << "超级物品";
    }
}

void GameBoard::updateScore(int points) {
    m_score += points;
    emit scoreChanged(m_score);
}

