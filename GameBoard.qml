// GameBoard.qml
import QtQuick 2.15

Item {
    id: root
    property int rows: 8
    property int columns: 8
    property int score: 0

    signal swapAnimationRequested(int r1, int c1, int r2, int c2)
    signal matchAnimationRequested(var matchedTiles)
    signal dropAnimationRequested(var dropPaths)
}
