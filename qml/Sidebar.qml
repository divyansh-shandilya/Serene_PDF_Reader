import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: sidebar
    Layout.preferredWidth: collapsed ? 0 : 215
    Layout.fillHeight: true
    color: appTheme.colors.sidebarBg
    clip: true

    Behavior on Layout.preferredWidth {
        NumberAnimation {
            duration: 180
            easing.type: Easing.OutCubic
        }
    }

    Theme {
        id: appTheme
    }

    // Clean right-side border only (Notion/Arc style - avoiding boxy top/left/bottom double borders)
    Rectangle {
        id: rightDivider
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 1
        color: appTheme.colors.line
    }

    property bool collapsed: false
    property string currentView: "all" // all | favorites
    property int selectedFolderIndex: -1
    property var foldersModel: null
    property var folderDocs: ({})

    signal requestUpload()
    signal openSettings()
    signal createFolder()
    signal deleteFolder()
    signal selectFolder(int index)

    ColumnLayout {
        id: sidebarContent
        width: 183
        anchors.left: parent.left
        anchors.leftMargin: 16
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.topMargin: 20
        anchors.bottomMargin: 20
        spacing: 12

        opacity: sidebar.width >= 120 ? 1.0 : 0.0
        Behavior on opacity {
            NumberAnimation { duration: 150 }
        }

        // Brand Header (High polish)
        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            Layout.bottomMargin: 8

            Image {
                width: 32
                height: 32
                source: "logo.svg"
                sourceSize.width: 32
                sourceSize.height: 32
                smooth: true
                antialiasing: true
            }

            ColumnLayout {
                spacing: 0
                Text {
                    text: "Serene"
                    font.family: "Georgia, Serif"
                    font.pixelSize: 16
                    font.weight: Font.Bold
                    color: appTheme.colors.textMain
                }
                Text {
                    text: "READER"
                    font.family: "Inter"
                    font.pixelSize: 8
                    font.weight: Font.Bold
                    color: appTheme.colors.textSub
                    font.letterSpacing: 1.8
                }
            }
        }

        // Action Items (All Documents & Starred)
        Button {
            id: allDocsPill
            Layout.fillWidth: true
            Layout.preferredHeight: 34
            hoverEnabled: true

            background: Rectangle {
                color: (sidebar.selectedFolderIndex < 0 && sidebar.currentView === "all")
                    ? appTheme.colors.primaryLight
                    : (allDocsPill.hovered ? appTheme.colors.hoverBg : "transparent")
                radius: 6
            }

            contentItem: RowLayout {
                spacing: 8
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10

                Text {
                    text: "\ue873" // page icon
                    font.family: window.iconFont
                    font.pixelSize: 14
                    color: (sidebar.selectedFolderIndex < 0 && sidebar.currentView === "all") ? appTheme.colors.textMain : appTheme.colors.textSub
                }
                Text {
                    text: "All Documents"
                    font.family: "Inter"
                    font.pixelSize: 12
                    font.weight: (sidebar.selectedFolderIndex < 0 && sidebar.currentView === "all") ? Font.DemiBold : Font.Normal
                    color: appTheme.colors.textMain
                    Layout.fillWidth: true
                }
            }
            onClicked: {
                sidebar.currentView = "all"
                sidebar.selectFolder(-1)
            }
        }

        Button {
            id: starredPill
            Layout.fillWidth: true
            Layout.preferredHeight: 34
            hoverEnabled: true

            background: Rectangle {
                color: (sidebar.selectedFolderIndex < 0 && sidebar.currentView === "favorites")
                    ? appTheme.colors.primaryLight
                    : (starredPill.hovered ? appTheme.colors.hoverBg : "transparent")
                radius: 6
            }

            contentItem: RowLayout {
                spacing: 8
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10

                Text {
                    text: window.icons.star
                    font.family: window.iconFont
                    font.pixelSize: 14
                    color: (sidebar.selectedFolderIndex < 0 && sidebar.currentView === "favorites") ? "#eab308" : appTheme.colors.textSub
                }
                Text {
                    text: "Starred"
                    font.family: "Inter"
                    font.pixelSize: 12
                    font.weight: (sidebar.selectedFolderIndex < 0 && sidebar.currentView === "favorites") ? Font.DemiBold : Font.Normal
                    color: appTheme.colors.textMain
                    Layout.fillWidth: true
                }
            }
            onClicked: {
                sidebar.currentView = "favorites"
                sidebar.selectFolder(-1)
            }
        }

        // Folder List Section
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            visible: sidebar.foldersModel && sidebar.foldersModel.count > 0

            Column {
                width: parent.width
                spacing: 4
                Repeater {
                    model: sidebar.foldersModel
                    delegate: Button {
                        id: folderItemBtn
                        width: parent.width
                        height: 30
                        hoverEnabled: true

                        background: Rectangle {
                            color: (sidebar.selectedFolderIndex === index && sidebar.currentView === "all")
                                ? appTheme.colors.primaryLight
                                : (folderItemBtn.hovered ? appTheme.colors.hoverBg : "transparent")
                            radius: 6
                        }

                        contentItem: RowLayout {
                            spacing: 8
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10

                            Text {
                                text: "\ue2c7" // folder icon
                                font.family: window.iconFont
                                font.pixelSize: 14
                                color: (sidebar.selectedFolderIndex === index && sidebar.currentView === "all") ? appTheme.colors.textMain : appTheme.colors.textSub
                            }
                            Text {
                                text: model.name
                                font.family: "Inter"
                                font.pixelSize: 12
                                font.weight: (sidebar.selectedFolderIndex === index && sidebar.currentView === "all") ? Font.DemiBold : Font.Normal
                                color: appTheme.colors.textMain
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                            }
                        }
                        onClicked: {
                            sidebar.currentView = "all"
                            sidebar.selectFolder(index)
                        }
                    }
                }
            }
        }

        // Delete active folder buttons
        Button {
            id: deleteFolderBtn
            Layout.fillWidth: true
            Layout.preferredHeight: 32
            visible: sidebar.selectedFolderIndex >= 0
            hoverEnabled: true

            background: Rectangle {
                color: deleteFolderBtn.pressed ? "#fee2e2" : (deleteFolderBtn.hovered ? "#ffebeb" : "transparent")
                border.color: deleteFolderBtn.hovered ? "#ef4444" : "transparent"
                border.width: 1
                radius: 6
            }

            contentItem: RowLayout {
                spacing: 8
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10

                Text {
                    text: window.icons.delete
                    font.family: window.iconFont
                    font.pixelSize: 14
                    color: "#ef4444"
                }
                Text {
                    text: "Delete Folder"
                    font.family: "Inter"
                    font.pixelSize: 11
                    font.weight: Font.Medium
                    color: "#ef4444"
                    Layout.fillWidth: true
                }
            }
            onClicked: sidebar.deleteFolder()
        }

        Item {
            Layout.fillHeight: true
            visible: !sidebar.foldersModel || sidebar.foldersModel.count === 0
        }

        // Bottom Utilities Bar (Matches mockup layout flawlessly)
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8

            Button {
                id: collapseBtn
                Layout.fillWidth: true
                Layout.preferredHeight: 32
                hoverEnabled: true

                background: Rectangle {
                    color: collapseBtn.pressed ? appTheme.colors.primaryLight : (collapseBtn.hovered ? appTheme.colors.hoverBg : "transparent")
                    radius: 6
                }

                contentItem: RowLayout {
                    spacing: 8
                    anchors.fill: parent
                    anchors.leftMargin: 10

                    Text {
                        text: "\ue5c4" // navigation back arrow, or we can use collapse symbol
                        font.family: window.iconFont
                        font.pixelSize: 14
                        color: appTheme.colors.textSub
                    }
                    Text {
                        text: "Collapse"
                        font.family: "Inter"
                        font.pixelSize: 11
                        color: appTheme.colors.textSub
                        Layout.fillWidth: true
                    }
                }
                onClicked: sidebar.collapsed = true
            }

            // New Entry solid charcoal button (styled premium, matches primary theme)
            Button {
                id: uploadCardBtn
                Layout.fillWidth: true
                Layout.preferredHeight: 36
                hoverEnabled: true

                background: Rectangle {
                    color: uploadCardBtn.pressed ? "#444444" : (uploadCardBtn.hovered ? appTheme.colors.primaryHover : appTheme.colors.primary)
                    radius: 8
                    Behavior on color { ColorAnimation { duration: 150 } }
                }

                contentItem: RowLayout {
                    spacing: 6
                    anchors.centerIn: parent

                    Text {
                        text: "＋"
                        font.family: "Inter"
                        font.pixelSize: 13
                        font.weight: Font.Bold
                        color: "#ffffff"
                    }
                    Text {
                        text: "New Entry"
                        font.family: "Inter"
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                        color: "#ffffff"
                    }
                }
                onClicked: sidebar.requestUpload()
            }

            Button {
                id: settingsBtn
                Layout.fillWidth: true
                Layout.preferredHeight: 32
                hoverEnabled: true

                background: Rectangle {
                    color: settingsBtn.pressed ? appTheme.colors.primaryLight : (settingsBtn.hovered ? appTheme.colors.hoverBg : "transparent")
                    radius: 6
                }

                contentItem: RowLayout {
                    spacing: 8
                    anchors.fill: parent
                    anchors.leftMargin: 10

                    Text {
                        text: window.icons.settings
                        font.family: window.iconFont
                        font.pixelSize: 14
                        color: appTheme.colors.textSub
                    }
                    Text {
                        text: "Settings"
                        font.family: "Inter"
                        font.pixelSize: 11
                        color: appTheme.colors.textSub
                        Layout.fillWidth: true
                    }
                }
                onClicked: sidebar.openSettings()
            }
        }
    }
}
