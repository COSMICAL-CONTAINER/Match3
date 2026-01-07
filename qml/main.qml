import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

ApplicationWindow {
    id: mainWindow
    visible: true
    width: 500
    height: 600
    title: "开心消消乐"

    property int cellSize: Math.min(width, height * 0.7) / 8

    // 使用正确的上下文属性名
    property var gameBoard: gameBoardCpp

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

            // 游戏区域 - 简化实现
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

                        property string tileColor: gameBoard ? gameBoard.tileAt(row, col) : "gray"
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
                                    return "qrc:/image/Rocket_1.png";
                                } else if (tileColor == "Rocket_2") {
                                    return "qrc:/image/Rocket_2.png";
                                } else if (tileColor == "Bomb") {
                                    return "qrc:/image/Bomb.png";
                                } else if (tileColor == "SuperItem") {
                                    return "qrc:/image/SuperItem.png";
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
                gameBoard.processMatches()
            }
        }

        // 连接游戏板信号
        Connections {
            target: gameBoard
            enabled: gameBoard !== undefined

            function onMatchAnimationRequested(matches) {
                console.log("播放匹配动画:", matches)
                // 播放完动画后调用
                animTimer.start()
            }
            function onBoardChanged() {
                // 刷新所有tile的颜色
                for (var i = 0; i < 64; i++) {
                    var item = boardRepeater.itemAt(i);
                    if (item) {
                        item.tileColor = gameBoard.tileAt(item.row, item.col);
                    }
                }
            }
            function onComboChanged(comboCount) {
                // 当 C++ 层发送 comboChanged 信号时，更新 QML 层的连击数
                // console.log("comboChanged:", comboCount);
                comboText.text = "连击 x" + comboCount;
                comboAnimation.start();
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
                    console.log("播放火箭动画: ", row, col);
                    animManager.runRocketEffect(row, col, 1);
                } else if (type === 2) {
                    console.log("播放火箭动画: ", row, col);
                    animManager.runRocketEffect(row, col, 2);
                } else if (type === 3) {
                    console.log("播放炸弹动画: ", row, col);
                    animManager.runBombEffect(row, col);
                } else if (type === 4) {
                    console.log("播放超级道具动画: ", row, col);
                    animManager.runSuperItemEffect(row, col, color);
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
}
