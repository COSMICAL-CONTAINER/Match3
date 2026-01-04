// GameButton.qml
import QtQuick 2.15
import QtQuick.Controls 2.15

Button {
    property color backgroundColor: "#3498DB"

    width: 100
    height: 40
    contentItem: Text {
        text: parent.text
        font.pixelSize: 14
        font.bold: true
        color: "white"
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    background: Rectangle {
        radius: 20
        color: parent.down ? Qt.darker(parent.backgroundColor, 1.2) : parent.backgroundColor
        border.color: Qt.darker(parent.backgroundColor, 1.5)
        border.width: 2
    }
}
