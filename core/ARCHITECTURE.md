Match3 架构概要

核心概念：
- Match3Game: 高层控制器，负责关卡切换、保存/加载、与 UI 的事件总线。
- Level: 单个关卡实例，包含 Board、胜利条件与计数器；支持非矩形地图（通过 mask 或有效格点列表）。
- PlayerProps: 玩家携带的道具库存（锤子、行/列火箭、一次性交换等），与关卡解耦。
- Board: 棋盘核心，持有 Cell（BoardItemBase 的派生），负责交换、匹配查询、下落、补充等。
- BoardItemBase / Tile / TileProp / Obstacle: 棋盘单元层次结构，支持颜色方块、棋盘道具与障碍。
- MatchFinder: 纯算法模块，负责检测三消/四消/交叉等并给出匹配集合及生成道具建议。
- PropFactory: 根据 MatchFinder 的建议创建棋盘道具（TileProp），并处理组合逻辑。

关系（简要）：
- Match3Game -> 管理 -> Level
- Level -> 包含 -> Board
- Board -> 包含 -> Grid of BoardItemBase (Tile / TileProp / Obstacle)
- MatchFinder 使用 Board 状态来计算匹配并返回 MatchResult
- PropFactory 使用 MatchResult 在 Board 上创建 TileProp
- PlayerProps 为 Match3Game/Level 提供玩家可携带的消耗品接口

重构建议：
- 首先从将现有 GameBoard 中的纯算法（findMatches/findProps）提取到 MatchFinder 开始；Board 保持状态且提供访问接口。
- 在 Board 中使用 LastSwapInfo 显式传递交换上下文，避免隐式成员导致竞态。
- 使用 enum class 替代字符串常量（仅在 UI 层映射到资源名），便于单元测试与类型检查。

下一步我可以：
- 为这些头文件生成基础实现（.cpp），并把你现有的 GameBoard.cpp 中的函数逐步迁移为 Board/MatchFinder/PropFactory 的实现。
- 或者直接把 GameBoard 拆分出一个兼容层，逐步替换调用点以减少一次性改动风险。

