import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15

Window {
    visible: true
    width: 400
    height: 500
    title: "Match3 Demo"

    property int rows: gameBoard.rows
    property int columns: gameBoard.columns
    property int cellSize: width / columns

    Rectangle {
        anchors.fill: parent

        Rectangle {
            id: gameArea
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: parent.width    // 保持正方形游戏区
            color: "black"
            Repeater {
                id: boardRepeater
                model: rows * columns

                Rectangle {
                    id: tileRect
                    width: cellSize - 2
                    height: cellSize - 2
                    radius: 4
                    border.width: 1
                    border.color: "gray"

                    property int row: Math.floor(index / columns)
                    property int col: index % columns
                    property int targetRow: 0 // 用于动画

                    // 颜色绑定棋盘数据
                    color: gameBoard.tileAt(row, col)

                    // 位置绑定 row/col
                    x: col * cellSize
                    y: row * cellSize

                    Behavior on y { NumberAnimation { duration: 3000; easing.type: Easing.OutBounce } }
                    Behavior on x { NumberAnimation { duration: 2000; easing.type: Easing.InOutQuad } }
                    Behavior on color { ColorAnimation { duration: 200 } }
                    // 交换动画
                    // ParallelAnimation {
                    //     id: swapAnim
                    //     PropertyAnimation { id: animX1; target: tileRect; property: "x"; duration: 120 }
                    //     PropertyAnimation { id: animY1; target: tileRect; property: "y"; duration: 120 }
                    //     PropertyAnimation { id: animX2; target: otherTile; property: "x"; duration: 120 }
                    //     PropertyAnimation { id: animY2; target: otherTile; property: "y"; duration: 120 }
                    // }

                    // 无效交换抖动动画
                    SequentialAnimation {
                        id: shakeAnim
                        NumberAnimation { target: tileRect; property: "x"; to: tileRect.x + 10; duration: 50 }
                        NumberAnimation { target: tileRect; property: "x"; to: tileRect.x - 10; duration: 50 }
                        NumberAnimation { target: tileRect; property: "x"; to: tileRect.x; duration: 50 }
                    }

                    MouseArea {
                        anchors.fill: parent
                        drag.target: null

                        property real pressX
                        property real pressY

                        onPressed: function(event) {
                            pressX = event.x
                            pressY = event.y
                            // console.log("Mouse pressed at", pressX, pressY)
                        }

                        onReleased: function(event){

                            let dx = event.x - pressX
                            let dy = event.y - pressY
                            // console.log("Tile released at row:", row, "col:", col, "dx:", dx, "dy:", dy)

                            let targetRow = row
                            let targetCol = col

                            if (Math.abs(dx) > Math.abs(dy)) {
                                if (dx > cellSize/2) targetCol = col + 1
                                else if (dx < -cellSize/2) targetCol = col - 1
                            } else {
                                if (dy > cellSize/2) targetRow = row + 1
                                else if (dy < -cellSize/2) targetRow = row - 1
                            }

                            var r1 = row
                            var c1 = col
                            var r2 = targetRow
                            var c2 = targetCol
                            if ((tileRect.row === r1 && tileRect.col === c1) ||
                                (tileRect.row === r2 && tileRect.col === c2)) {

                                var otherRow = (tileRect.row === r1) ? r2 : r1
                                var otherCol = (tileRect.col === c1) ? c2 : c1
                                var otherIndex = otherRow * columns + otherCol
                                var otherTile = boardRepeater.itemAt(otherIndex)
                                if (!otherTile) return

                                // 动态创建动画
                                var animQML = `
                                import QtQuick 2.0;
                                SequentialAnimation {
                                    PropertyAnimation { target: tileRect; property: "x"; to: ${otherTile.x+5}; duration: 1 }
                                    PropertyAnimation { target: tileRect; property: "x"; to: ${otherTile.x-5}; duration: 1 }
                                    PropertyAnimation { target: tileRect; property: "x"; to: ${tileRect.col*cellSize}; duration: 50 }
                                    PropertyAnimation { target: tileRect; property: "y"; to: ${otherTile.y+5}; duration: 1 }
                                    PropertyAnimation { target: tileRect; property: "y"; to: ${otherTile.y-5}; duration: 1 }
                                    PropertyAnimation { target: tileRect; property: "y"; to: ${tileRect.row*cellSize}; duration: 50 }
                                }
                                `;


                                var anim = Qt.createQmlObject(animQML, tileRect);
                                anim.start();
                            }

                            // console.log("Attempting swap with targetRow:", targetRow, "targetCol:", targetCol)
                            anim.onStopped.connect(function() {
                                gameBoard.trySwap(row, col, targetRow, targetCol)
                            })
                        }
                    }

                    // 无效交换抖动动画
                    Connections {
                        target: gameBoard
                        function onInvalidSwap(r1, c1, r2, c2) {
                            // console.log("Invalid swap triggered at row:", r, "col:", c)
                            if ((tileRect.row === r1 && tileRect.col === c1) ||
                                (tileRect.row === r2 && tileRect.col === c2)) {
                                shakeAnim.start() // 仅显示 shakeAnim
                            }
                        }
                    }
                }
            }
            // 技能按钮区
            Rectangle {
                id: skillArea
                anchors.top: gameArea.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.topMargin: 50   // 这里就可以加 20 像素间距

                Row {
                    anchors.centerIn: parent
                    spacing: 10
                    Button {
                        text: "new"
                        width: 120
                        height: 50
                        onClicked: gameBoard.GameInit()
                    }

                    Button {
                        text: "random"
                        width: 120
                        height: 50
                        onClicked: gameBoard.shuffleTiles()
                    }
                }
            }

        }

        // 监听棋盘变化刷新颜色
        Connections {
            target: gameBoard
            function onBoardChanged() {
                // console.log("Board changed, updating tiles...")
                for (let i = 0; i < rows * columns; ++i) {
                    let tile = boardRepeater.itemAt(i)
                    // console.log("Updating tile at row:", tile.row, "col:", tile.col, "new color:", gameBoard.tileAt(tile.row, tile.col))
                    tile.color = gameBoard.tileAt(tile.row, tile.col)
                }
            }
        }
        // 强烈动画
        // Connections {
        //     target: gameBoard
        //     function onTileDropped(fromRow, fromCol, toRow, toCol, color) {
        //         console.log("onTileDropped:", fromRow, fromCol, "->", toRow, toCol, "color raw:", color, "len:", (color ? color.length : 0))
        //         var tile = null
        //         if (fromRow === -1) {
        //             tile = boardRepeater.itemAt(toRow * columns + toCol)
        //         } else {
        //             tile = boardRepeater.itemAt(fromRow * columns + fromCol)
        //             if (!tile) {
        //                 // 保底：按 row/col/颜色查找
        //                 for (var i = 0; i < boardRepeater.count; ++i) {
        //                     var t = boardRepeater.itemAt(i)
        //                     if (t && t.row === fromRow && t.col === fromCol) { tile = t; break; }
        //                 }
        //             }
        //         }
        //         if (!tile) { console.log("drop: cannot find tile", fromRow, fromCol); return }
        //         // 设颜色之前做个净化（防止空白字符）
        //         var c = (color ? color.trim() : "")
        //         tile.row = toRow
        //         tile.col = toCol
        //         tile.color = color
        //     }
        // }
        Connections {
            target: gameBoard
            function onTileDropped(fromRow, fromCol, toRow, toCol, color) {
                let tile = boardRepeater.itemAt(toRow * columns + toCol)
                if (!tile) return
                tile.row = toRow
                tile.col = toCol
                tile.color = color
            }
        }
    }
}
