import QtQuick 2.15

Item {
    id: animManager
    property var boardView
    property var gameBoard
    property var tileMap: ({})
    property real cellSize: 50
    property bool busy: false

    // ===== 队列 =====
    property var swapQueue: []
    property var matchQueue: []
    property var dropQueue: []

    // ===== 连接游戏板信号 =====
    onGameBoardChanged: {
        if (gameBoard) {
            gameBoard.swapAnimationRequested.connect(enqueueSwap);
            gameBoard.matchAnimationRequested.connect(enqueueMatches);
            gameBoard.dropAnimationRequested.connect(enqueueDrops);
            gameBoard.rollbackSwap.connect(rollbackSwap);
            console.log("动画管理器已连接到游戏板");
        }
    }

    function findTile(row, col) {
        // console.log("Searching for tile: " + row + "_" + col);
        // console.log("TileMap: ", tileMap);
        return tileMap[row + "_" + col] || null;
    }

    // ===== 队列入列函数 =====
    function enqueueSwap(r1,c1,r2,c2){ swapQueue.push({r1:r1,c1:c1,r2:r2,c2:c2}); runNext(); }
    function enqueueMatches(matchedTiles){ matchQueue.push(matchedTiles); runNext(); }
    function enqueueDrops(dropPaths){ console.log("enqueueDrops called, dropPaths size:", dropPaths ? dropPaths.length : 0); dropQueue.push(dropPaths); runNext(); }

    // ===== 队列驱动 =====
    function runNext(){
        if(busy) return;
        busy = true;
        if(swapQueue.length>0){
            var s=swapQueue.shift();
            runSwap(s.r1,s.c1,s.r2,s.c2);
        } else if(matchQueue.length>0){
            var m=matchQueue.shift();
            runMatches(m);
        } else if(dropQueue.length>0){
            var d=dropQueue.shift();
            runDrops(d);
        } else {
            busy=false;
        }
    }

    // ===== 交换动画 =====
    function runSwap(r1,c1,r2,c2){
        var t1=findTile(r1,c1), t2=findTile(r2,c2);
        if(!t1||!t2){ busy=false; runNext(); return; }
        var dx=(c2-c1)*cellSize, dy=(r2-r1)*cellSize;

        var anim1=Qt.createQmlObject(`import QtQuick 2.15; ParallelAnimation { NumberAnimation { property: "offsetX"; duration: 200 } NumberAnimation { property: "offsetY"; duration: 200 } }`, animManager);
        anim1.animations[0].target=t1; anim1.animations[0].from=0; anim1.animations[0].to=dx;
        anim1.animations[1].target=t1; anim1.animations[1].from=0; anim1.animations[1].to=dy;

        var anim2=Qt.createQmlObject(`import QtQuick 2.15; ParallelAnimation { NumberAnimation { property: "offsetX"; duration: 200 } NumberAnimation { property: "offsetY"; duration: 200 } }`, animManager);
        anim2.animations[0].target=t2; anim2.animations[0].from=0; anim2.animations[0].to=-dx;
        anim2.animations[1].target=t2; anim2.animations[1].from=0; anim2.animations[1].to=-dy;

        var completed=0;
        function onFinished(){ completed++; if(completed===2){
            t1.offsetX=0;t1.offsetY=0; t2.offsetX=0;t2.offsetY=0;
            // 后端c++处理匹配结果
            gameBoard.finalizeSwap(r1,c1,r2,c2,false);
            // 更新 tileMap
            tileMap[r1+"_"+c1]=findTile(r1,c1);
            tileMap[r2+"_"+c2]=findTile(r2,c2);
            busy=false; runNext();
        }}
        anim1.onFinished.connect(onFinished);
        anim2.onFinished.connect(onFinished);
        anim1.start(); anim2.start();
    }

    function rollbackSwap(r1,c1,r2,c2){
        if(busy) return;
        busy=true;
        var t1=findTile(r1,c1), t2=findTile(r2,c2);
        if(!t1||!t2){ busy=false; return; }

        [t1,t2].forEach(function(t){
            var rollback=Qt.createQmlObject(`import QtQuick 2.15; ParallelAnimation { NumberAnimation { property: "offsetX"; to:0; duration:200 } NumberAnimation { property: "offsetY"; to:0; duration:200 } }`, animManager);
            rollback.animations[0].target=t; rollback.animations[1].target=t;
            rollback.start();
        });

        shakeTile(t1); shakeTile(t2);
        t1.offsetX=0; t1.offsetY=0; t2.offsetX=0; t2.offsetY=0;
        busy=false;
    }

    // ===== 匹配动画 =====
    function runMatches(matchedTiles){
        if(!matchedTiles||matchedTiles.length===0){ busy=false; runNext(); return; }
        var pending=matchedTiles.length;
        matchedTiles.forEach(function(pt){
            var tile=findTile(pt.x,pt.y);
            if(!tile){ pending--; return; }
            var anim=Qt.createQmlObject(`
                import QtQuick 2.15
                SequentialAnimation {
                    ParallelAnimation { NumberAnimation { property:"scale"; to:1.2; duration:150; easing.type:Easing.OutQuad }
                                       NumberAnimation { property:"opacity"; to:0.5; duration:150; easing.type:Easing.OutQuad } }
                    ParallelAnimation { NumberAnimation { property:"scale"; to:1.0; duration:150; easing.type:Easing.InQuad }
                                       NumberAnimation { property:"opacity"; to:1.0; duration:150; easing.type:Easing.InQuad } }
                }
            `, animManager);
            anim.animations[0].animations[0].target=tile;
            anim.animations[0].animations[1].target=tile;
            anim.animations[1].animations[0].target=tile;
            anim.animations[1].animations[1].target=tile;
            anim.onFinished.connect(function(){ pending--; if(pending===0){ busy=false; runNext(); } });
            anim.start();
        });
    }

    // ===== 掉落动画 =====
    function refreshBoardColors() {
        if (!gameBoard) return;
        for (var key in tileMap) {
            var parts = key.split("_");
            var r = parseInt(parts[0]);
            var c = parseInt(parts[1]);
            var tile = tileMap[key];
            if (tile && typeof gameBoard.getTileColor === "function") {
                tile.tileColor = gameBoard.getTileColor(r, c);
            }
        }
    }
    function runDrops(dropPaths){
        console.log("runDrops called, dropPaths:", dropPaths);
         if(!dropPaths||dropPaths.length===0){
             console.log("runDrops: empty dropPaths, calling commitDrop");
             // 即使没有移动路径，也需要让后端执行 commitDrop（填充新方块并检测连锁）
             if (gameBoard && typeof gameBoard.commitDrop === "function") {
                 gameBoard.commitDrop();
             }
             busy=false; runNext();
             return;
         }
        var pending=0;
        var commitCalled=false;
        dropPaths.forEach(function(path){
            for(var j=path.length-1;j>0;j--){
                var from=path[j-1], to=path[j];
                var tileFrom=findTile(from.x,from.y), tileTo=findTile(to.x,to.y);
                if(!tileFrom||!tileTo) continue;
                // 直接更新目标颜色（数据最终由后端 commitDrop 写入），动画仅为位移动画
                tileTo.tileColor=tileFrom.tileColor;
                tileFrom.tileColor="transparent";

                // 使用 offsetY 做下落位移动画，不改变 scale
                var anim=Qt.createQmlObject(`
                    import QtQuick 2.15
                    SequentialAnimation {
                        NumberAnimation { property: "offsetY"; from: -animManager.cellSize; to: 0; duration: 180; easing.type: Easing.OutCubic }
                    }
                `, animManager);
                anim.animations[0].target=tileTo;

                pending++;
                (function(a, target){ a.onFinished.connect(function(){
                    // 重置偏移并减少计数
                    if (target) target.offsetY = 0;
                    pending--; if(pending===0){
                        // 所有掉落动画完成后，通知后端把实际棋盘状态同步（应用重力并填充）
                        if (!commitCalled && gameBoard && typeof gameBoard.commitDrop === "function") {
                            commitCalled = true;
                            gameBoard.commitDrop();
                        }
                        busy=false; runNext();
                    }
                }); a.start(); })(anim, tileTo);
            }

            var top=path[0];
            if(top && top.x<0){
                var newColor="gray";
                if(gameBoard && typeof gameBoard.getRandomColorQml==="function")
                    newColor=gameBoard.getRandomColorQml();
                var topTile=findTile(top.x,top.y);
                if(topTile) topTile.tileColor=newColor;
            }
        });
        if(pending===0){ refreshBoardColors(); if (!commitCalled && gameBoard && typeof gameBoard.commitDrop === "function") { commitCalled = true; gameBoard.commitDrop(); } busy=false; runNext(); }
    }

    // 统一自适应：基于棋盘尺寸生成线性 GIF（行/列）
    function showLineGifAtRowCol(row, col, isVertical, source, lengthScale, thicknessTiles, durationMs) {
        if (!boardView) return null;
        var cell = boardView.cellSize || animManager.cellSize;
        var rows = boardView.rows || 8;
        var cols = boardView.columns || 8;
        var boardCenterX = boardView.offsetX + cols * cell / 2;
        var boardCenterY = boardView.offsetY + rows * cell / 2;
        // 长度按整盘尺寸计算，再乘 lengthScale；若资源保持比例导致长度未拉满，改用 Stretch 强制拉伸
        var length = (isVertical ? (rows * cell) : (cols * cell)) * (lengthScale || 1.0);
        var thickness = Math.max((thicknessTiles || 1) * cell, cell * 1.8);
        var localX = isVertical ? (boardView.offsetX + col * cell + cell/2) : boardCenterX;
        var localY = isVertical ? boardCenterY : (boardView.offsetY + row * cell + cell/2);
        var mapped = boardView.mapToItem(animManager, localX, localY);
        var w = isVertical ? thickness : length;
        var h = isVertical ? length : thickness;
        var gif = Qt.createQmlObject('import QtQuick 2.15; AnimatedImage { x: ' + (mapped.x - w/2) + '; y: ' + (mapped.y - h/2) + '; width: ' + w + '; height: ' + h + '; z: 9999; source: "' + source + '"; playing: true; cache: false; fillMode: Image.Stretch; smooth: true }', animManager);
        var killer = Qt.createQmlObject('import QtQuick 2.15; Timer { interval: ' + (durationMs || 700) + '; repeat: false }', animManager);
        killer.triggered.connect(function(){ if(gif) gif.destroy(); });
        killer.start();
        return gif;
    }

    // 处理火箭动画（自适配尺寸，延长到棋盘边缘）
    function runRocketEffect(row, col, type) {
        if (!gameBoard || !boardView) { console.error("runRocketEffect: missing gameBoard/boardView"); return; }
        var isVertical = (type === 1);
        // 将长度提升到覆盖整行/整列边缘（略加 1.02 余量以避免边界取整导致的缺口）
        showLineGifAtRowCol(row, col, isVertical,
                            isVertical ? 'qrc:/image/Animated/lighting_H.gif' : 'qrc:/image/Animated/lighting_V.gif',
                            1.02, 1.6, 700);

        // 收集需要被清除的 tiles（整行或整列）
        var tilesToClear = [];
        var rows = boardView.rows || 8;
        var cols = boardView.columns || 8;
        if (isVertical) {
            for (var r = 0; r < rows; ++r) { var t = findTile(r, col); if (t) tilesToClear.push(t); }
        } else {
            for (var c = 0; c < cols; ++c) { var t2 = findTile(row, c); if (t2) tilesToClear.push(t2); }
        }
        if (tilesToClear.length === 0) { console.error("runRocketEffect: no tiles found for rocket at", row, col); gameBoard.rocketEffectTriggered(row, col, type); return; }

        var pending = tilesToClear.length;
        tilesToClear.forEach(function(tile){
            var anim = Qt.createQmlObject('import QtQuick 2.15; SequentialAnimation { ParallelAnimation { NumberAnimation { property: "scale"; to: 0; duration: 200; easing.type: Easing.InQuad } NumberAnimation { property: "opacity"; to: 0; duration: 200; easing.type: Easing.InQuad } } }', animManager);
            // 修正：SequentialAnimation 中只有一个 ParallelAnimation，不能访问 animations[1]
            anim.animations[0].animations[0].target = tile;
            anim.animations[0].animations[1].target = tile;
            anim.onFinished.connect(function(){ pending--; if (pending === 0) { gameBoard.rocketEffectTriggered(row, col, type); } });
            anim.start();
        });
    }

    // 炸弹+火箭 -> 自适配 hugelighting_* GIF（线长覆盖棋盘边缘，厚度按 3 格）
    function runComboBombRocket(row, col, rocketType) {
        console.log("runComboBombRocket: combo at", row, col, "rocketType:", rocketType);
        var affected = [];
        var isVertical = (rocketType === 1);
        // 延长到棋盘边缘（略加 1.08 余量，确保覆盖）
        showLineGifAtRowCol(row, col, isVertical,
                            isVertical ? 'qrc:/image/Animated/hugelighting_H.gif' : 'qrc:/image/Animated/hugelighting_V.gif',
                            1.08, 3.0, 900);
        // 收集三行/三列
        if (isVertical) {
            for (var dc = -1; dc <= 1; ++dc) {
                var cc = col + dc; if(cc<0||cc>=8) continue;
                for (var r = 0; r < 8; ++r) { var t = findTile(r, cc); if(t) affected.push(t); }
            }
        } else {
            for (var dr = -1; dr <= 1; ++dr) {
                var rr = row + dr; if(rr<0||rr>=8) continue;
                for (var c = 0; c < 8; ++c) { var t2 = findTile(rr, c); if(t2) affected.push(t2); }
            }
        }
        // 去重 + 后端触发
        var unique = [];
        var keySet2 = {};
        affected.forEach(function(t){ var key = t.row + "_" + t.col; if(!keySet2[key]){ keySet2[key]=true; unique.push(t); } });
        if(unique.length===0){ console.log("runComboBombRocket: none, backend"); gameBoard.bombRocketTriggered(row,col,rocketType); return; }
        var pending = unique.length;
        unique.forEach(function(tile){
            var anim = Qt.createQmlObject('import QtQuick 2.15; SequentialAnimation { ParallelAnimation { NumberAnimation { property:"scale"; to:0; duration:200 } NumberAnimation { property:"opacity"; to:0; duration:200 } } }', animManager);
            anim.animations[0].animations[0].target = tile; anim.animations[0].animations[1].target = tile;
            anim.onFinished.connect(function(){ pending--; if(pending===0){ console.log("runComboBombRocket: visuals done, calling backend"); gameBoard.bombRocketTriggered(row,col,rocketType); } });
            anim.start();
        });
    }

    // 小炸弹
    function runBombEffect(row, col, color) {
        console.log("runBombEffect:", row, col, "color:", color);
        // 播放自适配小炸弹 GIF（5.5 格直径），并在 700ms 后销毁
        showGifAt(row, col, 'qrc:/image/Animated/smallbomb.gif', 700, { tileSpan: 5.5 });

        // 收集半径 2 的圆形范围内的格子
        var affected = [];
        var radius = 2;
        for (var r = row - radius; r <= row + radius; ++r) {
            for (var c = col - radius; c <= col + radius; ++c) {
                if (r >= 0 && r < 8 && c >= 0 && c < 8) {
                    var dr = r - row, dc = c - col;
                    if (dr*dr + dc*dc <= radius*radius) {
                        var t = findTile(r, c);
                        if (t) affected.push(t);
                    }
                }
            }
        }

        if (affected.length === 0) {
            console.log("runBombEffect: no tiles, calling backend immediately");
            gameBoard.bombEffectTriggered(row, col, color);
            return;
        }

        var pending = affected.length;
        affected.forEach(function(tile){
            var anim = Qt.createQmlObject('import QtQuick 2.15; SequentialAnimation { ParallelAnimation { NumberAnimation { property:"scale"; from:1; to:1.4; duration:180; easing.type: Easing.OutQuad } NumberAnimation { property:"opacity"; from:1; to:0; duration:180; easing.type: Easing.InQuad } } }', animManager);
            anim.animations[0].animations[0].target = tile;
            anim.animations[0].animations[1].target = tile;
            anim.onFinished.connect(function(){ pending--; if (pending === 0) { gameBoard.bombEffectTriggered(row, col, color); } });
            anim.start();
        });
    }

    // 超级道具 -> 自适配环形光效，直径按棋盘较大边的 1.1 倍（保持原方案）
    function runSuperItemEffect(row, col, color) {
        if (boardView) {
            var cell = boardView.cellSize || animManager.cellSize;
            var rows = boardView.rows || 8;
            var cols = boardView.columns || 8;
            var maxDiam = Math.max(rows * cell, cols * cell) * 1.1;
            var tile = findTile(row, col);
            var localCx = tile ? (tile.x + tile.width/2) : (boardView.offsetX + col * cell + cell/2);
            var localCy = tile ? (tile.y + tile.height/2) : (boardView.offsetY + row * cell + cell/2);
            var mappedC = boardView.mapToItem(animManager, localCx, localCy);
            var cGif = Qt.createQmlObject('import QtQuick 2.15; AnimatedImage { x: ' + (mappedC.x - maxDiam/2) + '; y: ' + (mappedC.y - maxDiam/2) + '; width: ' + maxDiam + '; height: ' + maxDiam + '; z: 9999; source: "qrc:/image/Animated/lighting_circle.gif"; playing: true; cache: false; fillMode: Image.PreserveAspectFit; smooth: true }', animManager);
            var cTimer = Qt.createQmlObject('import QtQuick 2.15; Timer { interval: 800; repeat: false }', animManager);
            cTimer.triggered.connect(function(){ if(cGif) cGif.destroy(); });
            cTimer.start();
        }

        var matchedTiles = [];
        for (var r = 0; r < 8; r++) {
            for (var c = 0; c < 8; c++) {
                if (gameBoard.tileAt(r, c) === color) {
                    var t = findTile(r, c);
                    if (t) matchedTiles.push(t);
                }
            }
        }

        if (matchedTiles.length === 0) {
            console.log("runSuperItemEffect: no tiles to animate, calling backend");
            gameBoard.superItemEffectTriggered(row, col, color);
            return;
        }

        var pending = matchedTiles.length;
        matchedTiles.forEach(function(tile) {
            var anim = Qt.createQmlObject(`
                import QtQuick 2.15
                SequentialAnimation {
                    ParallelAnimation { NumberAnimation { property: "scale"; from: 1; to: 1.2; duration: 120; easing.type: Easing.OutQuad }
                                       NumberAnimation { property: "opacity"; from: 1; to: 0.6; duration: 120 } }
                    ParallelAnimation { NumberAnimation { property: "scale"; from: 1.2; to: 1.5; duration: 180; easing.type: Easing.OutElastic }
                                       NumberAnimation { property: "opacity"; from: 0.6; to: 0; duration: 180 } }
                }
            `, animManager);
            anim.animations[0].animations[0].target = tile;
            anim.animations[0].animations[1].target = tile;
            anim.animations[1].animations[0].target = tile;
            anim.animations[1].animations[1].target = tile;
            anim.onFinished.connect(function(){ pending--; if(pending===0){ gameBoard.superItemEffectTriggered(row, col, color); } });
            anim.start();
        });
    }

    // ===== 组合道具动画 =====
    // 火箭+火箭 -> 十字（整行 + 整列）
    function runComboRocketRocket(row, col) {
        console.log("runComboRocketRocket: combo at", row, col);
        // 新增：横竖各播放一次与单火箭一致的 lighting 线性特效
        showLineGifAtRowCol(row, col, true,
                            'qrc:/image/Animated/lighting_H.gif',
                            1.02, 1.6, 700);
        showLineGifAtRowCol(row, col, false,
                            'qrc:/image/Animated/lighting_V.gif',
                            1.02, 1.6, 700);
        var affected = [];
        for (var c = 0; c < 8; ++c) { var t = findTile(row, c); if(t) affected.push(t); }
        for (var r = 0; r < 8; ++r) { var t2 = findTile(r, col); if(t2) affected.push(t2); }
        // 去重
        var unique = [];
        var keySet = {};
        affected.forEach(function(t){ var key = t.row + "_" + t.col; if(!keySet[key]){ keySet[key]=true; unique.push(t); } });
        if(unique.length===0){ console.log("runComboRocketRocket: no tiles, calling backend"); gameBoard.rocketRocketTriggered(row,col); return; }
        var pending = unique.length;
        unique.forEach(function(tile){
            var anim = Qt.createQmlObject(`import QtQuick 2.15; SequentialAnimation { ParallelAnimation { NumberAnimation { property:"scale"; to:0; duration:220 } NumberAnimation { property:"opacity"; to:0; duration:220 } } }`, animManager);
            anim.animations[0].animations[0].target = tile; anim.animations[0].animations[1].target = tile;
            anim.onFinished.connect(function(){ pending--; if(pending===0){ console.log("runComboRocketRocket: visuals done, calling backend"); gameBoard.rocketRocketTriggered(row,col); } });
            anim.start();
        });
    }

    // 炸弹+炸弹 -> 大范围爆炸（半径4）
    function runComboBombBomb(row, col) {
        console.log("runComboBombBomb: combo at", row, col);
        // 大炸弹：覆盖约 9.5x9.5 格（500px GIF 更适配），略向上偏移微调
        showGifAt(row, col, 'qrc:/image/Animated/hugebomb.gif', 950, { tileSpan: 9.5, offsetY: -10 });

        var radius = 4;
        var affected = [];
        for (var r = row - radius; r <= row + radius; ++r) {
            for (var c = col - radius; c <= col + radius; ++c) {
                if (r >= 0 && r < 8 && c >= 0 && c < 8) {
                    if ((r-row)*(r-row) + (c-col)*(c-col) <= radius*radius) {
                        var t = findTile(r,c); if(t) affected.push(t);
                    }
                }
            }
        }
        if(affected.length===0){ console.log("runComboBombBomb: none, backend"); gameBoard.bombBombTriggered(row,col); return; }
        var pending = affected.length;
        affected.forEach(function(tile){
            var anim = Qt.createQmlObject(`import QtQuick 2.15; SequentialAnimation { ParallelAnimation { NumberAnimation { property:"scale"; from:1; to:1.6; duration:180 } NumberAnimation { property:"opacity"; from:1; to:0; duration:180 } } }`, animManager);
            anim.animations[0].animations[0].target = tile; anim.animations[0].animations[1].target = tile;
            anim.onFinished.connect(function(){ pending--; if(pending===0){ console.log("runComboBombBomb: visuals done, calling backend"); gameBoard.bombBombTriggered(row,col); } });
            anim.start();
        });
    }

    // 超级 + 炸弹 -> 先播合成视觉，然后调用后端 superBombTriggered
    function runComboSuperBomb(row, col) {
        console.log("runComboSuperBomb: combo at", row, col);
        // 简单视觉：让周围 4 个邻格做短暂闪烁，然后直接调用后端
        var neigh = [];
        var dirs = [[-1,0],[1,0],[0,-1],[0,1]];
        for (var i = 0; i < dirs.length; ++i) {
            var rr = row + dirs[i][0]; var cc = col + dirs[i][1];
            if (rr>=0 && rr<8 && cc>=0 && cc<8) { var t = findTile(rr,cc); if(t) neigh.push(t); }
        }
        var pending = neigh.length;
        if (pending === 0) { console.log("runComboSuperBomb: no neighbor visuals, calling backend"); gameBoard.superBombTriggered(row,col); return; }
        neigh.forEach(function(tile){
            var anim = Qt.createQmlObject(`import QtQuick 2.15; SequentialAnimation { NumberAnimation { property:"scale"; to:1.2; duration:120 } NumberAnimation { property:"scale"; to:1.0; duration:120 } }`, animManager);
            anim.animations[0].target = tile;
            anim.animations[1].target = tile;
            anim.onFinished.connect(function(){ pending--; if(pending===0){ console.log("runComboSuperBomb: visuals done, calling backend"); gameBoard.superBombTriggered(row,col); } });
            anim.start();
        });
    }

    // 超级 + 火箭 -> 先播合成视觉，然后调用后端 superRocketTriggered
    function runComboSuperRocket(row, col) {
        console.log("runComboSuperRocket: combo at", row, col);
        // 简单视觉：对四邻做伸缩动画
        var neigh2 = [];
        var dirs2 = [[-1,0],[1,0],[0,-1],[0,1]];
        for (var i = 0; i < dirs2.length; ++i) {
            var rr = row + dirs2[i][0]; var cc = col + dirs2[i][1];
            if (rr>=0 && rr<8 && cc>=0 && cc<8) { var t = findTile(rr,cc); if(t) neigh2.push(t); }
        }
        var pending2 = neigh2.length;
        if (pending2 === 0) { console.log("runComboSuperRocket: no neighbor visuals, calling backend"); gameBoard.superRocketTriggered(row,col); return; }
        neigh2.forEach(function(tile){
            var anim = Qt.createQmlObject(`import QtQuick 2.15; SequentialAnimation { NumberAnimation { property:"scale"; to:1.15; duration:100 } NumberAnimation { property:"scale"; to:1.0; duration:100 } }`, animManager);
            anim.animations[0].target = tile;
            anim.animations[1].target = tile;
            anim.onFinished.connect(function(){ pending2--; if(pending2===0){ console.log("runComboSuperRocket: visuals done, calling backend"); gameBoard.superRocketTriggered(row,col); } });
            anim.start();
        });
    }

    // ===== 组合道具动画 =====
    // 超级 + 超级 -> 两次全图涟漪清除（两次都触发后端清盘），并带白色冲击波
    function runComboSuperSuper(row, col) {
        console.log("runComboSuperSuper: at", row, col);
        var centerTile = findTile(row, col);
        var cx = centerTile ? centerTile.x + centerTile.width/2 : boardView.x + boardView.width/2;
        var cy = centerTile ? centerTile.y + centerTile.height/2 : boardView.y + boardView.height/2;

        // 生成涟漪与白色冲击波，不使用未绑定标识符
        function spawnRipplePack(cx, cy, maxSize, duration) {
            var rippleItem = Qt.createQmlObject('\n                import QtQuick 2.15\n                Item { width: parent ? parent.width : 0; height: parent ? parent.height : 0 }\n            ', boardView);
            // 主涟漪圈（圆形）
            var circle = Qt.createQmlObject('\n                import QtQuick 2.15\n                Rectangle { color: "transparent"; border.width: 3; border.color: "#60FFFFFF" }\n            ', rippleItem);
            circle.width = 0; circle.height = 0;
            circle.radius = Qt.binding(function(){ return circle.width/2; });
            // 关键：x/y 绑定到中心坐标，随宽高变化保持圆心在(cx,cy)
            circle.x = Qt.binding(function(){ return cx - circle.width/2; });
            circle.y = Qt.binding(function(){ return cy - circle.height/2; });
            var anim = Qt.createQmlObject('import QtQuick 2.15; ParallelAnimation { ' +
                'NumberAnimation { property: "width"; to: ' + maxSize + '; duration: ' + duration + '; easing.type: Easing.OutCubic } ' +
                'NumberAnimation { property: "height"; to: ' + maxSize + '; duration: ' + duration + '; easing.type: Easing.OutCubic } ' +
                'NumberAnimation { property: "opacity"; from: 1; to: 0; duration: ' + duration + '; easing.type: Easing.OutQuad } ' +
            '}', rippleItem);
            anim.animations[0].target = circle;
            anim.animations[1].target = circle;
            anim.animations[2].target = circle;
            anim.onFinished.connect(function(){ rippleItem.destroy(); });
            anim.start();

            // 白色冲击波（圆形）
            var shock = Qt.createQmlObject('\n                import QtQuick 2.15\n                Rectangle { color: "transparent"; border.width: 6; border.color: "#B0FFFFFF" }\n            ', rippleItem);
            shock.width = 0; shock.height = 0;
            shock.radius = Qt.binding(function(){ return shock.width/2; });
            // 同步绑定中心
            shock.x = Qt.binding(function(){ return cx - shock.width/2; });
            shock.y = Qt.binding(function(){ return cy - shock.height/2; });
            var sdur = Math.floor(duration*0.6);
            var ssize = Math.floor(maxSize*0.8);
            var anim2 = Qt.createQmlObject('import QtQuick 2.15; ParallelAnimation { ' +
                'NumberAnimation { property: "width"; to: ' + ssize + '; duration: ' + sdur + '; easing.type: Easing.OutCubic } ' +
                'NumberAnimation { property: "height"; to: ' + ssize + '; duration: ' + sdur + '; easing.type: Easing.OutCubic } ' +
                'NumberAnimation { property: "opacity"; from: 1; to: 0; duration: ' + sdur + '; easing.type: Easing.OutQuad } ' +
            '}', rippleItem);
            anim2.animations[0].target = shock;
            anim2.animations[1].target = shock;
            anim2.animations[2].target = shock;
            anim2.start();
        }

        // 打击波
        function wavePunch(durationBase, scaleTo) {
            var tiles = [];
            for (var r = 0; r < 8; r++) {
                for (var c = 0; c < 8; c++) {
                    var t = findTile(r, c);
                    if (t) tiles.push(t);
                }
            }
            var centerRX = row;
            var centerCX = col;
            tiles.forEach(function(t){
                var dr = Math.abs(t.row - centerRX);
                var dc = Math.abs(t.col - centerCX);
                var ring = Math.max(dr, dc);
                var delay = ring * 40;
                var seq = Qt.createQmlObject('import QtQuick 2.15; SequentialAnimation { ' +
                    'PauseAnimation { duration: ' + delay + ' } ' +
                    'ParallelAnimation { NumberAnimation { property: "scale"; to: ' + scaleTo + '; duration: ' + Math.floor(durationBase/2) + '; easing.type: Easing.OutBack } ' +
                    'NumberAnimation { property: "opacity"; to: 0.7; duration: ' + Math.floor(durationBase/2) + '; } } ' +
                    'ParallelAnimation { NumberAnimation { property: "scale"; to: 1.0; duration: ' + Math.floor(durationBase/2) + '; easing.type: Easing.InBack } ' +
                    'NumberAnimation { property: "opacity"; to: 0; duration: ' + Math.floor(durationBase/2) + '; } } ' +
                '}', animManager);
                seq.animations[1].animations[0].target = t;
                seq.animations[1].animations[1].target = t;
                seq.animations[2].animations[0].target = t;
                seq.animations[2].animations[1].target = t;
                seq.onFinished.connect(function(){ t.opacity = 1.0; });
                seq.start();
            });
        }

        // 第一次涟漪 + 打击感动画 + 后端清盘
        var maxSize = Math.max(boardView.width, boardView.height);
        spawnRipplePack(cx, cy, maxSize, 600);
        wavePunch(300, 1.25);
        var t1 = Qt.createQmlObject('import QtQuick 2.15; Timer { interval: 620; repeat: false }', animManager);
        t1.triggered.connect(function(){ gameBoard.superSuperTriggered(row, col); });
        t1.start();

        // 第二次涟漪 + 打击感动画 + 再次后端清盘（实现两轮全图消除）
        var t2 = Qt.createQmlObject('import QtQuick 2.15; Timer { interval: 1200; repeat: false }', animManager);
        t2.triggered.connect(function(){ spawnRipplePack(cx, cy, maxSize, 600); wavePunch(300, 1.25); gameBoard.superSuperTriggered(row, col); });
        t2.start();
    }

    // 通用：在指定棋盘坐标显示一次性 GIF，并在 durationMs 后销毁
    // opts: { tileSpan: number, width: px, height: px, offsetX: px, offsetY: px }
    function showGifAt(row, col, source, durationMs, opts) {
        if (!boardView) return;
        var tile = findTile(row, col);
        var cell = boardView.cellSize || animManager.cellSize;
        var localCx = tile ? (tile.x + tile.width/2) : (boardView.offsetX + col * cell + cell/2);
        var localCy = tile ? (tile.y + tile.height/2) : (boardView.offsetY + row * cell + cell/2);

        // 将棋盘内坐标映射到 animManager 全局坐标，消除顶部信息区偏移影响
        var mapped = boardView.mapToItem(animManager, localCx, localCy);
        var cx = mapped.x;
        var cy = mapped.y;

        var w, h;
        if (opts && (opts.width || opts.height)) {
            w = opts.width || (opts.height || cell);
            h = opts.height || w;
        } else if (opts && opts.tileSpan) {
            w = cell * opts.tileSpan;
            h = cell * opts.tileSpan;
        } else {
            // 根据资源自动放大默认尺寸（不改路径，仅根据名字判断）
            var srcStr = String(source);
            if (srcStr.indexOf('smallbomb') !== -1) {
                w = cell * 5.5; h = w;
            } else if (srcStr.indexOf('hugebomb') !== -1) {
                w = cell * 9.5; h = w;
            } else {
                w = cell * 1.2; h = w;
            }
        }
        var ox = (opts && opts.offsetX) ? opts.offsetX : 0;
        var oy = (opts && opts.offsetY) ? opts.offsetY : 0;
        var x = cx - w/2 + ox;
        var y = cy - h/2 + oy;

        var gif = Qt.createQmlObject(
            'import QtQuick 2.15; AnimatedImage { ' +
            '    x: ' + x + '; y: ' + y + '; width: ' + w + '; height: ' + h + '; ' +
            '    z: 9999; source: "' + source + '"; playing: true; cache: false; ' +
            '    fillMode: Image.PreserveAspectFit; smooth: true; ' +
            '}', animManager);

        var killer = Qt.createQmlObject('import QtQuick 2.15; Timer { interval: ' + durationMs + '; repeat: false }', animManager);
        killer.triggered.connect(function(){ if (gif) gif.destroy(); });
        killer.start();
        return gif;
    }

}
