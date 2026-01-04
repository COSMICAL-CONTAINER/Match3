import QtQuick 2.15

Item {
    id: animManager
    property var boardView
    property var gameBoard
    property var tileMap: ({})
    property real cellSize: 50  // 可以在主界面传入
    property bool busy: false   // 全局锁，防止重复输入/二次交换

    // 连接游戏板信号
    onGameBoardChanged: {
        if (gameBoard) {
            gameBoard.swapAnimationRequested.connect(animateSwap);
            gameBoard.matchAnimationRequested.connect(animateMatches);
            // gameBoard.dropAnimationRequested.connect(animateDrops);
            gameBoard.animateRollback.connect(rollbackSwap);
            console.log("动画管理器已连接到游戏板");
        }
    }

    function findTile(row, col) {
        var key = row + "_" + col;
        return tileMap[key] || null;
    }

    // 交换动画
    function animateSwap(r1, c1, r2, c2) {
        if (animManager.busy) return;
        animManager.busy = true;

        var t1 = findTile(r1, c1);
        var t2 = findTile(r2, c2);
        if (!t1 || !t2) { animManager.busy = false; return; }

        var dx = (c2 - c1) * cellSize;
        var dy = (r2 - r1) * cellSize;

        var anim1 = Qt.createQmlObject(`
            import QtQuick 2.15
            ParallelAnimation {
                NumberAnimation { property: "offsetX"; duration: 200 }
                NumberAnimation { property: "offsetY"; duration: 200 }
            }
        `, animManager);
        anim1.animations[0].target = t1; anim1.animations[0].from = 0; anim1.animations[0].to = dx;
        anim1.animations[1].target = t1; anim1.animations[1].from = 0; anim1.animations[1].to = dy;
        anim1.start();

        var anim2 = Qt.createQmlObject(`
            import QtQuick 2.15
            ParallelAnimation {
                NumberAnimation { property: "offsetX"; duration: 200 }
                NumberAnimation { property: "offsetY"; duration: 200 }
            }
        `, animManager);
        anim2.animations[0].target = t2; anim2.animations[0].from = 0; anim2.animations[0].to = -dx;
        anim2.animations[1].target = t2; anim2.animations[1].from = 0; anim2.animations[1].to = -dy;

        anim2.onFinished.connect(function() {
            tileMap[r1 + "_" + c1] = t2;
            tileMap[r2 + "_" + c2] = t1;

            t1.offsetX = 0; t1.offsetY = 0;
            t2.offsetX = 0; t2.offsetY = 0;
            animManager.busy = false;

            // ✅ 动画完成后再让 C++ 真正交换
            gameBoard.finalizeSwap(r1, c1, r2, c2);
        });

        anim2.start();
    }
    function rollbackSwap(r1, c1, r2, c2) {
        if (animManager.busy) return;
        animManager.busy = true;

        var t1 = findTile(r1, c1);
        var t2 = findTile(r2, c2);
        if (!t1 || !t2) return;

        // 回滚 t1
        var rollback1 = Qt.createQmlObject(`
            import QtQuick 2.15
            ParallelAnimation {
                NumberAnimation { property: "offsetX"; to: 0; duration: 200 }
                NumberAnimation { property: "offsetY"; to: 0; duration: 200 }
            }
        `, animManager);
        rollback1.animations[0].target = t1; rollback1.animations[1].target = t1; rollback1.start();

        // 回滚 t2
        var rollback2 = Qt.createQmlObject(`
            import QtQuick 2.15
            ParallelAnimation {
                NumberAnimation { property: "offsetX"; to: 0; duration: 200 }
                NumberAnimation { property: "offsetY"; to: 0; duration: 200 }
            }
        `, animManager);
        rollback2.animations[0].target = t2; rollback2.animations[1].target = t2; rollback2.start();

        shakeTile(t1);
        shakeTile(t2);

        // 保证下次干净
        t1.offsetX = 0; t1.offsetY = 0;
        t2.offsetX = 0; t2.offsetY = 0;
        animManager.busy = false;
    }


    // 匹配动画
    function animateMatches(matchedTiles) {
        for (var i = 0; i < matchedTiles.length; i++) {
            var point = matchedTiles[i];
            var tile = findTile(point.x, point.y);
            if (!tile) continue;

            tile.scale = 1.2;
            tile.opacity = 0.5;

            // 缩放动画
            var animScale = Qt.createQmlObject(`
                import QtQuick 2.15
                NumberAnimation { property var targetTile: null; target: targetTile; property string prop: "scale"; to: 1.0; duration: 300 }
            `, animManager);
            animScale.targetTile = tile;
            animScale.start();

            // 透明度动画
            var animOpacity = Qt.createQmlObject(`
                import QtQuick 2.15
                NumberAnimation { property var targetTile: null; target: targetTile; property string prop: "opacity"; to: 1.0; duration: 300 }
            `, animManager);
            animOpacity.targetTile = tile;
            animOpacity.start();
        }
    }

    // 掉落动画
    function animateDrops(dropPaths) {
        console.log("执行颜色掉落，路径数:", dropPaths.length);
        if (!dropPaths || dropPaths.length === 0) return;
        if (animManager.busy) {
            // 如果正在忙，可以选择缓存或直接丢弃；这里先返回避免冲突
            console.log("busy, ignore animateColorDrops");
            return;
        }
        animManager.busy = true;

        var paths = dropPaths.slice();    // 复制
        // 不在这里修改外部 dropPaths（调用方负责），以免影响其他逻辑

        var pending = 0;
        function animStarted() { pending++; }
        function animFinished() {
            pending--;
            if (pending <= 0) {
                // 所有掉落动画完成后清理状态
                animManager.busy = false;
                // 可在此处触发下一步（如再次检测匹配）
            }
        }

        for (var i = 0; i < paths.length; i++) {
            var path = paths[i];
            if (!path || path.length < 2) continue;

            // 从下向上逐格传色（保证覆盖时不会破坏上方未处理的颜色）
            for (var j = path.length - 1; j > 0; j--) {
                var from = path[j - 1];
                var to   = path[j];

                var tileFrom = findTile(from.x, from.y);
                var tileTo   = findTile(to.x, to.y);
                if (!tileFrom || !tileTo) continue;

                // 记录颜色并传递
                var color = tileFrom.tileColor;
                tileTo.tileColor = color;
                tileFrom.tileColor = "transparent"; // 清空上格（或设为 ""）

                // small animation to simulate drop (scale + fade-in)
                var anim = Qt.createQmlObject(`
                    import QtQuick 2.15
                    SequentialAnimation {
                        NumberAnimation { property: "scale"; from: 1.2; to: 1.0; duration: 150; easing.type: Easing.OutCubic }
                        NumberAnimation { property: "opacity"; from: 0.6; to: 1.0; duration: 150 }
                    }
                `, animManager);

                // 使用 animations 数组来设置 target
                if (anim.animations && anim.animations.length > 0) {
                    anim.animations[0].target = tileTo;
                    anim.animations[1].target = tileTo;
                } else {
                    // 保险回退：直接设置 target 属性（少数 Qt 版本可能差异）
                    try { anim.children[0].target = tileTo; anim.children[1].target = tileTo; } catch(e) {}
                }

                // 计数与回调，防止闭包覆盖变量
                (function(a, t){
                    animStarted();
                    a.onFinished.connect(function() {
                        animFinished();
                    });
                    a.start();
                })(anim, tileTo);
            }

            // 如果路径最顶端是 fromRow == -1（表示新生成），我们需要设置最顶格的颜色：
            var top = path[0];
            if (top && top.x < 0) {
                // 需要 C++ 提供新颜色，优雅方法：在 GameBoard 暴露 Q_INVOKABLE QString getRandomColorQml()
                var newColor = "gray";
                if (gameBoard && typeof gameBoard.getRandomColorQml === "function")
                    newColor = gameBoard.getRandomColorQml();
                var topTile = findTile(top.x, top.y);
                if (topTile) topTile.tileColor = newColor;
            }
        }

        // 如果没有任何动画被启动（pending==0），立即解锁
        if (pending === 0) animManager.busy = false;
    }

    // 抖动动画
    function shakeTile(tile) {
        if (!tile) return;
        var originalX = tile.x;

        var anim = Qt.createQmlObject(`
            import QtQuick 2.15
            SequentialAnimation {
                property var targetTile: null
                property real origX: 0
                NumberAnimation { target: targetTile; property: "x"; to: origX + 5; duration: 50 }
                NumberAnimation { target: targetTile; property: "x"; to: origX - 5; duration: 50 }
                NumberAnimation { target: targetTile; property: "x"; to: origX; duration: 50 }
            }
        `, animManager);

        anim.targetTile = tile;
        anim.origX = originalX;
        anim.start();
    }
}
