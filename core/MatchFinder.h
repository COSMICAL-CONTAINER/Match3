#pragma once

#include "BoardModel.h"
#include "Types.h"

namespace core {

// MatchFinder: 纯算法模块，给定 BoardModel 返回匹配集合与建议的道具生成信息
class MatchFinder {
public:
    // findMatches 返回 MatchResult，包含去重后的匹配格子和建议生成的道具
    static MatchResult findMatches(const BoardModel &board, const LastSwapInfo *lastSwap = nullptr);
};

} // namespace core

// Provide legacy global MatchFinder shim declaration for old code (implemented in MatchFinderBridge.cpp)
#include "MatchFinderBridge.h"
