import QtQuick
import QtQuick.Controls

Button {
    id: control
    property string iconText: ""
    property real iconSize: 16
    property color iconColor: "#64748b"
    property color hoverBgColor: "#f1f5f9"
    property color pressedBgColor: "#e2e8f0"
    property real radius: 6

    implicitWidth: 34
    implicitHeight: 34
    hoverEnabled: true

    background: Rectangle {
        color: control.pressed ? control.pressedBgColor : (control.hovered ? control.hoverBgColor : "transparent")
        radius: control.radius
        Behavior on color { ColorAnimation { duration: 150 } }
    }

    contentItem: Text {
        text: control.iconText
        font.family: window.iconFont
        font.pixelSize: control.iconSize
        color: control.iconColor
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}
