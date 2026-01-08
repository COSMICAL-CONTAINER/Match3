import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtMultimedia

ApplicationWindow {
    id: mainWindow
    visible: true
    width: 500
    height: 600
    title: "开心消消乐"

    property int cellSize: Math.min(width, height * 0.7) / 8

    // 使用正确的上下文属性名
    property var gameBoard: gameBoardCpp

    // 音效接口：可覆盖默认映射或统一设置为 comboSfxOverride
    property url comboSfxOverride: ""   // 若设置则统一播放该音效
    property real comboSfxVolume: 0.8

    // 替换单一 MediaPlayer 为播放器池，避免连续播放被打断
    property int comboPlayerIndex: 0

    // 播放器池（4 路轮询），为避免后端不能混音，改为每个播放器独立 AudioOutput
    property real _comboOutVol: comboSfxVolume
    AudioOutput { id: comboOut1; volume: mainWindow._comboOutVol }
    AudioOutput { id: comboOut2; volume: mainWindow._comboOutVol }
    AudioOutput { id: comboOut3; volume: mainWindow._comboOutVol }
    AudioOutput { id: comboOut4; volume: mainWindow._comboOutVol }
    MediaPlayer { id: comboPlayer1; audioOutput: comboOut1 }
    MediaPlayer { id: comboPlayer2; audioOutput: comboOut2 }
    MediaPlayer { id: comboPlayer3; audioOutput: comboOut3 }
    MediaPlayer { id: comboPlayer4; audioOutput: comboOut4 }

    function getIdleComboPlayer() {
        // 优先选择空闲播放器；若都在播放则轮询下一个
        var players = [comboPlayer1, comboPlayer2, comboPlayer3, comboPlayer4];
        for (var i = 0; i < players.length; i++) {
            if (players[i].playbackState !== MediaPlayer.PlayingState) return players[i];
        }
        // 都在播放，则取轮询索引
        comboPlayerIndex = (comboPlayerIndex + 1) % players.length;
        return players[comboPlayerIndex];
    }

    function playComboSfx(url) {
        if (!url || String(url).length === 0) return;
        var p = getIdleComboPlayer();
        // 先停止并重置到开头，确保快速连播不受上次播放位置影响
        p.stop();
        p.source = url;
        p.position = 0;
        p.play();
    }

    // 根据连击返回默认音效路径（可按需修改映射）
    function getComboSfx(count) {
        if (mainWindow.comboSfxOverride && String(mainWindow.comboSfxOverride).length > 0) return mainWindow.comboSfxOverride;
        if (count === 2) return "qrc:/music/Good.wav";
        if (count === 3) return "qrc:/music/Great.wav";
        if (count === 4) return "qrc:/music/Excellent.wav";
        if (count === 5) return "qrc:/music/Amazing.wav";
        if (count >= 6) return "qrc:/music/Unbelievable.wav";
        return "";
    }

    // Combo 数字显示区
    Rectangle {
        id: comboDisplay
        width: parent.width
        height: 100
        color: "transparent"  // 保持透明背景

        anchors.top: parent.top
        z: 2  // 通过设置 z 值让它位于其他组件之上

        Text {
            id: comboText
            anchors.centerIn: parent
            font.pixelSize: 50
            color: "white"
            text: "连击 x" + (gameBoard ? gameBoard.comboCount : 0)
            font.bold: true
            opacity: 0 // 初始时隐藏

            // 透明度动画，显示时渐变
            SequentialAnimation {
                id: comboAnimation
                running: gameBoard.comboCount > 1  // 只有连击数大于 1 时，才启动动画
                NumberAnimation {
                    target: comboText
                    property: "opacity"
                    from: 0
                    to: 1
                    duration: 500
                    easing.type: Easing.OutQuad
                }

                SequentialAnimation {
                    ParallelAnimation {
                        NumberAnimation { property: "scale"; to: 1.5; duration: 300; easing.type: Easing.OutElastic }
                        ColorAnimation { target: comboText; property: "color"; to: "yellow"; duration: 300 }
                    }
                    ParallelAnimation {
                        NumberAnimation { property: "scale"; to: 1.0; duration: 300; easing.type: Easing.InElastic }
                        ColorAnimation { target: comboText; property: "color"; to: "white"; duration: 300 }
                    }
                }
            }

            // 定时器，2秒后隐藏文本
            Timer {
                id: hideTimer
                interval: 2000  // 设置为2秒
                running: false
                repeat: false
                onTriggered: {
                    // 使用动画让文本渐变消失
                    var fadeOut = Qt.createQmlObject('import QtQuick 2.15; NumberAnimation { target: comboText; property: "opacity"; from: 1; to: 0; duration: 1000; easing.type: Easing.OutQuad }', comboDisplay);
                    fadeOut.start();  // 启动渐变动画
                }
            }

            // 每次动画结束后启动定时器
            onOpacityChanged: {
                if (comboText.opacity === 1) {
                    hideTimer.start()  // 启动定时器
                }
            }
        }

        // 新增：连击图片显示（带闪光背景）
        Item {
            id: comboImageLayer
            anchors.fill: parent
            opacity: 0
            visible: false
            z: 3

            // 背景闪光（放大、右偏、轻微旋转）
            Image {
                id: shiningBg
                anchors.centerIn: parent
                anchors.horizontalCenterOffset: parent.width * 0.18
                width: parent.width * 0.75
                height: parent.height * 1.0
                rotation: 8
                fillMode: Image.PreserveAspectFit
                source: "qrc:/image/ui/Shining.png"
                opacity: 0.85
            }
            // 前景文案图片（放大、右偏、顺时针旋转）
            Image {
                id: comboBadge
                anchors.centerIn: parent
                anchors.horizontalCenterOffset: parent.width * 0.18
                width: parent.width * 0.62
                height: parent.height * 0.82
                rotation: 12
                fillMode: Image.PreserveAspectFit
                source: {
                    var cc = (gameBoard ? gameBoard.comboCount : 0);
                    if (cc === 2) return "qrc:/image/ui/Good.png";
                    if (cc === 3) return "qrc:/image/ui/Great.png";
                    if (cc === 4) return "qrc:/image/ui/Excellent.png";
                    if (cc === 5) return "qrc:/image/ui/Amazing.png";
                    if (cc >= 6) return "qrc:/image/ui/Unbelievable.png";
                    return "";
                }
            }

            // 显示淡入
            SequentialAnimation {
                id: comboImageAnim
                running: false
                NumberAnimation { target: comboImageLayer; property: "opacity"; from: 0; to: 1; duration: 250; easing.type: Easing.OutCubic }
            }

            // 2秒后自动淡出隐藏
            Timer {
                id: comboImageHideTimer
                interval: 2000
                repeat: false
                onTriggered: {
                    var fadeOut = Qt.createQmlObject('import QtQuick 2.15; NumberAnimation { target: comboImageLayer; property: "opacity"; from: 1; to: 0; duration: 500; easing.type: Easing.OutQuad }', comboDisplay);
                    fadeOut.onFinished.connect(function(){ comboImageLayer.visible = false; });
                    fadeOut.start();
                }
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#4A90E2" }
            GradientStop { position: 1.0; color: "#7B68EE" }
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 10

            // 标题栏
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 60
                color: "transparent"
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    Text {
                        text: "开心消消乐"
                        font.pixelSize: 24
                        font.bold: true
                        color: "white"
                        Layout.alignment: Qt.AlignLeft
                    }
                    Rectangle {
                        Layout.alignment: Qt.AlignRight
                        width: 100; height: 40; radius: 20; color: "#FF6B6B"
                        Text {
                            anchors.centerIn: parent
                            text: "分数: " + (gameBoard ? gameBoard.score : 0)
                            font.pixelSize: 16
                            color: "white"
                            font.bold: true
                        }
                    }
                }
            }

            // 游戏区域
            Rectangle {
                id: gameArea
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.margins: 10
                color: "#2C3E50"
                radius: 15
                border.color: "#34495E"
                border.width: 3

                property int rows: 8
                property int cols: 8
                property real cellSize: Math.min(width / cols, height / rows)   // 格子尽量正方形
                property real boardWidth: cols * cellSize
                property real boardHeight: rows * cellSize
                property real offsetX: (width - boardWidth) / 2
                property real offsetY: (height - boardHeight) / 2

                Repeater {
                    id: boardRepeater
                    model: gameArea.rows * gameArea.cols

                    Rectangle {
                        id: tileRect
                        width: gameArea.cellSize - 2
                        height: gameArea.cellSize - 2
                        radius: 8
                        border.width: 2
                        border.color: Qt.lighter(color, 1.2)

                        property int row: Math.floor(index / gameArea.cols)
                        property int col: index % gameArea.cols
                        property real offsetX: 0
                        property real offsetY: 0

                        x: gameArea.offsetX + col * gameArea.cellSize + offsetX
                        y: gameArea.offsetY + row * gameArea.cellSize + offsetY

                        property string tileColor: (gameBoard ? (gameBoard.tileAt(row, col) === "" ? "transparent" : gameBoard.tileAt(row, col)) : "gray")
                        property bool isMatched: false
                        color: tileColor
                        // 注册到动画管理器
                        Component.onCompleted: {
                            if (animManager) {
                                animManager.tileMap[row + "_" + col] = tileRect;
                            }
                        }
                        Image {
                            anchors.centerIn: parent
                            width: parent.width * 0.8
                            height: parent.height * 0.8
                            fillMode: Image.PreserveAspectFit
                            source: {
                                if (tileColor == "Rocket_1") {
                                    return "qrc:/image/item/Rocket_1.png";
                                } else if (tileColor == "Rocket_2") {
                                    return "qrc:/image/item/Rocket_2.png";
                                } else if (tileColor == "Bomb") {
                                    return "qrc:/image/item/Bomb.png";
                                } else if (tileColor == "SuperItem") {
                                    return "qrc:/image/item/SuperItem.png";
                                } else {
                                    return ""; // 默认值为空
                                }
                            }
                        }
                        MouseArea {
                            id: mouseArea
                            anchors.fill: parent

                            property int startRow
                            property int startCol
                            property bool dragging: false

                            onPressed: function(mouse) {
                                dragging = true
                                startRow = tileRect.row
                                startCol = tileRect.col
                                tileRect.scale = 0.9
                            }

                            onReleased: function(mouse) {
                                tileRect.scale = 1.0
                                dragging = false
                            }

                            onClicked: {
                                var tileColor = gameBoard.tileAt(row, col)
                                console.log("Tile clicked:", row, col, "color: ", tileColor);
                            }
                            onDoubleClicked:{
                                // 双击激活道具
                                var tileColor = gameBoard.tileAt(row, col)
                                if (tileColor === "Rocket_1") {
                                    console.log("播放火箭动画: ", row, col);
                                    animManager.runRocketEffect(row, col, 1);
                                } else if (tileColor === "Rocket_2") {
                                    console.log("播放火箭动画: ", row, col);
                                    animManager.runRocketEffect(row, col, 2);
                                } else if (tileColor === "Bomb") {
                                    console.log("播放炸弹动画: ", row, col);
                                    animManager.runBombEffect(row, col);
                                } else if (tileColor === "SuperItem") {
                                    console.log("播放超级道具动画: ", row, col);
                                    // 需要统计此时场上最多颜色的方块是啥
                                    var colorCount = {};  // 用于记录颜色出现的次数
                                    var mostFrequentColor = ""; // 用于存储出现次数最多的颜色

                                    // 遍历整个棋盘，统计每种颜色的出现次数
                                    for (var r = 0; r < 8; r++) {
                                        for (var c = 0; c < 8; c++) {
                                            var color = gameBoard.tileAt(r, c);
                                            if (color) {
                                                colorCount[color] = (colorCount[color] || 0) + 1; // 增加该颜色的计数
                                            }
                                        }
                                    }

                                    // 找到出现次数最多的颜色
                                    mostFrequentColor = Object.keys(colorCount).reduce(function(a, b) {
                                        return colorCount[a] > colorCount[b] ? a : b;
                                    });

                                    animManager.runSuperItemEffect(row, col, mostFrequentColor);
                                }
                            }
                            onPositionChanged: function(mouse) {
                                if (!dragging) return;

                                // 计算拖动距离
                                var dx = mouse.x - mouseArea.width/2
                                var dy = mouse.y - mouseArea.height/2

                                // 这里你可以用阈值判断方向
                                if (Math.abs(dx) > cellSize/2 || Math.abs(dy) > cellSize/2) {
                                    var targetRow = startRow + (Math.abs(dy) > Math.abs(dx) ? (dy>0?1:-1) : 0)
                                    var targetCol = startCol + (Math.abs(dx) > Math.abs(dy) ? (dx>0?1:-1) : 0)

                                    // console.log("Drag from", startRow, startCol, "to", targetRow, targetCol)

                                    if (gameBoard) gameBoard.trySwap(startRow, startCol, targetRow, targetCol)

                                    dragging = false  // 只处理一次拖动
                                }
                            }
                        }

                        // 匹配动画
                        Behavior on opacity {
                            NumberAnimation { duration: 300 }
                        }

                        Behavior on scale {
                            NumberAnimation { duration: 200 }
                        }
                    }
                }
            }

            // 控制按钮
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 80
                color: "transparent"
                Row {
                    anchors.centerIn: parent
                    spacing: 20
                    Button {
                        text: "开始游戏"
                        onClicked: if (gameBoard) gameBoard.startGame()
                    }
                    Button {
                        text: "重新洗牌"
                        onClicked: if (gameBoard) gameBoard.shuffleBoard()
                    }
                    Button {
                        text: "重新开始"
                        onClicked: if (gameBoard) gameBoard.resetGame()
                    }
                }
            }
        }

        Timer {
            id: animTimer
            interval: 400   // 匹配动画时长
            repeat: false
            onTriggered: {
                // 仅处理匹配，不在此处播放音效，避免重复触发
                gameBoard.processMatches()
            }
        }

        // 连接游戏板信号
        Connections {
            target: gameBoard
            enabled: gameBoard !== undefined

            function onMatchAnimationRequested(matches) {
                console.log("播放匹配动画:", matches)
                if (simpleSfx && simpleSfx.playMatch) simpleSfx.playMatch();
                animTimer.start()
            }
            function onBoardChanged() {
                // 刷新所有tile的颜色并重置视觉状态
                for (var i = 0; i < 64; i++) {
                    var item = boardRepeater.itemAt(i);
                    if (item) {
                        var newColor = gameBoard.tileAt(item.row, item.col);
                        if (newColor === "") newColor = "transparent";
                        item.tileColor = newColor;
                        // 重置可能残留的动画状态，确保视觉与数据一致
                        item.opacity = 1.0;
                        item.scale = 1.0;
                        item.offsetX = 0;
                        item.offsetY = 0;
                    }
                }
            }
            function onComboChanged(comboCount) {
                // 当 C++ 层发送 comboChanged 信号时，更新 QML 层的连击数
                // console.log("comboChanged:", comboCount);
                comboText.text = "连击 x" + comboCount;
                comboAnimation.start();

                // 同步显示图片层
                if (comboCount >= 2) {
                    comboImageLayer.visible = true;
                    comboImageLayer.opacity = 0;
                    comboImageAnim.start();
                    comboImageHideTimer.start();
                    // 播放音效（使用播放器池避免打断）
                    var sfx = getComboSfx(comboCount);
                    playComboSfx(sfx);
                }
            }

            function onInvalidSwap(r1, c1, r2, c2) {
                // console.log("无效交换:", r1, c1, r2, c2);
                // 播放抖动动画
                var tile1 = animManager.findTile(r1, c1);
                var tile2 = animManager.findTile(r2, c2);
                if (tile1) animManager.shakeTile(tile1);
                if (tile2) animManager.shakeTile(tile2);
            }

            function onBombCreateRequested(bombMatches) {
                // console.log("播放炸弹动画: ", bombMatches);
                // runBombEffect(bombMatches);
            }

            function onRocketCreateRequested(rocketMatches) {
                // console.log("播放火箭动画: ", rocketMatches);
                // animManager.runRocketEffect(rocketMatches);
            }

           function onSuperItemCreateRequested(superItemMatches) {
                // console.log("播放超级道具动画: ", superItemMatches);
                // runSuperItemEffect(superItemMatches);
            }

            function onPropEffect(row, col, type, color){
                if (type === 1) {
                    if (simpleSfx && simpleSfx.playPropLimited) simpleSfx.playPropLimited("rocket");
                    console.log("播放火箭动画: ", row, col);
                    animManager.runRocketEffect(row, col, 1);
                } else if (type === 2) {
                    if (simpleSfx && simpleSfx.playPropLimited) simpleSfx.playPropLimited("rocket");
                    console.log("播放火箭动画: ", row, col);
                    animManager.runRocketEffect(row, col, 2);
                } else if (type === 3) {
                    if (simpleSfx && simpleSfx.playPropLimited) simpleSfx.playPropLimited("bomb");
                    console.log("播放炸弹动画: ", row, col);
                    animManager.runBombEffect(row, col);
                } else if (type === 4) {
                    if (simpleSfx && simpleSfx.playPropLimited) simpleSfx.playPropLimited("super");
                    console.log("播放超级道具动画: ", row, col);
                    animManager.runSuperItemEffect(row, col, color);
                } else if (type === 100) { // Rocket + Rocket combo
                    if (simpleSfx && simpleSfx.playPropLimited) simpleSfx.playPropLimited("rocket");
                    console.log("播放火箭+火箭合成动画:", row, col);
                    animManager.runComboRocketRocket(row, col);
                } else if (type === 101) { // Bomb + Bomb combo
                    if (simpleSfx && simpleSfx.playPropLimited) simpleSfx.playPropLimited("bomb");
                    console.log("播放炸弹+炸弹合成动画:", row, col);
                    animManager.runComboBombBomb(row, col);
                } else if (type === 102) { // Bomb + Rocket combo
                    if (simpleSfx && simpleSfx.playPropLimited) simpleSfx.playPropLimited("bomb");
                    console.log("播放炸弹+火箭合成动画:", row, col, "meta:", color);
                    var rocketType = parseInt(color);
                    animManager.runComboBombRocket(row, col, rocketType);
                } else if (type === 103) { // Super + Bomb combo
                    if (simpleSfx && simpleSfx.playPropLimited) simpleSfx.playPropLimited("super");
                    console.log("播放超级+炸弹合成动画:", row, col);
                    animManager.runComboSuperBomb(row, col);
                } else if (type === 104) {// Super + Rocket combo
                    if (simpleSfx && simpleSfx.playPropLimited) simpleSfx.playPropLimited("super");
                    console.log("播放超级+火箭合成动画:", row, col);
                    animManager.runComboSuperRocket(row, col);
                } else if (type === 105) { // Super + Super combo
                    if (simpleSfx && simpleSfx.playPropLimited) simpleSfx.playPropLimited("super");
                    console.log("播放超级+超级合成涟漪动画:", row, col);
                    animManager.runComboSuperSuper(row, col);
                }
            }
        }
    }

    // 动画管理器
    AnimationManager {
        id: animManager
        boardView: gameArea
        gameBoard: gameBoardCpp
    }

    // 简易音效模块：新增并发上限（每 250ms 最多播放 3 次道具音效）
    Item {
        id: simpleSfx
        property real volume: 0.8
        SoundEffect { id: match1Sfx; source: "qrc:/music/match1.wav"; volume: simpleSfx.volume }
        SoundEffect { id: match2Sfx; source: "qrc:/music/match2.wav"; volume: simpleSfx.volume }
        SoundEffect { id: match3Sfx; source: "qrc:/music/match3.wav"; volume: simpleSfx.volume }
        SoundEffect { id: match4Sfx; source: "qrc:/music/match4.wav"; volume: simpleSfx.volume }
        SoundEffect { id: drop1Sfx;  source: "qrc:/music/drop1.wav"; volume: simpleSfx.volume }
        SoundEffect { id: drop2Sfx;  source: "qrc:/music/drop2.wav"; volume: simpleSfx.volume }
        SoundEffect { id: rocketSfx;  source: "qrc:/music/rocket.wav"; volume: simpleSfx.volume }
        SoundEffect { id: bombSfx;    source: "qrc:/music/Bomb.wav"; volume: simpleSfx.volume }
        SoundEffect { id: superItemSfx;  source: "qrc:/music/SuperItem.wav"; volume: simpleSfx.volume }

        property int propBurstCount: 0
        property double _burstWindowStart: 0
        property double _now: 0
        property double rocketLastTs: 0
        property double bombLastTs: 0
        property double superLastTs: 0
        property int burstWindowMs: 250
        property int kindCooldownMs: 120
        Timer {
            id: propBurstTick
            interval: 50; repeat: true; running: true
            onTriggered: simpleSfx._now = Date.now()
        }

        function _resetWindow(nowTs) {
            _burstWindowStart = nowTs; propBurstCount = 0;
        }

        function playPropLimited(kind) {
            var nowTs = (simpleSfx._now > 0 ? simpleSfx._now : Date.now());
            if (nowTs - _burstWindowStart > burstWindowMs) {
                _resetWindow(nowTs);
            }
            if (propBurstCount >= 3) return; // 全局窗口上限

            // 同类短冷却，避免同一声音快速连击
            var lastTs = 0;
            if (kind === "rocket") lastTs = rocketLastTs;
            else if (kind === "bomb") lastTs = bombLastTs;
            else if (kind === "super") lastTs = superLastTs;
            if (nowTs - lastTs < kindCooldownMs) return;

            // 适度降音防爆音
            var prevVol = simpleSfx.volume;
            if (propBurstCount >= 2) simpleSfx.volume = Math.max(0.6, prevVol * 0.8);

            // 播放
            if (kind === "rocket") { rocketSfx.play(); rocketLastTs = nowTs; }
            else if (kind === "bomb") { bombSfx.play(); bombLastTs = nowTs; }
            else if (kind === "super") { superItemSfx.play(); superLastTs = nowTs; }
            propBurstCount++;

            // 恢复音量（异步，以免影响其他 SoundEffect 混音）
            Qt.callLater(function(){ simpleSfx.volume = prevVol; });
        }

        function playMatch() {
            // 改用 MediaPlayer 池播放，避免 SoundEffect 在当前环境不出声的问题
            var r = Math.floor(Math.random() * 4) + 1;
            var url = (r === 1) ? "qrc:/music/match1.wav"
                      : (r === 2) ? "qrc:/music/match2.wav"
                      : (r === 3) ? "qrc:/music/match3.wav"
                      : "qrc:/music/match4.wav";
            if (mainWindow && mainWindow.playComboSfx) mainWindow.playComboSfx(url);
        }
    }
}
