#include "GameBoardCompat.h"
#include <QRandomGenerator>
#include <QDebug>
#include "GameEngine.h"
#include <QMetaType>

using namespace core;

GameBoardCompat::GameBoardCompat(QObject *parent, int rows, int columns)
    : QObject(parent), m_rows(rows), m_columns(columns)
{
    // 使用新的 core GameEngine 作为后端
    m_legacy = nullptr; // 不再触碰旧 GameBoard
    m_engine = new GameEngine(rows, columns, m_numColors);

    // 初始化分数和步数
    m_score = m_engine->score();
    m_step  = m_engine->step();

    // 初始化棋盘，保证无初始三消
    if (m_engine) {
        m_engine->startGame();
        emit boardChanged();
    }
}

QString GameBoardCompat::tileAt(int row, int col) const {
    if (m_engine)
        return m_engine->tileAt(row, col);
    return "transparent";
}

QString GameBoardCompat::getTileColor(int row, int col) const {
    return tileAt(row, col);
}

QString GameBoardCompat::getRandomColorQml() const {
    if (m_engine)
        return m_engine->getRandomColor();
    return QString();
}

void GameBoardCompat::startGame() {
    if (m_engine) {
        m_engine->startGame();
        emit boardChanged();
    }
}

void GameBoardCompat::resetGame() {
    if (m_engine) {
        m_engine->resetGame();
        emit boardChanged();
    }
}

void GameBoardCompat::shuffleBoard() {
    if (m_engine) {
        m_engine->shuffleBoard();
        emit boardChanged();
    }
}

void GameBoardCompat::trySwap(int r1, int c1, int r2, int c2) {
    Q_UNUSED(m_engine);
    // 兼容旧接口：这里只负责发出交换动画请求，真正的逻辑在 finalizeSwap 中
    emit swapAnimationRequested(r1, c1, r2, c2);
}

static int toLegacyPropType(PropType pt) {
    switch (pt) {
    case PropType::RocketVertical:   return Rocket_UpDownType;    // 竖火箭
    case PropType::RocketHorizontal: return Rocket_LeftRightType; // 横火箭
    case PropType::BombProp:         return BombType;
    case PropType::SuperProp:        return SuperItemType;
    default:                         return 0;
    }
}

void GameBoardCompat::finalizeSwap(int r1, int c1, int r2, int c2, bool isRecursion) {
    Q_UNUSED(isRecursion);
    if (!m_engine)
        return;

    qDebug() << "GameBoardCompat::finalizeSwap called" << r1 << c1 << r2 << c2;

    GameEngine::FinalizeResult fres = m_engine->finalizeSwap(r1, c1, r2, c2);
    MatchResult mr = fres.result;

    // 根据 kind 区分处理
    if (fres.kind == GameEngine::FinalizeKind::SingleProp) {
        // 单体道具交换激活：先让 QML 播道具动画，动画结束后再由 QML 调用 xxxEffectTriggered
        int row = fres.propRow;
        int col = fres.propCol;
        int legacyType = toLegacyPropType(fres.propType);
        QString colorKey;
        if (fres.propType == PropType::SuperProp && fres.propColorIndex >= 1 && fres.propColorIndex <= 6) {
            // 将颜色 index 映射回 key，便于超级道具动画配色
            switch (fres.propColorIndex) {
            case 1: colorKey = "red"; break;
            case 2: colorKey = "green"; break;
            case 3: colorKey = "blue"; break;
            case 4: colorKey = "yellow"; break;
            case 5: colorKey = "purple"; break;
            case 6: colorKey = "brown"; break;
            default: break;
            }
        }
        qDebug() << "  SingleProp: emit propEffectRequested at" << row << col << "type=" << legacyType << "color=" << colorKey;
        emit propEffectRequested(row, col, legacyType, colorKey);
        // 不立即掉落，由 QML 在 xxxEffectTriggered -> activateXxx 后再发 dropAnimationRequested
        emit boardChanged();
        return;
    }

    if (fres.kind == GameEngine::FinalizeKind::ComboProp) {
        // 目前只实现火箭+火箭组合，其它组合回滚
        int row = fres.propRow;
        int col = fres.propCol;
        int legacyType = 0;
        QString colorKey;

        switch (fres.comboType) {
        case ComboType::RocketRocket:
            legacyType = Combo_RocketRocketType; // 100
            break;
        case ComboType::BombBomb:
            legacyType = Combo_BombBombType; // 101
            break;
        case ComboType::BombRocket:
            legacyType = Combo_BombRocketType; // 102
            colorKey = QString::number(fres.comboRocketType); // 1/2 传给 QML
            break;
        case ComboType::SuperBomb:
            legacyType = Combo_SuperBombType; // 103
            break;
        case ComboType::SuperRocket:
            legacyType = Combo_SuperRocketType; // 104
            break;
        case ComboType::SuperSuper:
            legacyType = Combo_SuperSuperType; // 105
            break;
        default:
            break;
        }

        if (legacyType != 0) {
            qDebug() << "  ComboProp: emit propEffectRequested at" << row << col << "type=" << legacyType << "colorKey=" << colorKey;
            emit propEffectRequested(row, col, legacyType, colorKey);
            emit boardChanged();
            return;
        }

        qDebug() << "  ComboProp not supported comboType, rollback";
        emit rollbackSwap(r1, c1, r2, c2);
        return;
    }

    if (fres.kind == GameEngine::FinalizeKind::NormalMatch) {
        // 普通三消：发匹配动画请求，由 QML 播放后再调用 processMatches()
        QVariantList matchedTiles;
        for (const QPoint &p : mr.matched) {
            QVariantMap m;
            m["row"] = p.y();
            m["col"] = p.x();
            matchedTiles << m;
        }
        emit matchAnimationRequested(matchedTiles);
        emit boardChanged();
        return;
    }

    // 无效果交换：回滚
    emit rollbackSwap(r1, c1, r2, c2);
}

void GameBoardCompat::processMatches() {
    if (!m_engine) return;

    qDebug() << "GameBoardCompat::processMatches";
    QVector<BoardModel::Drop> drops = m_engine->processOneCascadeStep();

    QVariantList dropPaths;
    for (const auto &d : drops) {
        QVariantMap m;
        m["fromRow"] = d.fromR;
        m["fromCol"] = d.fromC;
        m["toRow"]   = d.toR;
        m["toCol"]   = d.toC;
        m["color"]   = m_engine->tileAt(d.toR, d.toC);
        m["isNew"]   = d.isNew;
        dropPaths << m;
    }

    emit dropAnimationRequested(dropPaths);
    emit boardChanged();
}

void GameBoardCompat::processDrop() {
    // 兼容旧接口，内部与 commitDrop 等价
    commitDrop();
}

void GameBoardCompat::commitDrop() {
    if (!m_engine) return;

    qDebug() << "GameBoardCompat::commitDrop: trigger next cascade step";

    // 旧逻辑：只取 drops 并总是 emit dropAnimationRequested。
    // 新逻辑：优先检测本轮是否有新的匹配，有则先发 matchAnimationRequested 让 QML 播消除动画。

    // 通过一次 finalizeNoSwap 来探测“是否存在无需交换的匹配”。
    // 说明：在连锁/掉落阶段，棋盘已经变化，此处用 rows/cols 外的无效坐标触发引擎走一次匹配探测逻辑。
    auto fres = m_engine->finalizeNoSwap();
    if (fres.kind == GameEngine::FinalizeKind::NormalMatch) {
        QVariantList matchedTiles;
        for (const QPoint &p : fres.result.matched) {
            QVariantMap m;
            m["row"] = p.y();
            m["col"] = p.x();
            matchedTiles << m;
        }
        emit matchAnimationRequested(matchedTiles);
        emit boardChanged();
        return;
    }

    QVector<BoardModel::Drop> drops = m_engine->processOneCascadeStep();

    QVariantList dropPaths;
    for (const auto &d : drops) {
        QVariantMap m;
        m["fromRow"] = d.fromR;
        m["fromCol"] = d.fromC;
        m["toRow"]   = d.toR;
        m["toCol"]   = d.toC;
        m["color"]   = m_engine->tileAt(d.toR, d.toC);
        m["isNew"]   = d.isNew;
        dropPaths << m;
    }

    emit dropAnimationRequested(dropPaths);
    emit boardChanged();
}

void GameBoardCompat::rocketEffectTriggered(int row, int col, int type) {
    if (!m_engine) return;

    qDebug() << "GameBoardCompat::rocketEffectTriggered at" << row << col << "type" << type;

    PropType pt = (type == Rocket_UpDownType) ? PropType::RocketVertical : PropType::RocketHorizontal;
    QVector<BoardModel::Drop> drops = m_engine->activateRocket(row, col, pt);

    QVariantList dropPaths;
    for (const auto &d : drops) {
        QVariantMap m;
        m["fromRow"] = d.fromR;
        m["fromCol"] = d.fromC;
        m["toRow"]   = d.toR;
        m["toCol"]   = d.toC;
        m["color"]   = m_engine->tileAt(d.toR, d.toC);
        m["isNew"]   = d.isNew;
        dropPaths << m;
    }

    emit dropAnimationRequested(dropPaths);
    emit boardChanged();
}

void GameBoardCompat::bombEffectTriggered(int row, int col) {
    if (!m_engine) return;

    qDebug() << "GameBoardCompat::bombEffectTriggered at" << row << col;

    QVector<BoardModel::Drop> drops = m_engine->activateBomb(row, col);

    QVariantList dropPaths;
    for (const auto &d : drops) {
        QVariantMap m;
        m["fromRow"] = d.fromR;
        m["fromCol"] = d.fromC;
        m["toRow"]   = d.toR;
        m["toCol"]   = d.toC;
        m["color"]   = m_engine->tileAt(d.toR, d.toC);
        m["isNew"]   = d.isNew;
        dropPaths << m;
    }

    emit dropAnimationRequested(dropPaths);
    emit boardChanged();
}

static uint8_t colorKeyToIndex(const QString &key) {
    if (key == "red")    return 1;
    if (key == "green")  return 2;
    if (key == "blue")   return 3;
    if (key == "yellow") return 4;
    if (key == "purple") return 5;
    if (key == "brown")  return 6;
    return 0;
}

void GameBoardCompat::superItemEffectTriggered(int row, int col, QString color) {
    if (!m_engine) return;

    uint8_t idx = colorKeyToIndex(color);
    qDebug() << "GameBoardCompat::superItemEffectTriggered at" << row << col << "color" << color << "idx" << idx;

    QVector<BoardModel::Drop> drops = m_engine->activateSuper(row, col, idx);

    QVariantList dropPaths;
    for (const auto &d : drops) {
        QVariantMap m;
        m["fromRow"] = d.fromR;
        m["fromCol"] = d.fromC;
        m["toRow"]   = d.toR;
        m["toCol"]   = d.toC;
        m["color"]   = m_engine->tileAt(d.toR, d.toC);
        m["isNew"]   = d.isNew;
        dropPaths << m;
    }

    emit dropAnimationRequested(dropPaths);
    emit boardChanged();
}

void GameBoardCompat::comboRocketRocketEffectTriggered(int row, int col) {
    if (!m_engine) return;

    qDebug() << "GameBoardCompat::comboRocketRocketEffectTriggered at" << row << col;

    QVector<BoardModel::Drop> drops = m_engine->activateComboRocketRocket(row, col);

    QVariantList dropPaths;
    for (const auto &d : drops) {
        QVariantMap m;
        m["fromRow"] = d.fromR;
        m["fromCol"] = d.fromC;
        m["toRow"]   = d.toR;
        m["toCol"]   = d.toC;
        m["color"]   = m_engine->tileAt(d.toR, d.toC);
        m["isNew"]   = d.isNew;
        dropPaths << m;
    }

    emit dropAnimationRequested(dropPaths);
    emit boardChanged();
}

// 组合：炸弹+炸弹
Q_INVOKABLE void GameBoardCompat::comboBombBombEffectTriggered(int row, int col) {
    if (!m_engine) return;
    qDebug() << "GameBoardCompat::comboBombBombEffectTriggered at" << row << col;
    QVector<BoardModel::Drop> drops = m_engine->activateComboBombBomb(row, col);
    QVariantList dropPaths;
    for (const auto &d : drops) {
        QVariantMap m;
        m["fromRow"] = d.fromR;
        m["fromCol"] = d.fromC;
        m["toRow"]   = d.toR;
        m["toCol"]   = d.toC;
        m["color"]   = m_engine->tileAt(d.toR, d.toC);
        m["isNew"]   = d.isNew;
        dropPaths << m;
    }
    emit dropAnimationRequested(dropPaths);
    emit boardChanged();
}

// 组合：炸弹+火箭（color 传递火箭方向 1/2）
Q_INVOKABLE void GameBoardCompat::comboBombRocketEffectTriggered(int row, int col, int rocketType) {
    if (!m_engine) return;
    qDebug() << "GameBoardCompat::comboBombRocketEffectTriggered at" << row << col << "rocketType" << rocketType;
    QVector<BoardModel::Drop> drops = m_engine->activateComboBombRocket(row, col, rocketType);
    QVariantList dropPaths;
    for (const auto &d : drops) {
        QVariantMap m;
        m["fromRow"] = d.fromR;
        m["fromCol"] = d.fromC;
        m["toRow"]   = d.toR;
        m["toCol"]   = d.toC;
        m["color"]   = m_engine->tileAt(d.toR, d.toC);
        m["isNew"]   = d.isNew;
        dropPaths << m;
    }
    emit dropAnimationRequested(dropPaths);
    emit boardChanged();
}

// 组合：超级+炸弹
Q_INVOKABLE void GameBoardCompat::comboSuperBombEffectTriggered(int row, int col)
{
    if (!m_engine) return;
    qDebug() << "GameBoardCompat::comboSuperBombEffectTriggered at" << row << col;

    // 第一阶段：只做变炸弹，不产生掉落
    QVector<BoardModel::Drop> drops = m_engine->activateComboSuperBomb(row, col);
    Q_UNUSED(drops);

    emit boardChanged();
}

void GameBoardCompat::executeComboSuperBomb(int row, int col)
{
    if (!m_engine) return;
    qDebug() << "GameBoardCompat::executeComboSuperBomb at" << row << col;

    QVector<BoardModel::Drop> drops = m_engine->executeComboSuperBomb(row, col);

    QVariantList qdrops;
    qdrops.reserve(drops.size());
    for (const auto &d : drops) {
        QVariantMap m;
        m["fromRow"] = d.fromR;
        m["fromCol"] = d.fromC;
        m["toRow"]   = d.toR;
        m["toCol"]   = d.toC;
        m["color"]   = m_engine->tileAt(d.toR, d.toC);
        m["isNew"]   = d.isNew;
        qdrops.append(m);
    }

    emit boardChanged();
    if (!qdrops.isEmpty())
        emit dropAnimationRequested(qdrops);
}

// 组合：超级+火箭
Q_INVOKABLE void GameBoardCompat::comboSuperRocketEffectTriggered(int row, int col)
{
    if (!m_engine) return;
    qDebug() << "GameBoardCompat::comboSuperRocketEffectTriggered at" << row << col;

    // 第一阶段：只把选中颜色变为火箭
    QVector<BoardModel::Drop> drops = m_engine->activateComboSuperRocket(row, col);
    Q_UNUSED(drops);

    emit boardChanged();
}

void GameBoardCompat::executeComboSuperRocket(int row, int col)
{
    if (!m_engine) return;
    qDebug() << "GameBoardCompat::executeComboSuperRocket at" << row << col;

    QVector<BoardModel::Drop> drops = m_engine->executeComboSuperRocket(row, col);

    QVariantList qdrops;
    qdrops.reserve(drops.size());
    for (const auto &d : drops) {
        QVariantMap m;
        m["fromRow"] = d.fromR;
        m["fromCol"] = d.fromC;
        m["toRow"]   = d.toR;
        m["toCol"]   = d.toC;
        m["color"]   = m_engine->tileAt(d.toR, d.toC);
        m["isNew"]   = d.isNew;
        qdrops.append(m);
    }

    emit boardChanged();
    if (!qdrops.isEmpty())
        emit dropAnimationRequested(qdrops);
}

// 组合：超级+超级
Q_INVOKABLE void GameBoardCompat::comboSuperSuperEffectTriggered(int row, int col) {
    if (!m_engine) return;
    qDebug() << "GameBoardCompat::comboSuperSuperEffectTriggered at" << row << col;
    auto drops = m_engine->activateComboSuperSuper(row, col);
    QVariantList list;
    list.reserve(drops.size());
    for (const auto &d : drops) {
        QVariantMap m;
        m["fromR"] = d.fromR;
        m["fromC"] = d.fromC;
        m["toR"] = d.toR;
        m["toC"] = d.toC;
        m["color"] = static_cast<int>(d.color);
        m["isNew"] = d.isNew;
        list.push_back(m);
    }
    emit dropAnimationRequested(list);
}

QVariantList GameBoardCompat::debugFindPossibleSwaps() const {
    QVariantList list;
    // TODO: 可调用 MatchFinder 提供提示
    return list;
}

// ===== 新增：推演道具连锁触发步骤（仅用于前端动画，不改动棋盘） =====
// step: { row:int, col:int, type:int, color:string }
// type 使用旧 GameBoard.h 常量：1/2/3/4/100..105
static bool isPropName(const QString &name) {
    return name == "Rocket_1" || name == "Rocket_2" || name == "Bomb" || name == "SuperItem";
}

static int propNameToLegacyType(const QString &name) {
    if (name == "Rocket_1") return Rocket_UpDownType;
    if (name == "Rocket_2") return Rocket_LeftRightType;
    if (name == "Bomb") return BombType;
    if (name == "SuperItem") return SuperItemType;
    return 0;
}

static QString pickColorForSuperFromBoard(GameBoardCompat *gb, int row, int col) {
    if (!gb) return "";
    // 优先从 3 格半径内找基础色
    for (int radius = 0; radius <= 3; ++radius) {
        for (int dr = -radius; dr <= radius; ++dr) {
            for (int dc = -radius; dc <= radius; ++dc) {
                int rr = row + dr;
                int cc = col + dc;
                if (rr < 0 || rr >= gb->rows() || cc < 0 || cc >= gb->columns()) continue;
                QString v = gb->getTileColor(rr, cc);
                if (v != "transparent" && !isPropName(v)) return v;
            }
        }
    }
    // 再全盘找
    for (int r = 0; r < gb->rows(); ++r)
        for (int c = 0; c < gb->columns(); ++c) {
            QString v = gb->getTileColor(r, c);
            if (v != "transparent" && !isPropName(v)) return v;
        }
    return "";
}

QVariantList GameBoardCompat::previewPropChainFrom(int row, int col, int type, const QString &colorKey) {
    QVariantList steps;
    const int rows = m_rows;
    const int cols = m_columns;

    // 复制一份“名字棋盘”，用于推演（不影响真实棋盘）
    QVector<QVector<QString>> sim(rows, QVector<QString>(cols, "transparent"));
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            sim[r][c] = tileAt(r, c);
        }
    }

    auto inBounds = [&](int r, int c) { return r >= 0 && r < rows && c >= 0 && c < cols; };
    auto emitStep = [&](int r, int c, int t, const QString &ck) {
        QVariantMap m;
        m["row"] = r;
        m["col"] = c;
        m["type"] = t;
        m["color"] = ck;
        steps.push_back(m);
    };

    // 队列：按顺序触发
    struct Node { int r; int c; int t; QString color; int extra; };
    QVector<Node> q;
    q.push_back({row, col, type, colorKey, 0});

    // 去重：同一格只触发一次
    QVector<QVector<bool>> fired(rows, QVector<bool>(cols, false));

    auto scheduleIfProp = [&](int r, int c) {
        if (!inBounds(r, c)) return;
        if (fired[r][c]) return;
        QString v = sim[r][c];
        int t = propNameToLegacyType(v);
        if (t == 0) return;
        QString ck = "";
        if (t == SuperItemType) ck = pickColorForSuperFromBoard(this, r, c);
        q.push_back({r, c, t, ck, 0});
    };

    // 工具：清一个格子，并把里面的道具入队
    auto clearCell = [&](int r, int c) {
        if (!inBounds(r, c)) return;
        if (sim[r][c] == "transparent") return;
        if (isPropName(sim[r][c])) scheduleIfProp(r, c);
        sim[r][c] = "transparent";
    };

    while (!q.isEmpty()) {
        Node cur = q.front();
        q.pop_front();

        if (!inBounds(cur.r, cur.c)) continue;
        if (fired[cur.r][cur.c]) continue;
        fired[cur.r][cur.c] = true;

        // 记录触发步骤（供 QML 播放）
        emitStep(cur.r, cur.c, cur.t, cur.color);

        if (cur.t == BombType) {
            // 小炸弹半径 2 圆形
            int radius = 2;
            for (int r = cur.r - radius; r <= cur.r + radius; ++r) {
                for (int c = cur.c - radius; c <= cur.c + radius; ++c) {
                    if (!inBounds(r, c)) continue;
                    int dr = r - cur.r;
                    int dc = c - cur.c;
                    if (dr*dr + dc*dc > radius*radius) continue;
                    clearCell(r, c);
                }
            }
        } else if (cur.t == Rocket_UpDownType) {
            // 纵向火箭：清整列
            for (int r = 0; r < rows; ++r) clearCell(r, cur.c);
        } else if (cur.t == Rocket_LeftRightType) {
            // 横向火箭：清整行
            for (int c = 0; c < cols; ++c) clearCell(cur.r, c);
        } else if (cur.t == SuperItemType) {
            // 超级：清一种颜色；同时命中道具也会触发
            QString target = cur.color;
            if (target.isEmpty()) target = pickColorForSuperFromBoard(this, cur.r, cur.c);
            for (int r = 0; r < rows; r++) {
                for (int c = 0; c < cols; c++) {
                    if (sim[r][c] == "transparent") continue;
                    if (isPropName(sim[r][c])) {
                        scheduleIfProp(r, c);
                        sim[r][c] = "transparent";
                        continue;
                    }
                    if (!target.isEmpty() && sim[r][c] == target) sim[r][c] = "transparent";
                }
            }
        } else if (cur.t == Combo_BombBombType) {
            // 大炸弹：与后端一致 radius=4
            int radius = 4;
            for (int r = cur.r - radius; r <= cur.r + radius; ++r) {
                for (int c = cur.c - radius; c <= cur.c + radius; ++c) {
                    if (!inBounds(r, c)) continue;
                    int dr = r - cur.r;
                    int dc = c - cur.c;
                    if (dr*dr + dc*dc > radius*radius) continue;
                    clearCell(r, c);
                }
            }
        } else if (cur.t == Combo_BombRocketType) {
            // 炸弹+火箭：半径2 + 三行/三列（QML 现有表现），并且命中道具继续触发
            int bRadius = 2;
            for (int r = cur.r - bRadius; r <= cur.r + bRadius; ++r) {
                for (int c = cur.c - bRadius; c <= cur.c + bRadius; ++c) {
                    if (!inBounds(r, c)) continue;
                    int dr = r - cur.r;
                    int dc = c - cur.c;
                    if (dr*dr + dc*dc > bRadius*bRadius) continue;
                    clearCell(r, c);
                }
            }
            bool vertical = (cur.color.toInt() == 1);
            if (vertical) {
                for (int dc = -1; dc <= 1; ++dc) {
                    int cc = cur.c + dc;
                    if (cc < 0 || cc >= cols) continue;
                    for (int r = 0; r < rows; ++r) clearCell(r, cc);
                }
            } else {
                for (int dr = -1; dr <= 1; ++dr) {
                    int rr = cur.r + dr;
                    if (rr < 0 || rr >= rows) continue;
                    for (int c = 0; c < cols; ++c) clearCell(rr, c);
                }
            }
        } else {
            // 其它 combo 暂不推演（可后续增加）
        }
    }

    return steps;
}
