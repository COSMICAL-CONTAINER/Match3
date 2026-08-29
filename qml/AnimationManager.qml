import QtQuick 2.15
import QtQml 2.15

Item {
    id: animManager
    property var boardView
    property var gameBoard
    property var tileMap: ({})
    property real cellSize: 50
    property bool busy: false

    // 棋盘行列数，提供给各种动画使用，避免未定义的 rows/cols 报错
    property int rows: boardView && boardView.rows ? boardView.rows : 8
    property int cols: boardView && boardView.columns ? boardView.columns : 8

    // 当前这一轮连锁的索引（仅用于调试输出）
    property int _cascadeIndex: 0

    // ===== 队列 =====
    property var swapQueue: []
    property var matchQueue: []
    property var dropQueue: []

    // === 从旧 GameBoard.h 同步的道具类型常量，property 名必须小写开头 ===
    readonly property int rocket_upDownType: 1
    readonly property int rocket_leftRightType: 2
    readonly property int bombType: 3
    readonly property int superItemType: 4

    readonly property int combo_rocketRocketType: 100
    readonly property int combo_bombBombType: 101
    readonly property int combo_bombRocketType: 102
    readonly property int combo_superBombType: 103
    readonly property int combo_superRocketType: 104
    readonly property int combo_superSuperType: 105

    // 统一：火箭特效资源（按实际资源方向：Rocket_1 对应 lighting_H，Rocket_2 对应 lighting_V）
    // 说明：isVertical==true(rocket_upDownType / Rocket_1) -> lighting_H
    //       isVertical==false(rocket_leftRightType / Rocket_2) -> lighting_V
    readonly property string rocketGifVertical: 'qrc:/image/Animated/lighting_H.gif'
    readonly property string rocketGifHorizontal: 'qrc:/image/Animated/lighting_V.gif'

    // ===== 连接游戏板信号（与旧版一致） =====
    onGameBoardChanged: {
        // 不再自动连接 swapAnimationRequested，避免形成闭环导致重复 finalizeSwap
        if (gameBoard) {
            try {
                if (typeof gameBoard.matchAnimationRequested !== 'undefined') {
                    gameBoard.matchAnimationRequested.connect(function(matchedTiles){ matchQueue.push(matchedTiles); if (!busy) runNext(); });
                }
                if (typeof gameBoard.dropAnimationRequested !== 'undefined') {
                    gameBoard.dropAnimationRequested.connect(function(dropPaths){ dropQueue.push(dropPaths); if (!busy) runNext(); });
                }
                if (typeof gameBoard.rollbackSwap !== 'undefined') {
                    gameBoard.rollbackSwap.connect(rollbackSwap);
                }
                if (typeof gameBoard.propEffectRequested !== 'undefined') {
                    gameBoard.propEffectRequested.connect(function(row, col, type, color){ handlePropEffectRequested(row, col, type, color); });
                }
            } catch(e) { console.log('AnimationManager connect error', e); }
            console.log('动画管理器已连接到游戏板');
        }
    }

    function findTile(row, col) { return tileMap[row + '_' + col] || null; }

    // 保留这些辅助函数，后续 schedulePropEffect 阶段化时会用到
    function markAllBombsAsMatched() {
        for (var r = 0; r < animManager.rows; ++r) {
            for (var c = 0; c < animManager.cols; ++c) {
                var t = findTile(r, c);
                if (!t) continue;
                if (t.color === "Bomb") t.isMatched = true;
            }
        }
    }

    function markAllRocketsAsMatched() {
        for (var r = 0; r < animManager.rows; ++r) {
            for (var c = 0; c < animManager.cols; ++c) {
                var t = findTile(r, c);
                if (!t) continue;
                if (t.color === "Rocket_1" || t.color === "Rocket_2") t.isMatched = true;
            }
        }
    }

    function collectAllMatchedTiles() {
        var list = [];
        for (var r = 0; r < animManager.rows; ++r) {
            for (var c = 0; c < animManager.cols; ++c) {
                var t = findTile(r, c);
                if (!t) continue;
                if (t.isMatched) list.push({ row: r, col: c });
            }
        }
        return list;
    }

    // 新增：连接道具效果请求信号的统一处理函数
    function handlePropEffectRequested(row, col, type, color) {
        console.log('handlePropEffectRequested:', row, col, 'type=', type, 'color=', color);

        // 特判：超级+炸弹 / 超级+火箭 这两种组合必须走 QML 两阶段表现，否则会“直接清图无动画”
        // （stage1 变形 -> 扫描全盘逐个触发动画 -> stage2 execute 结算并掉落）
        if (type === combo_superBombType) {
            if (animManager.runComboSuperBomb) { animManager.runComboSuperBomb(row, col); return; }
        }
        if (type === combo_superRocketType) {
            if (animManager.runComboSuperRocket) { animManager.runComboSuperRocket(row, col); return; }
        }

        if (!gameBoard || typeof gameBoard.previewPropChainFrom !== 'function') {
            console.log('previewPropChainFrom not available, fallback to old direct behavior');
            if (type === rocket_upDownType || type === rocket_leftRightType) {
                animManager.runRocketEffect(row, col, type);
            } else if (type === bombType) {
                animManager.runBombEffect(row, col, color);
            } else if (type === superItemType) {
                animManager.runSuperItemEffect(row, col, color);
            } else if (type === combo_rocketRocketType) {
                animManager.runComboRocketRocket(row, col);
            } else if (type === combo_bombBombType) {
                animManager.runComboBombBomb(row, col);
            } else if (type === combo_bombRocketType) {
                var rocketType = parseInt(color || '1');
                animManager.runComboBombRocket(row, col, rocketType);
            } else if (type === combo_superBombType) {
                animManager.runComboSuperBomb(row, col);
            } else if (type === combo_superRocketType) {
                animManager.runComboSuperRocket(row, col);
            } else if (type === combo_superSuperType) {
                // 没有单独的 QML 动画函数时，直接让后端执行（后端会发 dropAnimationRequested）
                if (typeof gameBoard.comboSuperSuperEffectTriggered === 'function') gameBoard.comboSuperSuperEffectTriggered(row, col);
            } else {
                console.log('handlePropEffectRequested: unknown type', type);
            }
            return;
        }

        // 1) 向后端要“连锁触发步骤列表”（不改棋盘）
        var steps = [];
        try {
            steps = gameBoard.previewPropChainFrom(row, col, type, color || "");
            console.log('previewPropChainFrom returned steps len =', (steps && steps.length) ? steps.length : 0);
        } catch(e) {
            console.log('previewPropChainFrom error', e);
            steps = [];
        }

        // 2) 逐步播放这些步骤的动画；全部播放完后，才真正调用后端 effectTriggered 修改棋盘
        playPropChainStepsSequentially(steps, function(){
            console.log('prop chain visuals done, now call backend to apply effect');
            if (type === rocket_upDownType || type === rocket_leftRightType) {
                if (typeof gameBoard.rocketEffectTriggered === 'function') gameBoard.rocketEffectTriggered(row, col, type);
            } else if (type === bombType) {
                if (typeof gameBoard.bombEffectTriggered === 'function') gameBoard.bombEffectTriggered(row, col);
            } else if (type === superItemType) {
                if (typeof gameBoard.superItemEffectTriggered === 'function') gameBoard.superItemEffectTriggered(row, col, color || "");
            } else if (type === combo_rocketRocketType) {
                if (typeof gameBoard.comboRocketRocketEffectTriggered === 'function') gameBoard.comboRocketRocketEffectTriggered(row, col);
            } else if (type === combo_bombBombType) {
                if (typeof gameBoard.comboBombBombEffectTriggered === 'function') gameBoard.comboBombBombEffectTriggered(row, col);
            } else if (type === combo_bombRocketType) {
                var rt = parseInt(color || '1');
                if (typeof gameBoard.comboBombRocketEffectTriggered === 'function') gameBoard.comboBombRocketEffectTriggered(row, col, rt);
            } else if (type === combo_superBombType) {
                // 这两个组合是“两阶段”：stage1 仅变形无掉落，stage2 才真正引爆并下落
                if (typeof gameBoard.executeComboSuperBomb === 'function') gameBoard.executeComboSuperBomb(row, col);
                else if (typeof gameBoard.comboSuperBombEffectTriggered === 'function') gameBoard.comboSuperBombEffectTriggered(row, col);
            } else if (type === combo_superRocketType) {
                if (typeof gameBoard.executeComboSuperRocket === 'function') gameBoard.executeComboSuperRocket(row, col);
                else if (typeof gameBoard.comboSuperRocketEffectTriggered === 'function') gameBoard.comboSuperRocketEffectTriggered(row, col);
            } else if (type === combo_superSuperType) {
                if (typeof gameBoard.comboSuperSuperEffectTriggered === 'function') gameBoard.comboSuperSuperEffectTriggered(row, col);
            }
        });
    }

    // 新增：道具链式触发动画调度（方案B）
    // steps: [{row,col,type,color}...]
    function playPropChainStepsSequentially(steps, finalCall) {
        if (!steps || steps.length === 0) {
            if (typeof finalCall === 'function') finalCall();
            return;
        }

        var idx = 0;
        function playNext() {
            if (idx >= steps.length) {
                if (typeof finalCall === 'function') finalCall();
                return;
            }
            var s = steps[idx++];
            if (!s) { playNext(); return; }

            var t = parseInt(s.type);
            var r = parseInt(s.row);
            var c = parseInt(s.col);
            var ck = (s.color !== undefined && s.color !== null) ? ("" + s.color) : "";

            // visual-only 顺序播放
            if (t === rocket_upDownType || t === rocket_leftRightType) {
                runRocketEffectVisual(r, c, t, playNext);
            } else if (t === bombType) {
                runBombEffectVisual(r, c, ck, playNext);
            } else if (t === superItemType) {
                runSuperItemEffectVisual(r, c, ck, playNext);
            } else if (t === combo_rocketRocketType) {
                runComboRocketRocketVisual(r, c, playNext);
            } else if (t === combo_bombBombType) {
                runComboBombBombVisual(r, c, playNext);
            } else if (t === combo_bombRocketType) {
                var rt = parseInt(ck || '1');
                runComboBombRocketVisual(r, c, rt, playNext);
            } else {
                playNext();
            }
        }

        playNext();
    }

    // ===== 下面是“只播放动画，不回调后端”的版本 =====

    function runComboRocketRocketVisual(row, col, done) {
        if (!boardView) { if (done) done(); return; }

        // 十字火箭线：按当前资源约定
        showLineGifAtRowCol(row, col, true,
                            animManager.rocketGifVertical,
                            1.08, 1.8, 700);
        showLineGifAtRowCol(row, col, false,
                            animManager.rocketGifHorizontal,
                            1.08, 1.8, 700);

        var tilesToClear = [];
        var rows2 = boardView.rows || 8;
        var cols2 = boardView.columns || 8;
        for (var r3 = 0; r3 < rows2; ++r3) {
            var tV = findTile(r3, col);
            if (tV) tilesToClear.push(tV);
        }
        for (var c3 = 0; c3 < cols2; ++c3) {
            var tH = findTile(row, c3);
            if (tH) tilesToClear.push(tH);
        }

        var uniq = [];
        var keySet = {};
        tilesToClear.forEach(function(t4){
            var key = t4.row + '_' + t4.col;
            if (!keySet[key]) { keySet[key] = true; uniq.push(t4); }
        });

        if (uniq.length === 0) { if (done) done(); return; }

        var pending = uniq.length;
        uniq.forEach(function(tile){
            var anim = Qt.createQmlObject(
                'import QtQuick 2.15; SequentialAnimation {'
              + '  ParallelAnimation {'
              + '    NumberAnimation { property: "scale"; to: 0.80; duration: 200; easing.type: Easing.InQuad }'
              + '    NumberAnimation { property: "opacity"; to: 0; duration: 200; easing.type: Easing.InQuad }'
              + '  }'
              + '}',
                animManager
            );
            anim.animations[0].animations[0].target = tile;
            anim.animations[0].animations[1].target = tile;
            anim.onFinished.connect(function(){ pending--; if (pending === 0 && done) done(); });
            anim.start();
        });
    }

    function runRocketEffectVisual(row, col, type, done) {
        var isVertical = (type === 1);
        showLineGifAtRowCol(row, col, isVertical,
                            isVertical ? animManager.rocketGifVertical : animManager.rocketGifHorizontal,
                            1.08, 1.6, 520);
        var tilesToClear = [];
        var rows = boardView.rows || 8;
        var cols = boardView.columns || 8;
        if (isVertical) {
            for (var r = 0; r < rows; ++r) { var t = findTile(r, col); if (t) tilesToClear.push(t); }
        } else {
            for (var c = 0; c < cols; ++c) { var t2 = findTile(row, c); if (t2) tilesToClear.push(t2); }
        }
        if (tilesToClear.length === 0) { if (done) done(); return; }
        var pending = tilesToClear.length;
        tilesToClear.forEach(function(tile){
            // 修复：原先 duration:200, easing... 中间逗号会导致 inline QML 语法错误（Expected token ','）
            // 观感优化：不再 scale->0，改为轻微缩小+淡出
            var anim = Qt.createQmlObject(
                        'import QtQuick 2.15; SequentialAnimation {'
                      + '  ParallelAnimation {'
                      + '    NumberAnimation { property: "scale"; from: 1; to: 0.85; duration: 160; easing.type: Easing.InQuad }'
                      + '    NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 160; easing.type: Easing.InQuad }'
                      + '  }'
                      + '}', animManager);
            anim.animations[0].animations[0].target = tile;
            anim.animations[0].animations[1].target = tile;
            anim.onFinished.connect(function(){ pending--; if (pending===0 && done) done(); });
            anim.start();
        });
    }

    function runBombEffectVisual(row, col, color, done) {
        showGifAt(row, col, 'qrc:/image/Animated/smallbomb.gif', 520, { tileSpan: 5.5 });
        var affected = [];
        var radius = 2;
        for (var r = row - radius; r <= row + radius; ++r) {
            for (var c = col - radius; c <= col + radius; ++c) {
                if (r >= 0 && r < animManager.rows && c >= 0 && c < animManager.cols) {
                    var dr = r - row;
                    var dc = c - col;
                    if (dr*dr + dc*dc <= radius*radius) {
                        var t = findTile(r, c);
                        if (t) affected.push(t);
                    }
                }
            }
        }
        if (affected.length === 0) { if (done) done(); return; }
        var pending = affected.length;
        affected.forEach(function(tile){
            // 观感优化：不要“变大”，改为轻微缩小+淡出
            var anim = Qt.createQmlObject(
                        'import QtQuick 2.15; SequentialAnimation {'
                      + '  ParallelAnimation {'
                      + '    NumberAnimation { property:"scale"; from:1; to:0.85; duration:120; easing.type: Easing.InQuad }'
                      + '    NumberAnimation { property:"opacity"; from:1; to:0; duration:120; easing.type: Easing.InQuad }'
                      + '  }'
                      + '}', animManager);
            anim.animations[0].animations[0].target = tile;
            anim.animations[0].animations[1].target = tile;
            anim.onFinished.connect(function(){ pending--; if (pending===0 && done) done(); });
            anim.start();
        });
    }

    function runSuperItemEffectVisual(row, col, color, done) {
        // 简化：只做中心圈 + 全局淡出（不走后端）
        var centerTile = findTile(row, col);
        var cell = boardView.cellSize || animManager.cellSize;
        var localCx = centerTile ? (centerTile.x + centerTile.width/2) : (boardView.offsetX + col * cell + cell/2);
        var localCy = centerTile ? (centerTile.y + centerTile.height/2) : (boardView.offsetY + row * cell + cell/2);
        var mappedC = boardView.mapToItem(animManager, localCx, localCy);
        var maxDiam = Math.max(animManager.rows * animManager.cellSize, animManager.cols * animManager.cellSize) * 0.9;
        var cGif = Qt.createQmlObject('import QtQuick 2.15; AnimatedImage { x: ' + (mappedC.x - maxDiam/2) + '; y: ' + (mappedC.y - maxDiam/2) + '; width: ' + maxDiam + '; height: ' + maxDiam + '; z: 9999; source: "qrc:/image/Animated/lighting_circle.gif"; playing: true; cache: false; fillMode: Image.PreserveAspectFit; smooth: true }', animManager);
        var killer = Qt.createQmlObject('import QtQuick 2.15; Timer { interval: 520; repeat: false }', animManager);
        killer.triggered.connect(function(){ try { if (cGif) cGif.destroy(); } catch(e){} });
        killer.start();

        // 全盘轻微闪烁
        var tiles = [];
        for (var r = 0; r < animManager.rows; ++r)
            for (var c = 0; c < animManager.cols; ++c) {
                var t = findTile(r, c);
                if (t) tiles.push(t);
            }
        if (tiles.length === 0) { if (done) done(); return; }
        var pending = tiles.length;
        tiles.forEach(function(t){
            var anim = Qt.createQmlObject('import QtQuick 2.15; SequentialAnimation { ParallelAnimation { NumberAnimation { property:"opacity"; to:0.6; duration:120 } } ParallelAnimation { NumberAnimation { property:"opacity"; to:1.0; duration:120 } } }', animManager);
            anim.animations[0].animations[0].target = t;
            anim.animations[1].animations[0].target = t;
            anim.onFinished.connect(function(){ pending--; if (pending===0 && done) done(); });
            anim.start();
        });
    }

    // 补齐：原代码里会调用 runSuperItemEffect（播放完动画后回调后端）
    function runSuperItemEffect(row, col, color) {
        runSuperItemEffectVisual(row, col, color, function(){
            if (gameBoard && typeof gameBoard.superItemEffectTriggered === 'function') {
                gameBoard.superItemEffectTriggered(row, col, color || "");
            }
        });
    }

    function runComboBombBombVisual(row, col, done) {
        showGifAt(row, col, 'qrc:/image/Animated/hugebomb.gif', 650, { tileSpan: 7.5 });
        var affected = [];
        var radius = 4;
        for (var r = row - radius; r <= row + radius; ++r) {
            for (var c = col - radius; c <= col + radius; ++c) {
                if (r >= 0 && r < animManager.rows && c >= 0 && c < animManager.cols) {
                    var dr = r - row;
                    var dc = c - col;
                    if (dr*dr + dc*dc <= radius*radius) {
                        var t = findTile(r, c);
                        if (t) affected.push(t);
                    }
                }
            }
        }
        if (affected.length === 0) { if (done) done(); return; }
        var pending = affected.length;
        affected.forEach(function(tile){
            // 观感优化：不要“变大”，改为轻微缩小+淡出
            var anim = Qt.createQmlObject(
                        'import QtQuick 2.15; SequentialAnimation {'
                      + '  ParallelAnimation {'
                      + '    NumberAnimation { property:"scale"; from:1; to:0.80; duration:140; easing.type: Easing.InQuad }'
                      + '    NumberAnimation { property:"opacity"; from:1; to:0.0; duration:140; easing.type: Easing.InQuad }'
                      + '  }'
                      + '}', animManager);
            anim.animations[0].animations[0].target = tile;
            anim.animations[0].animations[1].target = tile;
            anim.onFinished.connect(function(){ pending--; if (pending===0 && done) done(); });
            anim.start();
        });
    }

    function runComboBombRocketVisual(row, col, rocketType, done) {
        var isVertical = (rocketType === 1);
        showLineGifAtRowCol(row, col, isVertical,
                            isVertical ? animManager.rocketGifVertical : animManager.rocketGifHorizontal,
                            1.02, 1.6, 520);

        var rows = boardView.rows || animManager.rows;
        var cols = boardView.columns || animManager.cols;
        var affected = [];
        if (isVertical) {
            for (var c = col - 1; c <= col + 1; ++c) {
                if (c < 0 || c >= cols) continue;
                for (var r = 0; r < rows; ++r) { var tCol = findTile(r, c); if (tCol) affected.push(tCol); }
            }
        } else {
            for (var r2 = row - 1; r2 <= row + 1; ++r2) {
                if (r2 < 0 || r2 >= rows) continue;
                for (var c2 = 0; c2 < cols; ++c2) { var tRow = findTile(r2, c2); if (tRow) affected.push(tRow); }
            }
        }
        if (affected.length === 0) { if (done) done(); return; }

        var pending = affected.length;
        affected.forEach(function(tile){
            // 注意：ParallelAnimation 本身没有 target/property；必须设置到里面的 NumberAnimation。
            var anim = Qt.createQmlObject(
                'import QtQuick 2.15; SequentialAnimation { '
              + '  ParallelAnimation { '
              + '    NumberAnimation { property: "scale"; from: 1; to: 0.85; duration: 160; easing.type: Easing.InQuad }'
              + '    NumberAnimation { property: "opacity"; from: 1; to: 0.0; duration: 160; easing.type: Easing.InQuad }'
              + '  }'
              + '}',
                animManager
            );

            // 结构：SequentialAnimation.animations[0] = ParallelAnimation
            // ParallelAnimation.animations[0/1] = NumberAnimation
            anim.animations[0].animations[0].target = tile;
            anim.animations[0].animations[1].target = tile;

            anim.onFinished.connect(function(){
                // visual-only：不要让 tile 永久缩放/透明，避免你看到的“点到一个格子但地图上不显示(空洞)”
                try { tile.scale = 1.0; } catch(e) {}
                try { tile.opacity = 1.0; } catch(e) {}

                pending--;
                if (pending === 0 && done) done();
            });
            anim.start();
        });
    }

    // ===== 队列入列函数 =====
    function enqueueSwap(r1,c1,r2,c2){
        // 仅供外部 UI（点击/拖动逻辑）显式调用，不再作为 swapAnimationRequested 的槽
        swapQueue.push({r1:r1,c1:c1,r2:r2,c2:c2});
        if (!busy)
            runNext();
    }
    function enqueueMatches(matchedTiles){ matchQueue.push(matchedTiles); if (!busy) runNext(); }
    function enqueueDrops(dropPaths){ dropQueue.push(dropPaths); if (!busy) runNext(); }

    // ===== 队列驱动 =====
    function runNext(){
        if (busy) return;

        if (swapQueue.length > 0) {
            busy = true;
            _cascadeIndex = 0; // 新交换开始，重置连锁计数
            var s = swapQueue.shift();
            runSwap(s.r1, s.c1, s.r2, s.c2);
            return;
        }
        if (matchQueue.length > 0) {
            busy = true;
            var m = matchQueue.shift();
            runMatches(m);
            return;
        }
        if (dropQueue.length > 0) {
            busy = true;
            _cascadeIndex++;
            var d = dropQueue.shift();
            console.log('runNext: starting cascade', _cascadeIndex, 'with dropPaths length =', (d && typeof d.length === 'number') ? d.length : 'n/a');
            runDrops(d);
            return;
        }
        busy = false;
    }

    // ===== 交换动画（完成后归零偏移并调用 finalizeSwap）=====
    function runSwap(r1,c1,r2,c2){
        var t1=findTile(r1,c1), t2=findTile(r2,c2);
        if(!t1||!t2){ busy=false; runNext(); return; }
        var dx=(c2-c1)*cellSize, dy=(r2-r1)*cellSize;

        var anim1=Qt.createQmlObject('import QtQuick 2.15; ParallelAnimation { NumberAnimation { property: "offsetX"; duration: 200 } NumberAnimation { property: "offsetY"; duration: 200 } }', animManager);
        anim1.animations[0].target=t1; anim1.animations[0].from=t1.offsetX||0; anim1.animations[0].to=dx;
        anim1.animations[1].target=t1; anim1.animations[1].from=t1.offsetY||0; anim1.animations[1].to=dy;

        var anim2=Qt.createQmlObject('import QtQuick 2.15; ParallelAnimation { NumberAnimation { property: "offsetX"; duration: 200 } NumberAnimation { property: "offsetY"; duration: 200 } }', animManager);
        anim2.animations[0].target=t2; anim2.animations[0].from=t2.offsetX||0; anim2.animations[0].to=-dx;
        anim2.animations[1].target=t2; anim2.animations[1].from=t2.offsetY||0; anim2.animations[1].to=-dy;

        var finished = 0;
        function onOneFinished(){
            finished++;
            if (finished === 2) {
                // 交换动画结束，归零偏移
                try { t1.offsetX = 0; t1.offsetY = 0; } catch(e){}
                try { t2.offsetX = 0; t2.offsetY = 0; } catch(e){}

                // 仅在这里调用一次 finalizeSwap，由后端决定是否回滚或发出 matchAnimationRequested
                try {
                    if (gameBoard && typeof gameBoard.finalizeSwap === 'function') {
                        gameBoard.finalizeSwap(r1,c1,r2,c2, false);
                    }
                } catch(e) { console.log('runSwap finalizeSwap error', e); }

                busy = false;
                runNext();
            }
        }

        anim1.onFinished.connect(onOneFinished);
        anim2.onFinished.connect(onOneFinished);
        anim1.start();
        anim2.start();
    }

    function rollbackSwap(r1,c1,r2,c2){
        if(busy) return;
        busy=true;
        var t1=findTile(r1,c1), t2=findTile(r2,c2);
        if(!t1||!t2){ busy=false; return; }
        [t1,t2].forEach(function(t){
            var rollback=Qt.createQmlObject('import QtQuick 2.15; ParallelAnimation { NumberAnimation { property: "offsetX"; to:0; duration:200 } NumberAnimation { property: "offsetY"; to:0; duration:200 } }', animManager);
            rollback.animations[0].target=t; rollback.animations[1].target=t; rollback.start();
        });
        t1.offsetX=0; t1.offsetY=0; t2.offsetX=0; t2.offsetY=0;
        busy=false;
    }

    // ===== 匹配动画：先播完，再调用后端 processMatches 计算真正删除和掉落 =====
    function runMatches(matchedTiles){
        try { console.log('runMatches called, raw payload =', JSON.stringify(matchedTiles)); } catch(e) { console.log('runMatches called'); }

        // 先把棋盘刷新成“交换后的真实状态”，再在正确的颜色上做闪烁
        refreshBoardColors();

        if (!matchedTiles || !Array.isArray(matchedTiles) || matchedTiles.length === 0) {
            busy = false;
            runNext();
            return;
        }

        // 标准化为 {row,col}
        var list = [];
        for (var i=0;i<matchedTiles.length;i++){
            var m = matchedTiles[i];
            if (!m) continue;
            var r = null;
            var c = null;
            if (m.row !== undefined || m.col !== undefined) {
                r = m.row;
                c = m.col;
            } else if (Array.isArray(m) && m.length >= 2) {
                r = m[0];
                c = m[1];
            } else if (m.x !== undefined || m.y !== undefined) {
                r = m.x;
                c = m.y;
            }
            if (r !== null && c !== null)
                list.push({row:r,col:c});
        }

        if (list.length === 0) {
            console.log('runMatches: normalized list empty, skip processMatches');
            busy = false;
            runNext();
            return;
        }

        var pending = 0;
        for (var j=0;j<list.length;j++){
            var pos = list[j];
            var tile = findTile(pos.row, pos.col);
            if (!tile) continue;

            // 匹配动画：放大 + 渐隐，整体时间控制在 ~220ms，减少空洞停留时间
            var anim = Qt.createQmlObject('import QtQuick 2.15; SequentialAnimation { ParallelAnimation { NumberAnimation { property:"scale"; from:1; to:1.2; duration:180; easing.type: Easing.OutQuad } NumberAnimation { property:"opacity"; from:1; to:0.0; duration:180; easing.type: Easing.InQuad } } }', animManager);
            anim.animations[0].animations[0].target = tile;
            anim.animations[0].animations[1].target = tile;

            pending++;
            (function(t, r0, c0, a){
                a.onFinished.connect(function(){
                    // 匹配结束后，将该格子完全置空（透明）并恢复 scale，以便后续看到一个“空洞”
                    try { t.scale = 1.0; } catch(e){}
                    try { t.opacity = 0.0; } catch(e){}
                    try { t.tileColor = 'transparent'; } catch(e){}
                    try { t.isMatched = true; } catch(e){}

                    pending--;
                    if (pending === 0) {
                        try {
                            if (animManager.gameBoard && typeof animManager.gameBoard.processMatches === 'function') {
                                console.log('runMatches: all visuals done, calling gameBoard.processMatches()');
                                animManager.gameBoard.processMatches();
                            }
                        } catch(e) { console.log('runMatches processMatches error', e); }
                        animManager.busy = false;
                        animManager.runNext();
                    }
                });
                a.start();
            })(tile, pos.row, pos.col, anim);
        }

        if (pending === 0) {
            console.log('runMatches: pending == 0, no valid tiles to animate');
            animManager.busy = false;
            animManager.runNext();
        }
    }

    // ===== 掉落动画（结束后调用 commitDrop）=====
    function refreshBoardColors() {
        if (!gameBoard) return;
        console.log('refreshBoardColors: syncing tileMap from gameBoard');
        for (var key in tileMap) {
            var parts = key.split('_');
            var r = parseInt(parts[0]); var c = parseInt(parts[1]);
            var tile = tileMap[key];
            if (!tile) continue;
            try {
                var col = gameBoard.getTileColor(r, c);
                if (tile.isMatched) {
                    // 刚刚消除的位置保持透明，等掉落动画来填补
                    tile.tileColor = 'transparent';
                } else {
                    tile.tileColor = col;
                }
            } catch(e) {
                console.log('refreshBoardColors: error getTileColor at', r, c, e);
            }
        }
    }

    // 新增：掉落完成后，清理所有 isMatched 标记，并用后端颜色恢复这些格子的显示
    function clearMatchedFlags() {
        if (!gameBoard) return;
        console.log('clearMatchedFlags: reset isMatched & colors');
        for (var key in tileMap) {
            var tile = tileMap[key];
            if (!tile || !tile.isMatched)
                continue;
            tile.isMatched = false;
            try {
                var parts = key.split('_');
                var r = parseInt(parts[0]);
                var c = parseInt(parts[1]);
                tile.tileColor = gameBoard.getTileColor(r, c);
                tile.opacity = 1.0;
                if (typeof tile.scale !== 'undefined') tile.scale = 1.0;
            } catch(e) {
                console.log('clearMatchedFlags: error at key', key, e);
            }
        }
    }

    function runDrops(dropPaths){
        try {
            console.log('runDrops(cascade', _cascadeIndex, '): typeof payload =', typeof dropPaths,
                        ', hasLength =', (dropPaths && typeof dropPaths.length !== 'undefined'),
                        ', Array.isArray =', Array.isArray(dropPaths));
        } catch(e) {}

        if (!dropPaths) {
            console.log('runDrops: null/undefined payload, skip');
            busy = false;
            runNext();
            return;
        }

        var length = 0;
        try {
            if (typeof dropPaths.length === 'number')
                length = dropPaths.length;
        } catch(e) {
            length = 0;
        }

        console.log('runDrops(cascade', _cascadeIndex, '): computed length =', length);

        if (length === 0) {
            console.log('runDrops: empty list, skip commitDrop (no more cascades)');
            busy = false;
            runNext();
            return;
        }

        // 兼容字段名：后端传 fromRow/fromCol/toRow/toCol（QVariantMap），旧逻辑使用 fromR/fromC/toR/toC
        function getDropField(d, shortKey, longKey) {
            if (!d) return undefined;
            if (d[shortKey] !== undefined) return d[shortKey];
            return d[longKey];
        }

        var affectedCols = {};
        // 每列的最大“下落行数”（用于对齐同一列中不同 tile 的速度/时长）
        var colMaxDropRows = {};
        for (var ci = 0; ci < length; ++ci) {
            var cd = dropPaths[ci];
            if (!cd || Array.isArray(cd)) continue;
            var toR = getDropField(cd, 'toR', 'toRow');
            var toC = getDropField(cd, 'toC', 'toCol');
            var fromR = getDropField(cd, 'fromR', 'fromRow');
            // var fromC = getDropField(cd, 'fromC', 'fromCol'); // 当前 affectedCols 逻辑只需要 fromR
            if (typeof toR === 'undefined' || typeof toC === 'undefined') continue;
            if (fromR === undefined || fromR === null || fromR < 0 || fromR !== toR) {
                affectedCols[toC] = true;
            }

            // 计算该条路径的下落距离（行数），统计到列最大值中
            var isSpawn2 = (cd.isNew === true) || (fromR === undefined || fromR === null || fromR < 0);
            var distRows2 = 1;
            if (isSpawn2) {
                var startRow2 = fromR;
                if (startRow2 === undefined || startRow2 === null || startRow2 < 0)
                    startRow2 = toR - 1;
                distRows2 = startRow2 - toR;
                if (distRows2 < 1) distRows2 = 1;
            } else {
                distRows2 = Math.abs(parseInt(fromR) - parseInt(toR));
                if (distRows2 < 1) distRows2 = 1;
            }
            var cKey2 = '' + toC;
            if (colMaxDropRows[cKey2] === undefined || distRows2 > colMaxDropRows[cKey2])
                colMaxDropRows[cKey2] = distRows2;
        }

        var byTarget = {};
        for (var i = 0; i < length; ++i) {
            var dd = dropPaths[i];
            if (!dd || Array.isArray(dd)) continue;
            var tR = getDropField(dd, 'toR', 'toRow');
            var tC = getDropField(dd, 'toC', 'toCol');
            if (typeof tR === 'undefined' || typeof tC === 'undefined') continue;
            if (!affectedCols[tC])
                continue;
            var key = tR + '_' + tC;
            var ddFromR = getDropField(dd, 'fromR', 'fromRow');
            if (!byTarget[key] || (ddFromR !== undefined && getDropField(byTarget[key], 'fromR', 'fromRow') !== undefined && ddFromR > getDropField(byTarget[key], 'fromR', 'fromRow'))) {
                byTarget[key] = dd;
            }
        }
        var uniqueDrops = [];
        for (var k in byTarget) uniqueDrops.push(byTarget[k]);
        console.log('runDrops(cascade', _cascadeIndex, '): unique target count =', uniqueDrops.length, 'from original length =', length, 'affectedCols =', Object.keys(affectedCols));

        var startDelayTimer = Qt.createQmlObject('import QtQuick 2.15; Timer { interval: 80; repeat: false }', animManager);
        startDelayTimer.triggered.connect(function(){
            var pending = 0;
            var commitCalled = false;

            console.log('runDrops(cascade', _cascadeIndex, '): start anim, unique length =', uniqueDrops.length);

            for (var idx = 0; idx < uniqueDrops.length; ++idx) {
                var d = uniqueDrops[idx];
                if (!d) {
                    console.log('runDrops: idx', idx, 'is null, skip');
                    continue;
                }

                var fromR = getDropField(d, 'fromR', 'fromRow');
                var fromC = getDropField(d, 'fromC', 'fromCol');
                var toR   = getDropField(d, 'toR', 'toRow');
                var toC   = getDropField(d, 'toC', 'toCol');
                var isNew = d.isNew;
                var color = d.color;
                console.log('runDrops obj[', idx, ']: from(', fromR, ',', fromC, ') to(', toR, ',', toC, ') isNew=', isNew, ' color=', color);

                if (typeof toR === 'undefined' || typeof toC === 'undefined') {
                    console.log('runDrops: idx', idx, 'missing toR/toC, skip');
                    continue;
                }

                var tileTo = findTile(toR, toC);
                if (!tileTo) {
                    console.log('runDrops: cannot find tileTo at', toR, toC);
                    continue;
                }

                // 核心修复：不要无差别把 tileTo.opacity=1。
                // 只有“生成的新格子 / 填洞的位置（原先 isMatched=true）”才需要立刻恢复显示。
                // 否则会出现你看到的：上面一行(abc)在掉落时从完全透明->可见，像是被下面的匹配动画影响。
                var isSpawn = (isNew === true) || (fromR === undefined || fromR === null || fromR < 0);
                var isFillingHole = false;
                try { isFillingHole = (tileTo.isMatched === true) || (tileTo.opacity === 0) || (tileTo.tileColor === 'transparent'); } catch(e) { isFillingHole = false; }

                if (isSpawn || isFillingHole) {
                    try { tileTo.opacity = 1.0; } catch(e) {}
                    try { if (typeof tileTo.scale !== 'undefined') tileTo.scale = 1.0; } catch(e) {}
                    try { tileTo.isMatched = false; } catch(e) {}
                } else {
                    // 非填洞格子：只确保 scale 正常，opacity 保持现状（通常应为 1）
                    try { if (typeof tileTo.scale !== 'undefined') tileTo.scale = 1.0; } catch(e) {}
                }

                // 位移策略
                // - 已有格子(fromR>=0): 从原来的行 fromR 落到目标行 toR，因此 offsetY=(fromR-toR)*cellSize
                // - 新生成格子: 从棋盘上方落下
                var startOffset = 0;
                var distanceRows = 1;

                if (isSpawn) {
                    // 新生成：从棋盘上方进入。这里不要用“对齐列最大高度”的方式，
                    // 否则会让已有方块也被强行拉到上方开始落，视觉上就像“整列瞬移/回弹”。
                    distanceRows = Math.max(1, (animManager.rows - 1) - parseInt(toR));
                    startOffset  = -distanceRows * cellSize;
                } else {
                    var fr = parseInt(fromR);
                    var tr = parseInt(toR);
                    distanceRows = Math.max(1, Math.abs(fr - tr));
                    startOffset = (fr - tr) * cellSize;
                }

                // 关键修复：不要再把 startOffset 强制改成 -alignedRows*cellSize。
                // 正确的体验是：
                // 1) abc 在 bbb 消失后仍停在原地；
                // 2) 旧块从原位置开始下落；
                // 3) 新生成块一开始就在棋盘外(第一行之上)，最后跟随一起落下。

                // 统一速度：同列尽量保持同一落速（像素/毫秒），但起点必须真实
                var travelPx = Math.abs(startOffset);
                tileTo.offsetY = startOffset;

                try {
                    if (color !== undefined && color !== null && color !== 0) {
                        tileTo.tileColor = ('' + color);
                    }
                } catch(e) {
                    console.log('runDrops: error setting tileTo.color at', toR, toC, e);
                }

                var pxPerMs = 0.60; // 提高一点速度，减少“空洞停留”
                var totalDur = Math.max(160, Math.floor(travelPx / pxPerMs));
                if (totalDur > 520) totalDur = 520;
                var fallDur   = totalDur * 0.75;
                var squashDur = totalDur * 0.10;
                var settleDur = totalDur * 0.15;

                var seq = Qt.createQmlObject(
                    'import QtQuick 2.15; SequentialAnimation { running: true; PropertyAnimation { id: a1 } PropertyAnimation { id: a2 } PropertyAnimation { id: a3 } }',
                    tileTo
                );

                seq.animations[0].target = tileTo;
                seq.animations[0].property = 'offsetY';
                seq.animations[0].from = startOffset;
                seq.animations[0].to = 0;
                seq.animations[0].duration = fallDur;
                seq.animations[0].easing.type = Easing.InQuad;

                seq.animations[1].target = tileTo;
                seq.animations[1].property = 'scale';
                seq.animations[1].from = 1.0;
                seq.animations[1].to = 0.9;
                seq.animations[1].duration = squashDur;
                seq.animations[1].easing.type = Easing.OutQuad;

                seq.animations[2].target = tileTo;
                seq.animations[2].property = 'scale';
                seq.animations[2].from = 0.9;
                seq.animations[2].to = 1.0;
                seq.animations[2].duration = settleDur;
                seq.animations[2].easing.type = Easing.OutBounce;

                pending++;
                seq.finished.connect((function(anim, t){
                    return function(){
                        t.offsetY = 0;
                        t.scale = 1.0;
                        pending--;
                        console.log('runDrops(cascade', _cascadeIndex, '): one anim finished, pending =', pending);
                        if (pending === 0 && !commitCalled) {
                            commitCalled = true;
                            console.log('runDrops(cascade', _cascadeIndex, '): all animations finished, calling gameBoard.commitDrop()');
                            try {
                                clearMatchedFlags();
                                gameBoard.commitDrop();
                            } catch(e) { console.log('runDrops: commitDrop error', e); }
                            busy = false;
                            runNext();
                        }
                    };
                })(seq, tileTo));
            }

            if (pending === 0 && !commitCalled) {
                console.log('runDrops(cascade', _cascadeIndex, '): no valid animations created, still call commitDrop() to advance logic');
                try {
                    clearMatchedFlags();
                    gameBoard.commitDrop();
                } catch(e) { console.log('runDrops: commitDrop error (no anim)', e); }
                busy = false;
                runNext();
            }
        });
        startDelayTimer.start();
    }

    // 通用：在指定行列播放 GIF，一段时间后淡出销毁
    function showGifAt(row, col, source, durationMs, opts) {
        if (!boardView) return null;
        var cell = boardView.cellSize || animManager.cellSize;
        var rows = boardView.rows || 8;
        var cols = boardView.columns || 8;
        var tileSpan = (opts && opts.tileSpan) ? opts.tileSpan : 5.5; // 默认小炸弹约 5.5 格直径
        var w = tileSpan * cell;
        var h = tileSpan * cell;
        var tile = findTile(row, col);
        var localX = tile ? (tile.x + tile.width/2) : (boardView.offsetX + col * cell + cell/2);
        var localY = tile ? (tile.y + tile.height/2) : (boardView.offsetY + row * cell + cell/2);
        if (opts && opts.offsetY) localY += opts.offsetY;
        var mapped = boardView.mapToItem(animManager, localX, localY);

        // 关键修复：source 必须作为合法的 QML 字符串字面量传入
        var srcLiteral = JSON.stringify(String(source || ""));
        var gifQml = 'import QtQuick 2.15; AnimatedImage {'
                   + ' x: ' + (mapped.x - w/2)
                   + '; y: ' + (mapped.y - h/2)
                   + '; width: ' + w
                   + '; height: ' + h
                   + '; z: 9999'
                   + '; source: ' + srcLiteral
                   + '; playing: true; cache: false; fillMode: Image.PreserveAspectFit; smooth: true'
                   + ' }';
        var gif = Qt.createQmlObject(gifQml, animManager);

        console.log('showGifAt: created gif', source, 'at', row, col, 'size', w, h, 'duration', durationMs);
        var killer = Qt.createQmlObject('import QtQuick 2.15; Timer { interval: ' + (durationMs || 700) + '; repeat: false }', animManager);
        killer.triggered.connect(function(){ console.log('showGifAt: killer triggered for', source, row, col); if(gif){ try { gif.playing = false; gif.visible = false; gif.source = ""; } catch(e){} try { gif.destroy(); } catch(e){} } });
        killer.start();
        // 提前稍微停止播放，避免多余残影
        var stopper = Qt.createQmlObject('import QtQuick 2.15; Timer { interval: ' + Math.max(1, (durationMs || 700) - 80) + '; repeat: false }', animManager);
        stopper.triggered.connect(function(){ console.log('showGifAt: stopper triggered for', source, row, col); try { if(gif){ gif.playing = false; gif.visible = false; gif.source = ""; if (typeof gif.frame !== 'undefined') gif.frame = 0; } } catch(e){} });
        stopper.start();
        return gif;
    }

    // 统一自适应：基于棋盘尺寸生成线性 GIF（行/列）
    function showLineGifAtRowCol(row, col, isVertical, source, lengthScale, thicknessTiles, durationMs) {
        if (!boardView) return null;
        var cell = boardView.cellSize || animManager.cellSize;
        var rows = boardView.rows || 8;
        var cols = boardView.columns || 8;
        var boardCenterX = boardView.offsetX + cols * cell / 2;
        var boardCenterY = boardView.offsetY + rows * cell / 2;
        var length = (isVertical ? (rows * cell) : (cols * cell)) * (lengthScale || 1.0);
        var thickness = Math.max((thicknessTiles || 1) * cell, cell * 1.8);
        var localX = isVertical ? (boardView.offsetX + col * cell + cell/2) : boardCenterX;
        var localY = isVertical ? boardCenterY : (boardView.offsetY + row * cell + cell/2);
        var mapped = boardView.mapToItem(animManager, localX, localY);
        var w = isVertical ? thickness : length;
        var h = isVertical ? length : thickness;

        // 关键修复：source 可能包含 ':' '/'，必须作为合法 QML 字符串字面量传入
        var srcLiteral = JSON.stringify(String(source || ""));
        var gifQml = 'import QtQuick 2.15; AnimatedImage {'
                   + ' x: ' + (mapped.x - w/2)
                   + '; y: ' + (mapped.y - h/2)
                   + '; width: ' + w
                   + '; height: ' + h
                   + '; z: 9999'
                   + '; source: ' + srcLiteral
                   + '; playing: true; cache: false; fillMode: Image.Stretch; smooth: true'
                   + ' }';

        var gif = Qt.createQmlObject(gifQml, animManager);
        console.log('showLineGifAtRowCol: created line gif', source, 'at', row, col, 'vertical?', isVertical, 'size', w, h, 'duration', durationMs);

        var killer = Qt.createQmlObject('import QtQuick 2.15; Timer { interval: ' + (durationMs || 700) + '; repeat: false }', animManager);
        killer.triggered.connect(function(){ console.log('showLineGifAtRowCol: killer triggered for', source, row, col); if(gif) { try { gif.playing = false; gif.visible = false; gif.source = ""; } catch(e){} try { gif.destroy(); } catch(e){} } });
        killer.start();

        var stopper2 = Qt.createQmlObject('import QtQuick 2.15; Timer { interval: ' + (Math.max(1, (durationMs || 700) - 80)) + '; repeat: false }', animManager);
        stopper2.triggered.connect(function(){ console.log('showLineGifAtRowCol: stopper triggered for', source, row, col); try { if(gif) { gif.playing = false; gif.visible = false; gif.source = ""; if (typeof gif.frame !== 'undefined') gif.frame = 0; } } catch(e){} });
        stopper2.start();

        return gif;
    }

    // 处理火箭动画（自适配尺寸，延长到棋盘边缘）
    function runRocketEffect(row, col, type) {
        if (!gameBoard || !boardView) { console.error('runRocketEffect: missing gameBoard/boardView'); return; }
        var isVertical = (type === 1);
        // 注意：竖向火箭使用 lighting_V，横向火箭使用 lighting_H
        showLineGifAtRowCol(row, col, isVertical,
                            isVertical ? animManager.rocketGifVertical : animManager.rocketGifHorizontal,
                            1.08, 1.6, 700);

        // 收集需要被清除的 tiles（整行或整列）
        var tilesToClear = [];
        var rows = boardView.rows || 8;
        var cols = boardView.columns || 8;
        if (isVertical) {
            for (var r = 0; r < rows; ++r) { var t = findTile(r, col); if (t) tilesToClear.push(t); }
        } else {
            for (var c = 0; c < cols; ++c) { var t2 = findTile(row, c); if (t2) tilesToClear.push(t2); }
        }
        if (tilesToClear.length === 0) {
            console.error('runRocketEffect: no tiles found for rocket at', row, col);
            if (typeof gameBoard.rocketEffectTriggered === 'function')
                gameBoard.rocketEffectTriggered(row, col, type);
            return;
        }

        var pending = tilesToClear.length;
        tilesToClear.forEach(function(tile){
            // 关键：QML 语法里属性之间用 ';' 或换行分隔，不能用逗号。
            var anim = Qt.createQmlObject(
                'import QtQuick 2.15; SequentialAnimation {'
              + '  ParallelAnimation {'
              + '    NumberAnimation { property: "scale"; to: 0.80; duration: 200; easing.type: Easing.InQuad }'
              + '    NumberAnimation { property: "opacity"; to: 0; duration: 200; easing.type: Easing.InQuad }'
              + '  }'
              + '}',
                animManager
            );
            anim.animations[0].animations[0].target = tile;
            anim.animations[0].animations[1].target = tile;
            anim.onFinished.connect(function(){
                pending--;
                if (pending === 0) {
                    if (typeof gameBoard.rocketEffectTriggered === 'function')
                        gameBoard.rocketEffectTriggered(row, col, type);
                }
            });
            anim.start();
        });
    }

    // 小炸弹：中心小炸弹 GIF + 半径 2 区域放大淡出
    function runBombEffect(row, col, color) {
        console.log('runBombEffect:', row, col, 'color:', color);
        if (!color && gameBoard && typeof gameBoard.getTileColor === 'function') {
            try {
                color = gameBoard.getTileColor(row, col);
                console.log('runBombEffect: fallback color from gameBoard:', color);
            } catch(e) {
                console.log('runBombEffect: fallback color failed', e);
                color = '';
            }
        }

        showGifAt(row, col, 'qrc:/image/Animated/smallbomb.gif', 700, { tileSpan: 5.5 });

        var affected = [];
        var radius = 2;
        for (var r = row - radius; r <= row + radius; ++r) {
            for (var c = col - radius; c <= col + radius; ++c) {
                if (r >= 0 && r < animManager.rows && c >= 0 && c < animManager.cols) {
                    var dr = r - row;
                    var dc = c - col;
                    if (dr*dr + dc*dc <= radius*radius) {
                        var t = findTile(r, c);
                        if (t) affected.push(t);
                    }
                }
            }
        }

        if (affected.length === 0) {
            console.log('runBombEffect: no tiles, calling backend immediately');
            if (gameBoard && typeof gameBoard.bombEffectTriggered === 'function')
                gameBoard.bombEffectTriggered(row, col);
            return;
        }

        var pending = affected.length;
        affected.forEach(function(tile){
            var anim = Qt.createQmlObject('import QtQuick 2.15; SequentialAnimation { ParallelAnimation { NumberAnimation { property:"scale"; from:1; to:1.4; duration:180; easing.type: Easing.OutQuad } NumberAnimation { property:"opacity"; from:1; to:0; duration:180; easing.type: Easing.InQuad } } }', animManager);
            anim.animations[0].animations[0].target = tile;
            anim.animations[0].animations[1].target = tile;
            anim.onFinished.connect(function(){
                pending--;
                if (pending === 0) {
                    if (gameBoard && typeof gameBoard.bombEffectTriggered === 'function')
                        gameBoard.bombEffectTriggered(row, col);
                }
            });
            anim.start();
        });
    }

    // 炸弹+火箭：只消三行 或 三列（看火箭方向）
    function runComboBombRocket(row, col, rocketType) {
        console.log('runComboBombRocket at', row, col, 'rocketType', rocketType, '(NEW 3-rows OR 3-cols VERSION)');
        if (!boardView || !gameBoard) {
            if (gameBoard && typeof gameBoard.comboBombRocketEffectTriggered === 'function')
                gameBoard.comboBombRocketEffectTriggered(row, col, rocketType);
            return;
        }

        var isVertical = (rocketType === 1);
        showLineGifAtRowCol(row, col, isVertical,
                            isVertical ? animManager.rocketGifVertical : animManager.rocketGifHorizontal,
                            1.02, 1.6, 700);

        var rows = boardView.rows || animManager.rows;
        var cols = boardView.columns || animManager.cols;
        var affected = [];

        if (isVertical) {
            // 纵向火箭：只清 3 列（中心列 + 左右各一列），整盘高度
            for (var c = col - 1; c <= col + 1; ++c) {
                if (c < 0 || c >= cols) continue;
                for (var r = 0; r < rows; ++r) {
                    var tCol = findTile(r, c);
                    if (tCol) affected.push(tCol);
                }
            }
        } else {
            // 横向火箭：只清 3 行（中心行 + 上下各一行），整盘宽度
            for (var r2 = row - 1; r2 <= row + 1; ++r2) {
                if (r2 < 0 || r2 >= rows) continue;
                for (var c2 = 0; c2 < cols; ++c2) {
                    var tRow = findTile(r2, c2);
                    if (tRow) affected.push(tRow);
                }
            }
        }

        var unique = [];
        var keySet2 = {};
        affected.forEach(function(t2){
            var key = t2.row + '_' + t2.col;
            if (!keySet2[key]) { keySet2[key] = true; unique.push(t2); }
        });

        if (unique.length === 0) {
            console.log('runComboBombRocket(NEW): no tiles, calling backend');
            if (gameBoard && typeof gameBoard.comboBombRocketEffectTriggered === 'function')
                gameBoard.comboBombRocketEffectTriggered(row, col, rocketType);
            return;
        }

        // 注意：不要再“保护 SuperItem”，因为在新规则里道具也会被连锁清除/触发。
        var pending2 = unique.length;
        unique.forEach(function(tile2){
            var anim2 = Qt.createQmlObject('import QtQuick 2.15; SequentialAnimation { ParallelAnimation { NumberAnimation { property:"scale"; to:0; duration:220 } NumberAnimation { property: "opacity"; to:0; duration:220 } } }', animManager);
            anim2.animations[0].animations[0].target = tile2;
            anim2.animations[0].animations[1].target = tile2;
            anim2.onFinished.connect(function(){
                pending2--;
                if (pending2 === 0) {
                    console.log('runComboBombRocket(NEW): visuals done, calling backend');
                    if (gameBoard && typeof gameBoard.comboBombRocketEffectTriggered === 'function')
                        gameBoard.comboBombRocketEffectTriggered(row, col, rocketType);
                }
            });
            anim2.start();
        });
    }

    // 炸弹+炸弹：大范围圆形爆炸
    function runComboBombBomb(row, col) {
        console.log('runComboBombBomb at', row, col);
        if (!boardView || !gameBoard) {
            if (gameBoard && typeof gameBoard.comboBombBombEffectTriggered === 'function')
                gameBoard.comboBombBombEffectTriggered(row, col);
            return;
        }

        // 大炸弹 GIF
        showGifAt(row, col, 'qrc:/image/Animated/hugebomb.gif', 900, { tileSpan: 7.5 });

        var affected = [];
        var radius = 3; // 稍大一点的圆形范围
        for (var r = row - radius; r <= row + radius; ++r) {
            for (var c = col - radius; c <= col + radius; ++c) {
                if (r >= 0 && r < animManager.rows && c >= 0 && c < animManager.cols) {
                    var dr = r - row;
                    var dc = c - col;
                    if (dr*dr + dc*dc <= radius*radius) {
                        var t = findTile(r, c);
                        if (t) affected.push(t);
                    }
                }
            }
        }

        if (affected.length === 0) {
            console.log('runComboBombBomb: no tiles, calling backend immediately');
            if (gameBoard && typeof gameBoard.comboBombBombEffectTriggered === 'function')
                gameBoard.comboBombBombEffectTriggered(row, col);
            return;
        }

        var pending = affected.length;
        affected.forEach(function(tile){
            // 观感优化：不要“变大”，改为轻微缩小+淡出
            var anim = Qt.createQmlObject(
                        'import QtQuick 2.15; SequentialAnimation {'
                      + '  ParallelAnimation {'
                      + '    NumberAnimation { property:"scale"; from:1; to:0.80; duration:140; easing.type: Easing.InQuad }'
                      + '    NumberAnimation { property:"opacity"; from:1; to:0.0; duration:140; easing.type: Easing.InQuad }'
                      + '  }'
                      + '}', animManager);
            anim.animations[0].animations[0].target = tile;
            anim.animations[0].animations[1].target = tile;
            anim.onFinished.connect(function(){ pending--; if (pending===0 && done) done(); });
            anim.start();
        });
    }

    // 火箭+火箭组合动画
    function runComboRocketRocket(row, col) {
        console.log('runComboRocketRocket at', row, col);
        if (!boardView || !gameBoard) {
            if (gameBoard && typeof gameBoard.comboRocketRocketEffectTriggered === 'function')
                gameBoard.comboRocketRocketEffectTriggered(row, col);
            return;
        }

        // 十字火箭线：竖向用 V，横向用 H
        showLineGifAtRowCol(row, col, true,
                            animManager.rocketGifVertical,
                            1.08, 1.8, 700);
        showLineGifAtRowCol(row, col, false,
                            animManager.rocketGifHorizontal,
                            1.08, 1.8, 700);

        var tilesToClear = [];
        var rows2 = boardView.rows || 8;
        var cols2 = boardView.columns || 8;
        for (var r3 = 0; r3 < rows2; ++r3) {
            var tV = findTile(r3, col);
            if (tV) tilesToClear.push(tV);
        }
        for (var c3 = 0; c3 < cols2; ++c3) {
            var tH = findTile(row, c3);
            if (tH) tilesToClear.push(tH);
        }
        var uniq = [];
        var keySet4 = {};
        tilesToClear.forEach(function(t4){
            var key = t4.row + '_' + t4.col;
            if (!keySet4[key]) { keySet4[key] = true; uniq.push(t4); }
        });

        if (uniq.length === 0) {
            console.log('runComboRocketRocket: no tiles, calling backend');
            if (gameBoard && typeof gameBoard.comboRocketRocketEffectTriggered === 'function')
                gameBoard.comboRocketRocketEffectTriggered(row, col);
            return;
        }

        var pending4 = uniq.length;
        uniq.forEach(function(tile4){
            var anim4 = Qt.createQmlObject('import QtQuick 2.15; SequentialAnimation { ParallelAnimation { NumberAnimation { property:"scale"; to:0; duration:220 } NumberAnimation { property: "opacity"; to:0; duration:220 } } }', animManager);
            anim4.animations[0].animations[0].target = tile4;
            anim4.animations[0].animations[1].target = tile4;
            anim4.onFinished.connect(function(){
                pending4--;
                if (pending4 === 0) {
                    console.log('runComboRocketRocket: visuals done, calling backend comboRocketRocketEffectTriggered');
                    if (gameBoard && typeof gameBoard.comboRocketRocketEffectTriggered === 'function')
                        gameBoard.comboRocketRocketEffectTriggered(row, col);
                }
            });
            anim4.start();
        });
    }

    // 超级 + 火箭
    function runComboSuperRocket(row, col) {
        console.log('runComboSuperRocket at', row, col);
        if (!boardView || !gameBoard) {
            if (gameBoard && typeof gameBoard.comboSuperRocketEffectTriggered === 'function')
                gameBoard.comboSuperRocketEffectTriggered(row, col);
            return;
        }

        if (typeof gameBoard.comboSuperRocketEffectTriggered === 'function') {
            console.log('runComboSuperRocket: immediately calling comboSuperRocketEffectTriggered (stage1)');
            gameBoard.comboSuperRocketEffectTriggered(row, col);
        }
        refreshBoardColors();

        var centerTile = findTile(row, col);
        var cell2 = boardView.cellSize || animManager.cellSize;
        var localCx2 = centerTile ? (centerTile.x + centerTile.width/2) : (boardView.offsetX + col * cell2 + cell2/2);
        var localCy2 = centerTile ? (centerTile.y + centerTile.height/2) : (boardView.offsetY + row * cell2 + cell2/2);
        var mappedC2 = boardView.mapToItem(animManager, localCx2, localCy2);
        var maxDiam2 = Math.max(animManager.rows * animManager.cellSize, animManager.cols * animManager.cellSize) * 0.9;
        var cGif2 = Qt.createQmlObject('import QtQuick 2.15; AnimatedImage { x: ' + (mappedC2.x - maxDiam2/2) + '; y: ' + (mappedC2.y - maxDiam2/2) + '; width: ' + maxDiam2 + '; height: ' + maxDiam2 + '; z: 9999; source: "qrc:/image/Animated/lighting_circle.gif"; playing: true; cache: false; fillMode: Image.PreserveAspectFit; smooth: true }', animManager);
        var cTimer2 = Qt.createQmlObject('import QtQuick 2.15; Timer { interval: 600; repeat: false }', animManager);
        cTimer2.triggered.connect(function(){ if(cGif2) { try { cGif2.playing = false; cGif2.visible = false; cGif2.source = ""; } catch(e){} try { cGif2.destroy(); } catch(e){} } });
        cTimer2.start();

        var tiles2 = [];
        for (var r2 = 0; r2 < animManager.rows; r2++) {
            for (var c2b = 0; c2b < animManager.cols; c2b++) {
                var t2 = findTile(r2, c2b);
                if (t2) tiles2.push(t2);
            }
        }
        var centerRX2 = row;
        var centerCX2 = col;
        var durationBase2 = 220;
        var scaleTo2 = 1.12;
        var pending2 = tiles2.length;
        if (pending2 === 0) {
            console.log('runComboSuperRocket: no tiles for wave, directly execute rockets');
            if (typeof gameBoard.executeComboSuperRocket === 'function')
                gameBoard.executeComboSuperRocket(row, col);
            return;
        }

        function triggerAllRocketsSequentially() {
            console.log('runComboSuperRocket: wave done, start triggering all rockets sequentially');
            var rocketPositions = [];
            for (var rr = 0; rr < animManager.rows; ++rr) {
                for (var cc = 0; cc < animManager.cols; ++cc) {
                    try {
                        var colName = gameBoard.getTileColor(rr, cc);
                        if (colName === 'Rocket_1' || colName === 'Rocket_2') {
                            var rType = (colName === 'Rocket_1') ? 1 : 2; // 1=纵，2=横
                            rocketPositions.push({ row: rr, col: cc, type: rType });
                        }
                    } catch(e) {}
                }
            }
            if (rocketPositions.length === 0) {
                console.log('runComboSuperRocket: no Rocket tiles found after stage1, fallback to executeComboSuperRocket');
                if (typeof gameBoard.executeComboSuperRocket === 'function')
                    gameBoard.executeComboSuperRocket(row, col);
                return;
            }

            var index = 0;
            function triggerNext() {
                if (index >= rocketPositions.length) {
                    console.log('runComboSuperRocket: all rockets visually triggered, calling executeComboSuperRocket once');
                    if (typeof gameBoard.executeComboSuperRocket === 'function')
                        gameBoard.executeComboSuperRocket(row, col);
                    return;
                }
                var rp = rocketPositions[index++];
                console.log('runComboSuperRocket: trigger rocket at', rp.row, rp.col, 'type', rp.type);
                runRocketEffect(rp.row, rp.col, rp.type);

                var tmr2b = Qt.createQmlObject('import QtQuick 2.15; Timer { interval: 50; repeat: false }', animManager);
                tmr2b.triggered.connect(triggerNext);
                tmr2b.start();
            }
            triggerNext();
        }

        tiles2.forEach(function(t3){
            var dr2 = Math.abs(t3.row - centerRX2);
            var dc2 = Math.abs(t3.col - centerCX2);
            var ring2 = Math.max(dr2, dc2);
            var delay2 = ring2 * 35;
            var seq2 = Qt.createQmlObject('import QtQuick 2.15; SequentialAnimation { ' +
                'PauseAnimation { duration: ' + delay2 + ' } ' +
                'ParallelAnimation { NumberAnimation { property: "scale"; to: ' + scaleTo2 + '; duration: ' + Math.floor(durationBase2/2) + '; easing.type: Easing.OutBack } ' +
                'NumberAnimation { property: "opacity"; to: 0.8; duration: ' + Math.floor(durationBase2/2) + '; } } ' +
                'ParallelAnimation { NumberAnimation { property: "scale"; to: 1.0; duration: ' + Math.floor(durationBase2/2) + '; easing.type: Easing.InBack } ' +
                'NumberAnimation { property: "opacity"; to: 1.0; duration: ' + Math.floor(durationBase2/2) + '; } } ' +
            '}', animManager);
            seq2.animations[1].animations[0].target = t3;
            seq2.animations[1].animations[1].target = t3;
            seq2.animations[2].animations[0].target = t3;
            seq2.animations[2].animations[1].target = t3;
            seq2.onFinished.connect(function(){
                pending2--;
                if (pending2 === 0) {
                    console.log('runComboSuperRocket: wave visuals done, begin rocket chain');
                    triggerAllRocketsSequentially();
                }
            });
            seq2.start();
        });
    }

    // 超级 + 炸弹
    function runComboSuperBomb(row, col) {
        console.log('runComboSuperBomb at', row, col);
        if (!boardView || !gameBoard) {
            if (gameBoard && typeof gameBoard.comboSuperBombEffectTriggered === 'function')
                gameBoard.comboSuperBombEffectTriggered(row, col);
            return;
        }

        // 先直接让后端选色并把整盘该色变成炸弹，然后刷新一次棋盘颜色
        if (typeof gameBoard.comboSuperBombEffectTriggered === 'function') {
            console.log('runComboSuperBomb: immediately calling comboSuperBombEffectTriggered (stage1)');
            gameBoard.comboSuperBombEffectTriggered(row, col);
        }
        refreshBoardColors();

        // 中心环形光效
        var centerTile = findTile(row, col);
        var cell = boardView.cellSize || animManager.cellSize;
        var localCx = centerTile ? (centerTile.x + centerTile.width/2) : (boardView.offsetX + col * cell + cell/2);
        var localCy = centerTile ? (centerTile.y + centerTile.height/2) : (boardView.offsetY + row * cell + cell/2);
        var mappedC = boardView.mapToItem(animManager, localCx, localCy);
        var maxDiam = Math.max(animManager.rows * animManager.cellSize, animManager.cols * animManager.cellSize) * 0.9;
        var cGif = Qt.createQmlObject('import QtQuick 2.15; AnimatedImage { x: ' + (mappedC.x - maxDiam/2) + '; y: ' + (mappedC.y - maxDiam/2) + '; width: ' + maxDiam + '; height: ' + maxDiam + '; z: 9999; source: "qrc:/image/Animated/lighting_circle.gif"; playing: true; cache: false; fillMode: Image.PreserveAspectFit; smooth: true }', animManager);
        var cTimer = Qt.createQmlObject('import QtQuick 2.15; Timer { interval: 650; repeat: false }', animManager);
        cTimer.triggered.connect(function(){ if(cGif) { try { cGif.playing = false; cGif.visible = false; cGif.source = ""; } catch(e){} try { cGif.destroy(); } catch(e){} } });
        cTimer.start();

        // 全盘轻微波纹
        var tiles = [];
        for (var r = 0; r < animManager.rows; r++) {
            for (var c = 0; c < animManager.cols; c++) {
                var t = findTile(r, c);
                if (t) tiles.push(t);
            }
        }
        var centerRX = row;
        var centerCX = col;
        var durationBase = 240;
        var scaleTo = 1.18;
        var pending = tiles.length;
        if (pending === 0) {
            console.log('runComboSuperBomb: no tiles for wave, directly execute bombs');
            if (typeof gameBoard.executeComboSuperBomb === 'function')
                gameBoard.executeComboSuperBomb(row, col);
            return;
        }

        // 波纹结束后，逐个“只播放动画”触发所有炸弹：
        // 重要：这里绝对不能调用 runBombEffect（它会触发后端 bombEffectTriggered 并导致中途下落/打断链）
        function triggerAllBombsSequentially() {
            console.log('runComboSuperBomb: wave done, start triggering all bombs sequentially');
            var bombPositions = [];
            for (var rr = 0; rr < animManager.rows; ++rr) {
                for (var cc = 0; cc < animManager.cols; ++cc) {
                    try {
                        var colName = gameBoard.getTileColor(rr, cc);
                        if (colName === 'Bomb') {
                            bombPositions.push({ row: rr, col: cc, color: colName });
                        }
                    } catch(e) {}
                }
            }
            if (bombPositions.length === 0) {
                console.log('runComboSuperBomb: no Bomb tiles found after stage1, fallback to executeComboSuperBomb');
                if (typeof gameBoard.executeComboSuperBomb === 'function')
                    gameBoard.executeComboSuperBomb(row, col);
                return;
            }

            var index = 0;
            function triggerNext() {
                if (index >= bombPositions.length) {
                    console.log('runComboSuperBomb: all bombs visually triggered, calling executeComboSuperBomb once');
                    // stage2：只调用一次，让后端用 runPropChain 统一结算并下落
                    if (typeof gameBoard.executeComboSuperBomb === 'function')
                        gameBoard.executeComboSuperBomb(row, col);
                    return;
                }
                var bp = bombPositions[index++];
                console.log('runComboSuperBomb: trigger bomb visual at', bp.row, bp.col);

                // visual-only：播完一个再播下一个，保证“按顺序”表现
                runBombEffectVisual(bp.row, bp.col, bp.color, function(){
                    var tmr = Qt.createQmlObject('import QtQuick 2.15; Timer { interval: 35; repeat: false }', animManager);
                    tmr.triggered.connect(triggerNext);
                    tmr.start();
                });
            }
            triggerNext();
        }

        tiles.forEach(function(t){
            var dr = Math.abs(t.row - centerRX);
            var dc = Math.abs(t.col - centerCX);
            var ring = Math.max(dr, dc);
            var delay = ring * 38;
            var seq = Qt.createQmlObject('import QtQuick 2.15; SequentialAnimation { ' +
                'PauseAnimation { duration: ' + delay + ' } ' +
                'ParallelAnimation { NumberAnimation { property: "scale"; to: ' + scaleTo + '; duration: ' + Math.floor(durationBase/2) + '; easing.type: Easing.OutBack } ' +
                'NumberAnimation { property: "opacity"; to: 0.8; duration: ' + Math.floor(durationBase/2) + '; } } ' +
                'ParallelAnimation { NumberAnimation { property: "scale"; to: 1.0; duration: ' + Math.floor(durationBase/2) + '; easing.type: Easing.InBack } ' +
                'NumberAnimation { property: "opacity"; to: 1.0; duration: ' + Math.floor(durationBase/2) + '; } } ' +
            '}', animManager);
            seq.animations[1].animations[0].target = t;
            seq.animations[1].animations[1].target = t;
            seq.animations[2].animations[0].target = t;
            seq.animations[2].animations[1].target = t;
            seq.onFinished.connect(function(){
                pending--;
                if (pending === 0) {
                    console.log('runComboSuperBomb: wave visuals done, begin bomb chain');
                    triggerAllBombsSequentially();
                }
            });
            seq.start();
        });
    }

    // 新增：给外部（棋盘格双击/点击道具）调用的激活入口。
    // 目的：让“单独激活某一个道具”也走 previewPropChain + 顺序播放（方案B），实现连锁。
    function activateTileAt(row, col) {
        if (!gameBoard) return;
        var color = '';
        try { color = gameBoard.getTileColor(row, col); } catch(e) { color = ''; }

        var type = 0;
        // 关键：按你的工程资源约定修正
        // Rocket_1 => 竖向(火箭上/下)
        // Rocket_2 => 横向(火箭左/右)
        if (color === 'Rocket_1') type = rocket_upDownType;
        else if (color === 'Rocket_2') type = rocket_leftRightType;
        else if (color === 'Bomb') type = bombType;
        else if (color === 'SuperItem') type = superItemType;

        if (type === 0) {
            console.log('activateTileAt: not a prop at', row, col, 'color=', color);
            return;
        }

        // 单击/双击激活道具：统一走 handlePropEffectRequested，这样就会走 previewPropChain 的连锁流程
        handlePropEffectRequested(row, col, type, '');
    }

}

// === QML 侧：在后端 commitDrop 后如果出现了新的匹配，应该播放匹配动画而不是“直接消除” ===
// 说明：当前后端的 processOneCascadeStep() 同时负责（1）消除/触发道具链、（2）生成掉落。
// 为了让“掉落后继续三消”也有动画，需要在 commitDrop() 被调用后让后端显式发出 matchAnimationRequested。
// 这部分需要 C++ 侧配合（见 GameBoardCompat::commitDrop 的改动）。
