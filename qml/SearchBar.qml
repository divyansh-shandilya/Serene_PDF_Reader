import QtQuick
import QtQuick.Controls

Item {
    id: searchBar
    Theme {
        id: appTheme
    }

    property alias text: textField.text
    property string placeholderText: "Search library..."
    property color colorFill: appTheme.colors.primaryLight
    property color textColor: appTheme.colors.textMain
    property color placeholderColor: appTheme.colors.textMuted
    property real radius: 6

    implicitHeight: 32

    TextField {
        id: textField
        anchors.fill: parent
        placeholderText: searchBar.placeholderText
        placeholderTextColor: searchBar.placeholderColor
        color: searchBar.textColor
        font.family: "Inter"
        font.pixelSize: 12
        hoverEnabled: true
        leftPadding: 34
        rightPadding: 12
        selectByMouse: true

        background: Rectangle {
            color: searchBar.colorFill
            radius: searchBar.radius
            border.color: textField.activeFocus ? appTheme.colors.borderHover : "transparent"
            border.width: 1

            Text {
                anchors.left: parent.left
                anchors.leftMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                text: window.icons.search
                font.family: window.iconFont
                font.pixelSize: 14
                color: appTheme.colors.textMuted
            }
        }
    }
}
