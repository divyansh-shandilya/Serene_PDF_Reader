import QtQuick
import QtQuick.Controls

MouseArea {
    id: chip
    property string text: ""
    property bool selected: false

    Theme {
        id: appTheme
    }

    implicitWidth: chipLabel.contentWidth + 32
    implicitHeight: 32
    hoverEnabled: true
    cursorShape: Qt.PointingHandCursor

    Rectangle {
        id: bgRec
        anchors.fill: parent
        color: chip.selected ? appTheme.colors.cardBg : "transparent"
        radius: 6
        border.color: chip.selected ? appTheme.colors.line : "transparent"
        border.width: chip.selected ? 1 : 0

        Behavior on color { ColorAnimation { duration: 150 } }
    }

    Text {
        id: chipLabel
        anchors.centerIn: parent
        text: chip.text
        font.family: "Inter"
        font.pixelSize: 12
        font.weight: chip.selected ? Font.DemiBold : Font.Normal
        color: chip.selected ? appTheme.colors.textMain : appTheme.colors.textSub
    }
}
