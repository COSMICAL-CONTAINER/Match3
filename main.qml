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

                Grid {
                    id: boardGrid
                    anchors.fill: parent
                    anchors.margins: 10
                    columns: 8
                    rows: 8
                    spacing: 2

                    Repeater {
                        id: boardRepeater
                        model: 64

                        Rectangle {
                            id: tileRect
                            width: cellSize - 2
                            height: cellSize - 2
                            radius: 8
                            border.width: 2
                            border.color: Qt.lighter(color, 1.2)
                            property real offsetX: 0
                            property real offsetY: 0
                            x: col * cellSize + offsetX
                            y: row * cellSize + offsetY

                            property int row: Math.floor(index / 8)
                            property int col: index % 8
                            property string tileColor: gameBoard ? gameBoard.tileAt(row, col) : "gray"
                            property bool isMatched: false
                            color: tileColor

                            // 注册到动画管理器
                            Component.onCompleted: {
                                if (animManager) {
                                    animManager.tileMap[row + "_" + col] = tileRect;
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

                                // onClicked: {
                                //     // 简化交互：点击相邻方块进行交换
                                //     // 实际游戏中应该实现拖拽逻辑
                                //     console.log("Tile clicked:", row, col);
                                // }

                                onPositionChanged: function(mouse) {
                                    if (!dragging) return;

                                    // 计算拖动距离
                                    var dx = mouse.x - mouseArea.width/2
                                    var dy = mouse.y - mouseArea.height/2

                                    // 这里你可以用阈值判断方向
                                    if (Math.abs(dx) > cellSize/2 || Math.abs(dy) > cellSize/2) {
                                        var targetRow = startRow + (Math.abs(dy) > Math.abs(dx) ? (dy>0?1:-1) : 0)
                                        var targetCol = startCol + (Math.abs(dx) > Math.abs(dy) ? (dx>0?1:-1) : 0)

                                        console.log("Drag from", startRow, startCol, "to", targetRow, targetCol)

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

        // 连接游戏板信号
        Connections {
            target: gameBoard
            enabled: gameBoard !== undefined

            function onBoardChanged() {
                // 刷新所有tile的颜色
                for (var i = 0; i < 64; i++) {
                    var item = boardRepeater.itemAt(i);
                    if (item) {
                        item.tileColor = gameBoard.tileAt(item.row, item.col);
                    }
                }
            }

            function onInvalidSwap(r1, c1, r2, c2) {
                console.log("无效交换:", r1, c1, r2, c2);
                // 播放抖动动画
                var tile1 = animManager.findTile(r1, c1);
                var tile2 = animManager.findTile(r2, c2);
                if (tile1) animManager.shakeTile(tile1);
                if (tile2) animManager.shakeTile(tile2);
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
