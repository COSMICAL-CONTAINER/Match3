# Match3 / 开心消消乐

<p align="center">
  <img src="https://github.com/COSMICAL-CONTAINER/Match3/blob/main/pic/rocket_prop.png?raw=true" width="360" alt="gameplay"/>
</p>

A classic match‑3 puzzle game made with **Qt 6 / QML**. Swap adjacent tiles to make lines of 3 or more, earn points, create powerful props, chain combos — and try to finish before you run out of steps.

一款用 **Qt 6 / QML** 制作的经典三消游戏。滑动交换相邻方块，凑成 3 个及以上即可消除得分、生成强力道具、触发连锁连击——在步数耗尽之前拿到尽可能高的分数。

---

## ✨ Features / 特性

- 🎮 **Classic match‑3 gameplay / 经典三消玩法** — swap, match, gravity drop, cascade combos / 交换、消除、重力下落、连锁连击
- 🚀 **Props / 道具系统** — rockets, bombs and super items, all activatable and chainable / 火箭、炸弹、超级道具，可激活且可连锁触发
- 💥 **6 combo props / 6 种组合道具** — rocket+rocket, bomb+bomb, bomb+rocket, super+rocket, super+bomb, super+super / 火箭+火箭、炸弹+炸弹、炸弹+火箭、超级+火箭、超级+炸弹、超级+超级
- 📊 **End‑of‑game statistics / 结算统计** — per‑color eliminations and prop activations shown at game over / 结算时展示各颜色消除数与道具激活数
- ⏸️ **Pause & initial step settings / 暂停与初始步数设置**
- 🎵 **BGM & sound effects / 背景音乐与音效**
- 🧩 **Refactored core engine / 重构的核心引擎** — game rules live in a plain C++ core (`core/`), UI in QML / 规则逻辑下沉到纯 C++ 核心（`core/`），界面由 QML 驱动

## 🕹️ How to play / 玩法

| 规则 Rule | 说明 Description |
|---|---|
| Match / 消除 | Swap two adjacent tiles; 3+ in a row/column clears them for 10 points per tile / 交换相邻方块，横竖 3 个以上消除，每格 10 分 |
| Combo / 连击 | Falling tiles can trigger new matches automatically / 方块下落后自动连锁消除 |
| 4 in a row / 四连 | Creates a **rocket** — clears a full row or column / 生成**火箭**，清除整行或整列 |
| 5 in a row / 五连 | Creates a **super item** — clears all tiles of one color / 生成**超级道具**，清除全场同色方块 |
| T / L shape / T 字或 L 字 | Creates a **bomb** — clears tiles within radius 2 / 生成**炸弹**，清除半径 2 范围内的方块 |
| Swap two props / 道具交换 | Two props swapped together trigger a powerful combo / 两个道具交换触发强力组合效果 |
| Steps / 步数 | Each effective move costs 1 step; game over at 0 / 每次有效移动扣 1 步，步数用完游戏结束 |

### Basic gameplay / 基础玩法演示

Swap adjacent tiles, match 3+, watch the cascade:

交换相邻方块，三消消除，方块下落连锁：

<p align="center">
  <img src="https://github.com/COSMICAL-CONTAINER/Match3/blob/main/pic/play.gif?raw=true" width="360" alt="basic gameplay"/>
</p>

### Rocket in action / 火箭道具实战

Double‑click a rocket prop to activate it — it clears the whole column and chains into any props it hits:

双击火箭道具即可激活——清除整列，命中其它道具还会连锁引爆：

<p align="center">
  <img src="https://github.com/COSMICAL-CONTAINER/Match3/blob/main/pic/gameplay.gif?raw=true" width="360" alt="rocket gameplay"/>
</p>

### Pause menu / 暂停菜单

Pause anytime to adjust the initial step count, restart or resume:

随时暂停，可修改初始步数、重新开始或继续游戏：

<p align="center">
  <img src="https://github.com/COSMICAL-CONTAINER/Match3/blob/main/pic/pause.gif?raw=true" width="360" alt="pause demo"/>
</p>

<p align="center">
  <img src="https://github.com/COSMICAL-CONTAINER/Match3/blob/main/pic/pause_panel.png?raw=true" width="360" alt="pause menu"/>
</p>

## 🏗️ Project structure / 项目结构

```
├── main.cpp                  # app entry, registers the game backend / 程序入口
├── core/
│   ├── GameEngine.{h,cpp}    # game rules: matches, gravity, props, combos, score / 规则引擎
│   ├── BoardModel.{h,cpp}    # board state as compact uint8 grid / 棋盘数据模型
│   ├── MatchFinder.{h,cpp}   # pure match-detection algorithms / 纯函数匹配检测
│   ├── GameBoardCompat.{h,cpp} # QObject facade exposed to QML / 暴露给 QML 的桥接层
│   └── Types.h               # shared types & prop constants / 共享类型与道具常量
├── qml/
│   ├── main.qml              # UI, HUD, overlays / 界面与覆盖层
│   └── AnimationManager.qml  # animation queue & prop effects / 动画队列与道具特效
└── image/, music/            # art & audio assets / 美术与音频资源
```

## 🔧 Build / 构建的方法

**Requirements / 环境要求**: Qt 6.5+ (QtQuick, QtMultimedia), qmake or Qt Creator

1. Open `Match3Demo.pro` in Qt Creator, or run / 在 Qt Creator 中打开 `Match3Demo.pro`，或执行：

```bash
qmake Match3Demo.pro
make            # Windows (MinGW): mingw32-make
```

2. Run the produced `Match3Demo` binary / 运行生成的 `Match3Demo`。

> On Windows with Qt's MinGW toolchain, make sure Qt's `mingw_64/bin` and the matching `Tools/mingw*/bin` are at the front of your `PATH` when building/running.
> Windows 下使用 Qt 自带 MinGW 工具链时，构建和运行请将 Qt 的 `mingw_64/bin` 与配套 `Tools/mingw*/bin` 放在 `PATH` 最前。

---

## License / 许可

See [LICENSE](LICENSE).
