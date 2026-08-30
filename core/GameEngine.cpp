#include "GameEngine.h"
#include <QDebug>
#include <QRandomGenerator>

using namespace core;

// 简单编码约定：1-6 为普通颜色，
// 7 = 纵向火箭(Rocket_1), 8 = 横向火箭(Rocket_2), 9 = 炸弹, 10 = 超级道具
static uint8_t encodeProp(PropType type, uint8_t baseColor) {
    Q_UNUSED(baseColor);
    switch (type) {
    case PropType::RocketVertical:   return 7; // Rocket_1 竖向
    case PropType::RocketHorizontal: return 8; // Rocket_2 横向
    case PropType::BombProp:         return 9;
    case PropType::SuperProp:        return 10;
    default:                         return 0;
    }
}

static bool isBasicColor(uint8_t v) {
    return v >= 1 && v <= 6;
}

static bool isPropCode(uint8_t v) {
    return v >= 7 && v <= 10;
}

static PropType codeToPropType(uint8_t v) {
    switch (v) {
    case 7: return PropType::RocketVertical;   // 7 -> Rocket_1 竖向
    case 8: return PropType::RocketHorizontal; // 8 -> Rocket_2 横向
    case 9: return PropType::BombProp;
    case 10:return PropType::SuperProp;
    default:return PropType::None;
    }
}

// ===== 道具链式触发（bomb 命中其它道具会继续触发） =====
// 说明：
// - 只负责“同一轮清除逻辑”的连锁触发，不在中途下落；最后由调用者统一 applyGravityAndRefill。
// - 为避免死循环：同一格坐标的道具只允许触发一次（坐标去重）。
struct PropTrigger {
    int r = -1;
    int c = -1;
    uint8_t code = 0;        // 触发点当时的道具编码(7..10)
    uint8_t superColor = 0;  // 若为超级道具，需要指定目标颜色；为 0 则自动挑选
    bool isCombo = false;
    ComboType comboType = ComboType::None;
    int legacyRocketType = 0; // BombRocket 方向: 1 竖, 2 横
};

static inline quint32 packRC(int r, int c) {
    return (quint32(((quint32)r) & 0xFFFF) << 16) | (quint32(((quint32)c) & 0xFFFF));
}

uint8_t GameEngine::pickAnyBasicColorNear(int row, int col) const {
    int rows = m_board.rows();
    int cols = m_board.cols();
    for (int radius = 0; radius <= 3; ++radius) {
        for (int dr = -radius; dr <= radius; ++dr) {
            for (int dc = -radius; dc <= radius; ++dc) {
                int rr = row + dr;
                int cc = col + dc;
                if (rr < 0 || rr >= rows || cc < 0 || cc >= cols) continue;
                uint8_t v = m_board.tileAt(rr, cc);
                if (isBasicColor(v)) return v;
            }
        }
    }
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            uint8_t v = m_board.tileAt(r, c);
            if (isBasicColor(v)) return v;
        }
    }
    return 0;
}

void GameEngine::runPropChain(QVector<PropTrigger> &queue) {
    QVector<quint32> firedOnce;

    auto alreadyFired = [&](quint32 k) -> bool {
        return firedOnce.contains(k);
    };

    while (!queue.isEmpty()) {
        PropTrigger t = queue.front();
        queue.pop_front();
        if (t.r < 0 || t.c < 0 || t.r >= m_board.rows() || t.c >= m_board.cols()) continue;

        quint32 key = packRC(t.r, t.c);
        if (alreadyFired(key)) continue;
        firedOnce.push_back(key);

        // 组合触发的搭档道具已随组合消耗，不允许再被当作单体道具二次触发
        if (t.isCombo && m_pendingComboPartner.x() >= 0) {
            firedOnce.push_back(packRC(m_pendingComboPartner.x(), m_pendingComboPartner.y()));
            m_pendingComboPartner = QPoint(-1, -1);
        }

        uint8_t cur = m_board.tileAt(t.r, t.c);
        if (!isPropCode(cur)) {
            if (!t.isCombo) continue;
        }

        if (!t.isCombo) {
            PropType pt = codeToPropType(cur);
            if (pt == PropType::RocketVertical || pt == PropType::RocketHorizontal) {
                QVector<PropTrigger> newOnes;
                int rows = m_board.rows();
                int cols = m_board.cols();

                clearCellCounted(t.r, t.c);

                if (pt == PropType::RocketVertical) {
                    for (int r = 0; r < rows; ++r) {
                        uint8_t v = m_board.tileAt(r, t.c);
                        if (v == 0) continue;
                        if (isPropCode(v)) newOnes.push_back({r, t.c, v, 0, false, ComboType::None, 0});
                        clearCellCounted(r, t.c);
                    }
                } else {
                    for (int c = 0; c < cols; ++c) {
                        uint8_t v = m_board.tileAt(t.r, c);
                        if (v == 0) continue;
                        if (isPropCode(v)) newOnes.push_back({t.r, c, v, 0, false, ComboType::None, 0});
                        clearCellCounted(t.r, c);
                    }
                }

                queue += newOnes;

            } else if (pt == PropType::BombProp) {
                QVector<PropTrigger> newOnes;

                clearCellCounted(t.r, t.c);

                int radius = 2;
                int rows = m_board.rows();
                int cols = m_board.cols();
                for (int r = t.r - radius; r <= t.r + radius; ++r) {
                    for (int c = t.c - radius; c <= t.c + radius; ++c) {
                        if (r < 0 || r >= rows || c < 0 || c >= cols) continue;
                        int dr = r - t.r;
                        int dc = c - t.c;
                        if (dr*dr + dc*dc > radius*radius) continue;
                        if (r == t.r && c == t.c) continue;

                        uint8_t v = m_board.tileAt(r, c);
                        if (v == 0) continue;
                        if (isPropCode(v)) newOnes.push_back({r, c, v, 0, false, ComboType::None, 0});
                        clearCellCounted(r, c);
                    }
                }

                queue += newOnes;

            } else if (pt == PropType::SuperProp) {
                uint8_t color = t.superColor;
                if (color == 0) color = pickAnyBasicColorNear(t.r, t.c);

                QVector<PropTrigger> newOnes;
                int rows = m_board.rows();
                int cols = m_board.cols();

                clearCellCounted(t.r, t.c);

                if (color != 0) {
                    for (int r = 0; r < rows; ++r) {
                        for (int c = 0; c < cols; ++c) {
                            uint8_t v = m_board.tileAt(r, c);
                            if (v == 0) continue;
                            if (isPropCode(v)) {
                                newOnes.push_back({r, c, v, 0, false, ComboType::None, 0});
                                clearCellCounted(r, c);
                                continue;
                            }
                            if (v == color) {
                                clearCellCounted(r, c);
                            }
                        }
                    }
                }

                queue += newOnes;
            }

            continue;
        }

        // combo 触发：只处理 bomb 相关（你需求里“炸弹激活范围内包含其它道具继续触发”重点在这里）
        if (t.comboType == ComboType::BombBomb) {
            QVector<PropTrigger> newOnes;
            int radius = 4;
            int rows = m_board.rows();
            int cols = m_board.cols();
            for (int r = t.r - radius; r <= t.r + radius; ++r) {
                for (int c = t.c - radius; c <= t.c + radius; ++c) {
                    if (r < 0 || r >= rows || c < 0 || c >= cols) continue;
                    int dr = r - t.r;
                    int dc = c - t.c;
                    if (dr*dr + dc*dc > radius*radius) continue;
                    uint8_t v = m_board.tileAt(r, c);
                    if (v == 0) continue;
                    // 关键：组合中心格不应该再被 schedule 成“单体道具触发”
                    if (!(r == t.r && c == t.c) && isPropCode(v))
                        newOnes.push_back({r, c, v, 0, false, ComboType::None, 0});
                    clearCellCounted(r, c);
                }
            }
            queue += newOnes;

        } else if (t.comboType == ComboType::BombRocket) {
            QVector<PropTrigger> newOnes;
            int rows = m_board.rows();
            int cols = m_board.cols();

            {
                int radius = 2;
                for (int r = t.r - radius; r <= t.r + radius; ++r) {
                    for (int c = t.c - radius; c <= t.c + radius; ++c) {
                        if (r < 0 || r >= rows || c < 0 || c >= cols) continue;
                        int dr = r - t.r;
                        int dc = c - t.c;
                        if (dr*dr + dc*dc > radius*radius) continue;
                        uint8_t v = m_board.tileAt(r, c);
                        if (v == 0) continue;
                        // 关键：组合中心格不应该再被 schedule 成“单体道具触发”
                        if (!(r == t.r && c == t.c) && isPropCode(v))
                            newOnes.push_back({r, c, v, 0, false, ComboType::None, 0});
                        clearCellCounted(r, c);
                    }
                }
            }

            bool vertical = (t.legacyRocketType == 1);
            if (vertical) {
                for (int dc = -1; dc <= 1; ++dc) {
                    int cc = t.c + dc; if (cc < 0 || cc >= cols) continue;
                    for (int r = 0; r < rows; ++r) {
                        uint8_t v = m_board.tileAt(r, cc);
                        if (v == 0) continue;
                        // 关键：组合中心格不应该再被 schedule 成“单体道具触发”
                        if (!(r == t.r && cc == t.c) && isPropCode(v))
                            newOnes.push_back({r, cc, v, 0, false, ComboType::None, 0});
                        clearCellCounted(r, cc);
                    }
                }
            } else {
                for (int dr = -1; dr <= 1; ++dr) {
                    int rr = t.r + dr; if (rr < 0 || rr >= rows) continue;
                    for (int c = 0; c < cols; ++c) {
                        uint8_t v = m_board.tileAt(rr, c);
                        if (v == 0) continue;
                        // 关键：组合中心格不应该再被 schedule 成“单体道具触发”
                        if (!(rr == t.r && c == t.c) && isPropCode(v))
                            newOnes.push_back({rr, c, v, 0, false, ComboType::None, 0});
                        clearCellCounted(rr, c);
                    }
                }
            }

            queue += newOnes;
        }
    }
}

GameEngine::GameEngine(int rows, int cols, int numColors)
    : m_board(rows, cols, numColors), m_score(0), m_step(0)
{
}

void GameEngine::clearCellCounted(int r, int c) {
    int rows = m_board.rows();
    int cols = m_board.cols();
    if (r < 0 || r >= rows || c < 0 || c >= cols) return;
    uint8_t v = m_board.tileAt(r, c);
    if (v == 0) return;
    if (v >= 1 && v <= 6) m_stats[v - 1]++; // 基础色消除统计
    m_score += 10;
    m_board.setTile(r, c, 0);
}

QString GameEngine::tileAt(int r, int c) const {
    return m_board.colorName(m_board.tileAt(r,c));
}

QString GameEngine::getRandomColor() const {
    return m_board.colorName((uint8_t)(QRandomGenerator::global()->bounded(1, m_board.numColors()+1)));
}

bool GameEngine::canSwap(int r1,int c1,int r2,int c2) const {
    int dr = abs(r1-r2); int dc = abs(c1-c2);
    return (dr==1 && dc==0) || (dr==0 && dc==1);
}

MatchResult GameEngine::simulateSwapAndFindMatches(int r1,int c1,int r2,int c2) const {
    if (!canSwap(r1,c1,r2,c2))
        return MatchResult{};
    BoardModel copy = m_board;
    copy.swapTiles(r1,c1,r2,c2);
    LastSwapInfo info(r1,c1,r2,c2);
    return MatchFinder::findMatches(copy, &info);
}

GameEngine::FinalizeResult GameEngine::finalizeNoSwap() {
    FinalizeResult out;

    // 不发生交换，只在当前棋盘上找匹配
    MatchResult mr = MatchFinder::findMatches(m_board, nullptr);
    if (!mr.matched.isEmpty()) {
        out.kind = FinalizeKind::NormalMatch;
        out.result = mr;
    }

    return out;
}

GameEngine::FinalizeResult GameEngine::finalizeSwap(int r1,int c1,int r2,int c2) {
    FinalizeResult out;

    qDebug() << "GameEngine::finalizeSwap called" << r1 << c1 << r2 << c2;
    qDebug() << "  board BEFORE finalize:\n" << m_board.toString();

    if (!canSwap(r1,c1,r2,c2)) {
        qDebug() << "  canSwap=false, illegal";
        return out; // kind 默认为 None
    }

    uint8_t v1 = m_board.tileAt(r1, c1);
    uint8_t v2 = m_board.tileAt(r2, c2);
    bool p1 = isPropCode(v1);
    bool p2 = isPropCode(v2);

    // === Case 1: 单体道具 + 普通颜色（只识别，不清场）===
    if ((p1 && isBasicColor(v2)) || (p2 && isBasicColor(v1))) {
        qDebug() << "  finalizeSwap: single-prop swap detected" << "v1=" << int(v1) << "v2=" << int(v2);

        // 真实交换到棋盘上：道具移动到目标位置
        m_board.swapTiles(r1, c1, r2, c2);

        int propR, propC; uint8_t propCode; uint8_t colorCode;
        if (p1) { // 第一个位置原来是道具
            propR = r2; propC = c2; propCode = v1; colorCode = v2;
        } else { // p2
            propR = r1; propC = c1; propCode = v2; colorCode = v1;
        }

        PropType pt = codeToPropType(propCode);
        out.kind = FinalizeKind::SingleProp;
        out.result = MatchResult{}; // 不通过匹配产生
        out.propRow = propR;
        out.propCol = propC;
        out.propType = pt;
        out.propColorIndex = isBasicColor(colorCode) ? colorCode : 0;

        qDebug() << "  single-prop swap finalized logically at (row,col)=" << propR << propC
                 << " type=" << int(pt) << " colorIndex=" << int(out.propColorIndex);
        qDebug() << "  board AFTER swap (no clear yet):\n" << m_board.toString();
        return out;
    }

    // === Case 2: 道具 + 道具（组合） ===
    if (p1 && p2) {
        PropType t1 = codeToPropType(v1);
        PropType t2 = codeToPropType(v2);
        qDebug() << "  finalizeSwap: prop+prop (combo) detected" << (int)t1 << (int)t2;

        m_board.swapTiles(r1, c1, r2, c2);

        out.kind = FinalizeKind::ComboProp;
        out.propRow = r2;
        out.propCol = c2;
        out.comboType = ComboType::None;
        out.comboRocketType = 0;
        // 记录搭档道具坐标：组合触发时随组合消耗，不再二次单体触发
        m_pendingComboPartner = QPoint(r1, c1);

        bool rocket1 = (t1 == PropType::RocketHorizontal || t1 == PropType::RocketVertical);
        bool rocket2 = (t2 == PropType::RocketHorizontal || t2 == PropType::RocketVertical);
        bool bomb1   = (t1 == PropType::BombProp);
        bool bomb2   = (t2 == PropType::BombProp);
        bool super1  = (t1 == PropType::SuperProp);
        bool super2  = (t2 == PropType::SuperProp);

        if (super1 && super2) {
            out.comboType = ComboType::SuperSuper;
        } else if ((super1 && bomb2) || (super2 && bomb1)) {
            out.comboType = ComboType::SuperBomb;
        } else if ((super1 && rocket2) || (super2 && rocket1)) {
            out.comboType = ComboType::SuperRocket;
        } else if (rocket1 && rocket2) {
            out.comboType = ComboType::RocketRocket;
        } else if (bomb1 && bomb2) {
            out.comboType = ComboType::BombBomb;
        } else if ((bomb1 && rocket2) || (bomb2 && rocket1)) {
            out.comboType = ComboType::BombRocket;
            // 记录火箭方向，兼容旧的 Rocket_UpDownType / Rocket_LeftRightType
            PropType rt = rocket1 ? t1 : t2;
            out.comboRocketType = (rt == PropType::RocketVertical) ? 1 : 2;
        }

        qDebug() << "  comboType=" << (int)out.comboType << "comboRocketType=" << out.comboRocketType;
        qDebug() << "  board AFTER combo swap (no clear yet):\n" << m_board.toString();
        return out;
    }

    // === Case 3: 普通交换，按原逻辑寻找三消 ===
    MatchResult mr = simulateSwapAndFindMatches(r1,c1,r2,c2);
    qDebug() << "  simulateSwapAndFindMatches matched size=" << mr.matched.size();

    if (mr.matched.isEmpty()) {
        return out; // kind 仍为 None
    }

    m_board.swapTiles(r1,c1,r2,c2);
    qDebug() << "  board AFTER applying swap (no remove yet):\n" << m_board.toString();

    out.result = mr;
    out.drops.clear();
    out.kind = FinalizeKind::NormalMatch;
    return out;
}

QVector<BoardModel::Drop> GameEngine::removeMatches(const MatchResult &mr) {
    QVector<BoardModel::Drop> drops;
    drops.reserve(m_board.rows() * m_board.cols());

    if (mr.matched.isEmpty())
        return drops;

    qDebug() << "GameEngine::removeMatches: removing" << mr.matched.size() << "tiles, suggestions=" << mr.suggestions.size();

    int rows = m_board.rows();
    int cols = m_board.cols();

    // 1) 先构建一个标记矩阵，标出所有要清除的位置
    QVector<QVector<bool>> toClear(rows, QVector<bool>(cols, false));
    for (const QPoint &p : mr.matched) {
        int r = p.y();
        int c = p.x();
        if (r < 0 || r >= rows || c < 0 || c >= cols) continue;
        toClear[r][c] = true;
    }

    // 2) 对于 MatchFinder 给出的道具建议，在对应位置保留一个“道具格子”（不清空）
    //    简化策略：若多个建议指向同一格，后来的会覆盖前面的类型
    for (const PropSuggestion &s : mr.suggestions) {
        int r = s.pos.y();
        int c = s.pos.x();
        if (r < 0 || r >= rows || c < 0 || c >= cols) continue;
        uint8_t baseColor = m_board.tileAt(r, c);
        uint8_t propCode = encodeProp(s.type, baseColor);
        if (propCode == 0) continue;

        // 在此格生成道具：不将其记为 toClear，而是直接写入道具编码
        toClear[r][c] = false;
        m_board.setTile(r, c, propCode);
        qDebug() << "GameEngine::removeMatches: create prop" << int(propCode) << "at (row,col)=" << r << c;
    }

    // 3) 清空其余匹配到的位置
    for (const QPoint &p : mr.matched) {
        int r = p.y();
        int c = p.x();
        if (r < 0 || r >= rows || c < 0 || c >= cols) continue;
        if (!toClear[r][c]) continue; // 已被保留为道具
        qDebug() << "  clear tile at (row,col)=" << r << c;
        clearCellCounted(r, c);
    }

    qDebug() << "GameEngine::removeMatches applied, board:\n" << m_board.toString();

    // 4) 应用重力并生成掉落信息
    drops = m_board.applyGravityAndRefill();
    qDebug() << "  drops count=" << drops.size();
    for (int i = 0; i < drops.size(); ++i) {
        const auto &d = drops[i];
        qDebug() << "    drop" << i << ": from(" << d.fromR << "," << d.fromC
                 << ") to(" << d.toR << "," << d.toC << ") colorIndex=" << int(d.color)
                 << " isNew=" << d.isNew;
    }

    return drops;
}

// 新增：单轮连锁处理，只处理当前棋盘上的一轮匹配
QVector<BoardModel::Drop> GameEngine::processOneCascadeStep() {
    QVector<BoardModel::Drop> drops;

    MatchResult mr = MatchFinder::findMatches(m_board, nullptr);
    if (mr.matched.isEmpty()) {
        qDebug() << "GameEngine::processOneCascadeStep: no matches";
        return drops;
    }

    qDebug() << "GameEngine::processOneCascadeStep: found" << mr.matched.size() << "matched tiles";
    drops = removeMatches(mr);
    return drops;
}

// 连锁处理：保留原有多轮聚合接口，内部复用单轮函数
QVector<BoardModel::Drop> GameEngine::processAllMatches() {
    QVector<BoardModel::Drop> totalDrops;

    qDebug() << "GameEngine::processAllMatches START, board=\n" << m_board.toString();

    while (true) {
        MatchResult mr = MatchFinder::findMatches(m_board, nullptr);
        if (mr.matched.isEmpty()) {
            qDebug() << "  iteration : found 0 matched tiles";
            break;
        }

        qDebug() << "  iteration : found" << mr.matched.size() << "matched tiles";
        QVector<BoardModel::Drop> stepDrops = removeMatches(mr);
        totalDrops += stepDrops;
    }

    qDebug() << "GameEngine::processAllMatches END, total drops=" << totalDrops.size();
    return totalDrops;
}

void GameEngine::startGame() {
    // 初始化棋盘：保证没有任何三消
    qDebug() << "GameEngine::startGame initializing board with no initial matches";

    for (int i = 0; i < 15; ++i) m_stats[i] = 0;
    m_pendingComboPartner = QPoint(-1, -1);

    int rows = m_board.rows();
    int cols = m_board.cols();

    auto randomColorIndex = [this]() -> uint8_t {
        return (uint8_t)(QRandomGenerator::global()->bounded(1, m_board.numColors()+1));
    };

    // 循环直到整盘没有三消（通常很快收敛，如果需要可以加保护上限）
    int guard = 0;
    const int GUARD_MAX = 100;
    while (true) {
        // 逐格填充，并避免与左/上的直接三连
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                uint8_t color;
                int innerGuard = 0;
                do {
                    color = randomColorIndex();
                    innerGuard++;
                    // 避免水平三连
                    bool bad = false;
                    if (c >= 2) {
                        uint8_t c1 = m_board.tileAt(r, c-1);
                        uint8_t c2 = m_board.tileAt(r, c-2);
                        if (c1 == color && c2 == color) bad = true;
                    }
                    // 避免垂直三连
                    if (!bad && r >= 2) {
                        uint8_t c1 = m_board.tileAt(r-1, c);
                        uint8_t c2 = m_board.tileAt(r-2, c);
                        if (c1 == color && c2 == color) bad = true;
                    }
                    if (!bad) break;
                } while (innerGuard < 20);

                m_board.setTile(r, c, color);
            }
        }

        // 使用 MatchFinder 再全盘扫一遍，确保没有更复杂的 T/L 形等三消
        LastSwapInfo dummy; // 无实际交换，仅作上下文占位
        MatchResult mr = MatchFinder::findMatches(m_board, &dummy);
        if (mr.matched.isEmpty()) {
            qDebug() << "GameEngine::startGame -> board initialized with no matches";
            break;
        }

        guard++;
        if (guard >= GUARD_MAX) {
            qDebug() << "GameEngine::startGame: guard reached, still has" << mr.matched.size() << "matches, accepting board as-is";
            break;
        }
    }
}

void GameEngine::resetGame() {
    m_score = 0;
    m_step = 0; // 如有步数逻辑，可按需重置
    for (int i = 0; i < 15; ++i) m_stats[i] = 0;
    m_pendingComboPartner = QPoint(-1, -1);
    startGame();
}

void GameEngine::shuffleBoard() {
    // 简单实现：重新初始化一盘无三消的新棋盘
    startGame();
}

// ===== 单体道具激活实现 =====

QVector<BoardModel::Drop> GameEngine::activateRocket(int row, int col, PropType type) {
    QVector<BoardModel::Drop> drops;
    int rows = m_board.rows();
    int cols = m_board.cols();
    if (row < 0 || row >= rows || col < 0 || col >= cols) return drops;

    qDebug() << "GameEngine::activateRocket at" << row << col << "type" << (int)type;
    addStat(6); // 单体火箭触发

    // 改为：火箭清行/列 + 链式触发（命中道具继续触发），最后统一下落一次
    QVector<PropTrigger> q;
    const uint8_t legacyType = (type == PropType::RocketVertical) ? 7 : 8; // 7=竖向火箭, 8=横向火箭（与 runPropChain 的约定一致）
    q.push_back({row, col, legacyType, 0, false, ComboType::None, 0});
    runPropChain(q);

    drops = m_board.applyGravityAndRefill();
    qDebug() << "GameEngine::activateRocket -> chain finished, drops=" << drops.size();
    return drops;
}

QVector<BoardModel::Drop> GameEngine::activateBomb(int row, int col) {
    QVector<BoardModel::Drop> drops;
    int rows = m_board.rows();
    int cols = m_board.cols();
    if (row < 0 || row >= rows || col < 0 || col >= cols) return drops;

    qDebug() << "GameEngine::activateBomb at" << row << col;
    addStat(7); // 单体炸弹触发

    // 改为：炸弹爆炸 + 链式触发（命中道具继续触发），最后统一下落一次
    QVector<PropTrigger> q;
    q.push_back({row, col, 9, 0, false, ComboType::None, 0});
    runPropChain(q);

    drops = m_board.applyGravityAndRefill();
    qDebug() << "GameEngine::activateBomb -> chain finished, drops=" << drops.size();
    return drops;
}

QVector<BoardModel::Drop> GameEngine::activateSuper(int row, int col, uint8_t colorIndex) {
    QVector<BoardModel::Drop> drops;
    int rows = m_board.rows();
    int cols = m_board.cols();
    if (row < 0 || row >= rows || col < 0 || col >= cols) return drops;

    qDebug() << "GameEngine::activateSuper at" << row << col << "colorIndex" << colorIndex;
    addStat(8); // 单体超级道具触发

    // 空颜色（如 QML 双击激活未指定目标色）时自动挑选邻近基础色
    uint8_t color = colorIndex;
    if (color == 0) color = pickAnyBasicColorNear(row, col);
    if (color == 0) return drops;

    // 清除触发的超级道具自身
    m_board.setTile(row, col, 0);

    int cleared = 0;
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            uint8_t v = m_board.tileAt(r, c);
            if (v == 0) continue;
            if (v == color) {
                m_board.setTile(r, c, 0);
                ++cleared;
            }
        }
    }

    if (cleared > 0) {
        m_score += cleared * 10;
    }

    drops = m_board.applyGravityAndRefill();
    qDebug() << "GameEngine::activateSuper -> cleared" << cleared << "tiles, drops=" << drops.size();
    return drops;
}

QVector<BoardModel::Drop> GameEngine::activateComboBombBomb(int row, int col) {
    QVector<BoardModel::Drop> drops;
    int rows = m_board.rows();
    int cols = m_board.cols();
    if (row < 0 || row >= rows || col < 0 || col >= cols) return drops;

    qDebug() << "GameEngine::activateComboBombBomb at" << row << col;
    addStat(10); // 炸弹+炸弹组合

    QVector<PropTrigger> q;
    q.push_back({row, col, 9, 0, true, ComboType::BombBomb, 0});
    runPropChain(q);

    drops = m_board.applyGravityAndRefill();
    qDebug() << "GameEngine::activateComboBombBomb -> chain finished, drops=" << drops.size();
    return drops;
}

QVector<BoardModel::Drop> GameEngine::activateComboRocketRocket(int row, int col) {
    // 之前已实现：清整行 + 整列
    QVector<BoardModel::Drop> drops;
    int rows = m_board.rows();
    int cols = m_board.cols();
    if (row < 0 || row >= rows || col < 0 || col >= cols) return drops;

    qDebug() << "GameEngine::activateComboRocketRocket at" << row << col;
    addStat(9); // 火箭+火箭组合

    int cleared = 0;
    if (m_board.tileAt(row, col) != 0) { m_board.setTile(row, col, 0); ++cleared; }
    for (int c = 0; c < cols; ++c) {
        if (c == col) continue;
        uint8_t v = m_board.tileAt(row, c);
        if (v == 0) continue;
        m_board.setTile(row, c, 0);
        ++cleared;
    }
    for (int r = 0; r < rows; ++r) {
        if (r == row) continue;
        uint8_t v = m_board.tileAt(r, col);
        if (v == 0) continue;
        m_board.setTile(r, col, 0);
        ++cleared;
    }

    if (cleared > 0) m_score += cleared * 10;
    drops = m_board.applyGravityAndRefill();
    qDebug() << "GameEngine::activateComboRocketRocket -> cleared" << cleared << "tiles, drops=" << drops.size();
    return drops;
}

QVector<BoardModel::Drop> GameEngine::activateComboBombRocket(int row, int col, int legacyRocketType) {
    QVector<BoardModel::Drop> drops;
    int rows = m_board.rows();
    int cols = m_board.cols();
    if (row < 0 || row >= rows || col < 0 || col >= cols) return drops;

    qDebug() << "GameEngine::activateComboBombRocket at" << row << col << "legacyRocketType" << legacyRocketType;
    addStat(11); // 炸弹+火箭组合

    QVector<PropTrigger> q;
    q.push_back({row, col, 9, 0, true, ComboType::BombRocket, legacyRocketType});
    runPropChain(q);

    drops = m_board.applyGravityAndRefill();
    qDebug() << "GameEngine::activateComboBombRocket -> chain finished, drops=" << drops.size();
    return drops;
}

QVector<BoardModel::Drop> GameEngine::activateComboSuperBomb(int row, int col)
{
    qDebug() << "GameEngine::activateComboSuperBomb (stage1: transform to bombs) at" << row << col;
    addStat(13); // 超级+炸弹组合

    int rows = m_board.rows();
    int cols = m_board.cols();

    // 选择一个目标基础颜色（1..6），优先从附近找，其次全盘找
    auto pickTargetColor = [&]() -> uint8_t {
        for (int radius = 0; radius <= 3; ++radius) {
            for (int dr = -radius; dr <= radius; ++dr) {
                for (int dc = -radius; dc <= radius; ++dc) {
                    int rr = row + dr;
                    int cc = col + dc;
                    if (rr < 0 || rr >= rows || cc < 0 || cc >= cols) continue;
                    uint8_t v = m_board.tileAt(rr, cc);
                    if (v >= 1 && v <= 6) return v;
                }
            }
        }
        for (int r = 0; r < rows; ++r)
            for (int c = 0; c < cols; ++c) {
                uint8_t v = m_board.tileAt(r, c);
                if (v >= 1 && v <= 6) return v;
            }
        return 0;
    };

    uint8_t targetColor = pickTargetColor();
    qDebug() << "GameEngine::activateComboSuperBomb picked color index" << targetColor;
    if (targetColor == 0)
        return {};

    // 只把所有该颜色格子变成炸弹(9)，不清除、不下落
    int changed = 0;
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            if (m_board.tileAt(r, c) == targetColor) {
                m_board.setTile(r, c, 9);
                ++changed;
            }
        }
    }
    qDebug() << "GameEngine::activateComboSuperBomb stage1 transformed" << changed << "tiles to bombs";

    // 第一阶段不产生掉落
    return {};
}

// 第二阶段：根据当前棋盘上的炸弹执行全盘爆炸并下落
QVector<BoardModel::Drop> GameEngine::executeComboSuperBomb(int row, int col)
{
    Q_UNUSED(row);
    Q_UNUSED(col);
    qDebug() << "GameEngine::executeComboSuperBomb (stage2: explode all bombs)";

    int rows = m_board.rows();
    int cols = m_board.cols();

    // 关键修复：第二阶段不能直接“清全盘”，否则被炸到的火箭/炸弹/超级都不会按规则继续连锁触发。
    // 正确做法：把当前棋盘上的所有炸弹(9)入队，让 runPropChain 执行：炸弹爆炸 -> 命中其它道具继续入队。
    QVector<PropTrigger> q;
    q.reserve(rows * cols);

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            if (m_board.tileAt(r, c) == 9) {
                q.push_back({r, c, 9, 0, false, ComboType::None, 0});
            }
        }
    }

    if (!q.isEmpty()) {
        runPropChain(q);
    }

    // 统一下落一次
    QVector<BoardModel::Drop> drops = m_board.applyGravityAndRefill();
    qDebug() << "GameEngine::executeComboSuperBomb -> chain finished, bombs triggered =" << q.size()
             << " drops=" << drops.size();
    return drops;
}

QVector<BoardModel::Drop> GameEngine::activateComboSuperRocket(int row, int col)
{
    qDebug() << "GameEngine::activateComboSuperRocket (stage1: transform to rockets) at" << row << col;
    addStat(12); // 超级+火箭组合

    int rows = m_board.rows();
    int cols = m_board.cols();

    auto pickTargetColor = [&]() -> uint8_t {
        for (int radius = 0; radius <= 3; ++radius) {
            for (int dr = -radius; dr <= radius; ++dr) {
                for (int dc = -radius; dc <= radius; ++dc) {
                    int rr = row + dr;
                    int cc = col + dc;
                    if (rr < 0 || rr >= rows || cc < 0 || cc >= cols) continue;
                    uint8_t v = m_board.tileAt(rr, cc);
                    if (v >= 1 && v <= 6) return v;
                }
            }
        }
        for (int r = 0; r < rows; ++r)
            for (int c = 0; c < cols; ++c) {
                uint8_t v = m_board.tileAt(r, c);
                if (v >= 1 && v <= 6) return v;
            }
        return 0;
    };

    uint8_t targetColor = pickTargetColor();
    qDebug() << "GameEngine::activateComboSuperRocket picked color index" << targetColor;
    if (targetColor == 0)
        return {};

    int changed = 0;
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            uint8_t v = m_board.tileAt(r, c);
            if (v == targetColor) {
                uint8_t rocket = (c % 2 == 0) ? 7 : 8; // 偶列纵火箭，奇列横火箭
                m_board.setTile(r, c, rocket);
                ++changed;
            }
        }
    }
    qDebug() << "GameEngine::activateComboSuperRocket stage1 transformed" << changed << "tiles to rockets";

    // 第一阶段不清不掉落
    return {};
}

// 第二阶段：根据当前棋盘上的火箭，清除对应行列并下落
QVector<BoardModel::Drop> GameEngine::executeComboSuperRocket(int row, int col)
{
    Q_UNUSED(row);
    Q_UNUSED(col);
    qDebug() << "GameEngine::executeComboSuperRocket (stage2: fire all rockets)";

    int rows = m_board.rows();
    int cols = m_board.cols();

    QVector<bool> clearRow(rows, false), clearCol(cols, false);

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            uint8_t v = m_board.tileAt(r, c);
            if (v == 7) { // 纵向火箭
                clearCol[c] = true;
            } else if (v == 8) { // 横向火箭
                clearRow[r] = true;
            }
        }
    }

    int cleared = 0;
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            if (clearRow[r] || clearCol[c]) {
                if (m_board.tileAt(r, c) != 0) {
                    m_board.setTile(r, c, 0);
                    ++cleared;
                }
            }
        }
    }
    qDebug() << "GameEngine::executeComboSuperRocket cleared" << cleared << "tiles";

    return m_board.applyGravityAndRefill();
}

QVector<BoardModel::Drop> GameEngine::activateComboSuperSuper(int row, int col)
{
    qDebug() << "GameEngine::activateComboSuperSuper at" << row << col;
    addStat(14); // 超级+超级组合

    int rows = m_board.rows();
    int cols = m_board.cols();

    int cleared = 0;
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            if (m_board.tileAt(r, c) != 0) {
                m_board.setTile(r, c, 0);
                ++cleared;
            }
        }
    }
    qDebug() << "GameEngine::activateComboSuperSuper cleared" << cleared << "tiles";

    return m_board.applyGravityAndRefill();
}
