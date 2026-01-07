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
    function enqueueDrops(dropPaths){ dropQueue.push(dropPaths); runNext(); }

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
        if(!dropPaths||dropPaths.length===0){ busy=false; runNext(); return; }
        var pending=0;
        dropPaths.forEach(function(path){
            for(var j=path.length-1;j>0;j--){
                var from=path[j-1], to=path[j];
                var tileFrom=findTile(from.x,from.y), tileTo=findTile(to.x,to.y);
                if(!tileFrom||!tileTo) continue;
                tileTo.tileColor=tileFrom.tileColor;
                tileFrom.tileColor="transparent";

                var anim=Qt.createQmlObject(`
                    import QtQuick 2.15
                    SequentialAnimation {
                        NumberAnimation { property:"scale"; from:1.2; to:1.0; duration:150; easing.type:Easing.OutCubic }
                        NumberAnimation { property:"opacity"; from:0.6; to:1.0; duration:150 }
                    }
                `, animManager);
                anim.animations[0].target=tileTo;
                anim.animations[1].target=tileTo;

                pending++;
                (function(a){ a.onFinished.connect(function(){ pending--; if(pending===0){ busy=false; runNext(); } }); a.start(); })(anim);
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
        if(pending===0){ refreshBoardColors(); busy=false; runNext(); }
    }

    function shakeTile(tile){
        if(!tile) return;
        var origX=tile.x;
        var anim=Qt.createQmlObject(`
            import QtQuick 2.15
            SequentialAnimation {
                property var targetTile:null
                property real origX:0
                NumberAnimation { target:targetTile; property:"x"; to:origX+5; duration:50 }
                NumberAnimation { target:targetTile; property:"x"; to:origX-5; duration:50 }
                NumberAnimation { target:targetTile; property:"x"; to:origX; duration:50 }
            }
        `, animManager);
        anim.targetTile=tile;
        anim.origX=origX;
        anim.start();
    }

    // 处理火箭动画
    function runRocketEffect(row, col, type) {
        var tile = findTile(row, col);
        console.log("Tile found for position: " + row + ", " + col);
        console.log("Tile: ", tile);  // 调试 tile 对象

        if (tile) {
            console.log("Creating animation for tile: ", tile);

            var anim;

            if (type === 1) { // 纵向火箭
                anim = Qt.createQmlObject(`
                    import QtQuick 2.15
                    SequentialAnimation {
                        ParallelAnimation {
                            NumberAnimation { property: "scale"; from: 1; to: 1.2; duration: 150; easing.type: Easing.OutQuad }
                            NumberAnimation { property: "opacity"; from: 1; to: 0.5; duration: 150; easing.type: Easing.OutQuad }
                        }
                        ParallelAnimation {
                            NumberAnimation { property: "scale"; from: 1.2; to: 1.5; duration: 200; easing.type: Easing.OutElastic }
                            NumberAnimation { property: "opacity"; from: 0.5; to: 1; duration: 200; easing.type: Easing.InQuad }
                        }
                        ParallelAnimation {
                            NumberAnimation { property: "scale"; from: 1.5; to: 1.0; duration: 150; easing.type: Easing.InQuad }
                            NumberAnimation { property: "opacity"; from: 1; to: 1; duration: 150; easing.type: Easing.InQuad }
                        }
                    }
                `, animManager);
            } else if (type === 2) { // 横向火箭
                anim = Qt.createQmlObject(`
                    import QtQuick 2.15
                    SequentialAnimation {
                        ParallelAnimation {
                            NumberAnimation { property: "scale"; from: 1; to: 1.2; duration: 150; easing.type: Easing.OutQuad }
                            NumberAnimation { property: "opacity"; from: 1; to: 0.5; duration: 150; easing.type: Easing.OutQuad }
                        }
                        ParallelAnimation {
                            NumberAnimation { property: "scale"; from: 1.2; to: 1.5; duration: 200; easing.type: Easing.OutElastic }
                            NumberAnimation { property: "opacity"; from: 0.5; to: 1; duration: 200; easing.type: Easing.InQuad }
                        }
                        ParallelAnimation {
                            NumberAnimation { property: "scale"; from: 1.5; to: 1.0; duration: 150; easing.type: Easing.InQuad }
                            NumberAnimation { property: "opacity"; from: 1; to: 1; duration: 150; easing.type: Easing.InQuad }
                        }
                    }
                `, animManager);
            }

            // 确保动画的目标是正确的 `tile` 对象
            anim.animations[0].animations[0].target = tile;
            anim.animations[0].animations[1].target = tile;
            anim.animations[1].animations[0].target = tile;
            anim.animations[1].animations[1].target = tile;
            anim.animations[2].animations[0].target = tile;
            anim.animations[2].animations[1].target = tile;

            // 动画完成后的回调
            anim.onFinished.connect(function() {
                console.log("Rocket animation finished, clearing row or column");
                var matchedTiles = []
                if (type === 1) { // 纵向火箭消除
                    // 纵向火箭消除整列
                    for (var r = 0; r < 8; r++) {
                        matchedTiles.push({x: r, y: col}); // 添加该列的所有方块
                    }
                } else if (type === 2) { // 横向火箭消除
                    for (var c = 0; c < 8; c++) {
                        matchedTiles.push({x: row, y: c}); // 添加该行的所有方块
                    }
                }
                enqueueMatches(matchedTiles)
                gameBoard.rocketEffectTriggered(row, col, type);
                gameBoard.processDrop()
            });

            // 启动动画
            anim.start();
        } else {
            console.error("Tile not found for position: " + row + ", " + col);
        }
    }


    // 处理炸弹动画
    function runBombEffect(row, col) {
        var tile = findTile(row, col);

        // 半径为 2 的圆形区域
        var radius = 2;
        // 收集炸弹影响范围内的所有匹配方块
        var matchedTiles = [];

        if (tile) {
            // 播放炸弹爆炸动画（缩放、消失）
            var anim = Qt.createQmlObject(`
                import QtQuick 2.15
                SequentialAnimation {
                    ParallelAnimation {
                        NumberAnimation { property: "scale"; from: 1; to: 2; duration: 200; easing.type: Easing.OutQuad;}
                        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 200; easing.type: Easing.OutQuad;}
                    }
                }
            `, animManager);
            anim.animations[0].animations[0].target = tile;
            anim.animations[0].animations[1].target = tile;
            // 遍历炸弹范围内的所有方块
            for (var r = row - radius; r <= row + radius; ++r) {
                for (var c = col - radius; c <= col + radius; ++c) {
                    // 检查是否越界
                    if (r >= 0 && r < 8 && c >= 0 && c < 8) {
                        // 计算当前点到炸弹中心的距离，如果在半径范围内，收集该点
                        if (Math.sqrt(Math.pow(r - row, 2) + Math.pow(c - col, 2)) <= radius) {
                            matchedTiles.push({ x: r, y: c });
                        }
                    }
                }
            }
            // 启动动画
            anim.start();

            enqueueMatches(matchedTiles)

            // 触发炸弹效果
            gameBoard.bombEffectTriggered(row, col);
            gameBoard.processDrop()
        } else {
            console.log("Tile not found for position:", row, col); // 调试输出，确保tile能被找到
        }
    }

    // 超级道具
    function runSuperItemEffect(row, col, color) {
        console.log("Super Item Effect triggered at position:", row, col, "with color:", color);

        // 找到所有颜色为 'color' 的方块
        var matchedTiles = [];

        // 遍历整个棋盘，寻找与传入颜色相同的方块
        for (var r = 0; r < 8; r++) {
            for (var c = 0; c < 8; c++) {
                if (gameBoard.tileAt(r, c) === color) {
                    matchedTiles.push({x: r, y: c});  // 将符合颜色的方块加入到 matchedTiles 数组
                }
            }
        }

        // 如果找到了符合条件的方块，执行消除动画
        if (matchedTiles.length > 0) {
            console.log("Found " + matchedTiles.length + " tiles with color " + color);

            // 播放消除动画
            matchedTiles.forEach(function(pt) {
                var tile = findTile(pt.x, pt.y);
                if (tile) {
                    console.log("Creating animation for tile:", tile);

                    var anim = Qt.createQmlObject(`
                        import QtQuick 2.15
                        SequentialAnimation {
                            ParallelAnimation {
                                NumberAnimation { property: "scale"; from: 1; to: 1.2; duration: 150; easing.type: Easing.OutQuad }
                                NumberAnimation { property: "opacity"; from: 1; to: 0.5; duration: 150; easing.type: Easing.OutQuad }
                            }
                            ParallelAnimation {
                                NumberAnimation { property: "scale"; from: 1.2; to: 1.5; duration: 200; easing.type: Easing.OutElastic }
                                NumberAnimation { property: "opacity"; from: 0.5; to: 1; duration: 200; easing.type: Easing.InQuad }
                            }
                            ParallelAnimation {
                                NumberAnimation { property: "scale"; from: 1.5; to: 1.0; duration: 150; easing.type: Easing.InQuad }
                                NumberAnimation { property: "opacity"; from: 1; to: 1; duration: 150; easing.type: Easing.InQuad }
                            }
                        }
                    `, animManager);

                    anim.animations[0].animations[0].target = tile;
                    anim.animations[0].animations[1].target = tile;
                    anim.animations[1].animations[0].target = tile;
                    anim.animations[1].animations[1].target = tile;
                    anim.animations[2].animations[0].target = tile;
                    anim.animations[2].animations[1].target = tile;

                    anim.start();
                }
            });

            // 执行消除操作
            enqueueMatches(matchedTiles);
            gameBoard.superItemEffectTriggered(row, col, color);
            gameBoard.processDrop();
        } else {
            console.log("No matching tiles found for color: " + color);
        }
    }


}
