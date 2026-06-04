import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

RowLayout {
    id: topBar
    spacing: 12
    Layout.fillWidth: true
    Layout.leftMargin: 24
    Layout.rightMargin: 24
    Layout.topMargin: 16
    Layout.bottomMargin: 4

    property alias searchQuery: searchBar.text
    property bool sidebarCollapsed: false
    signal toggleSidebar()

    Theme {
        id: appTheme
    }

    IconButton {
        id: sidebarToggleBtn
        iconText: topBar.sidebarCollapsed ? "\ue5d2" : "\ue5c4"
        iconSize: 18
        iconColor: appTheme.colors.textSub
        Layout.alignment: Qt.AlignVCenter
        onClicked: topBar.toggleSidebar()
    }

    SearchBar {
        id: searchBar
        Layout.fillWidth: true
    }


}
