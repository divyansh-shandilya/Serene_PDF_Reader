import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects

Item {
    id: cardRoot
    width: 115
    height: 195

    property string viewMode: "grid" // "grid" | "list"

    // Model mappings
    property int docId: 0
    property string title: ""
    property string author: ""
    property string filePath: ""
    property int lastPage: -1
    property int pageCount: 1
    property string thumbnailPath: ""
    property bool isFavorite: false
    property var foldersModel: null
    property bool inSelectedFolder: false

    // State / signals
    signal openDocument()
    signal toggleFavorite()
    signal removeDocument()
    signal moveToFolder()
    signal removeFromFolder()

    Theme {
        id: appTheme
    }

    component StyledMenuItem : MenuItem {
        id: menuItem
        implicitHeight: 34
        implicitWidth: 170

        contentItem: Text {
            text: menuItem.text
            font.family: "Inter"
            font.pixelSize: 13
            font.weight: Font.Normal
            color: menuItem.enabled ? (menuItem.hovered ? "#1e293b" : "#475569") : "#cbd5e1"
            verticalAlignment: Text.AlignVCenter
            leftPadding: 16
            rightPadding: 16
        }

        background: Rectangle {
            implicitWidth: 170
            implicitHeight: 34
            color: menuItem.hovered ? "#f1f5f9" : "transparent"
            radius: 5
            anchors.fill: parent
            anchors.margins: 4
        }
    }

    component StyledMenuSeparator : MenuSeparator {
        id: menuSep
        implicitWidth: 170
        implicitHeight: 9
        topPadding: 4
        bottomPadding: 4
        leftPadding: 8
        rightPadding: 8

        contentItem: Rectangle {
            implicitWidth: 154
            implicitHeight: 1
            color: "#e2e8f0"
        }
    }

    // Outer container wrapper for smooth scaling & shadow on hover
    Rectangle {
        id: container
        x: 0
        y: (hoverArea.containsMouse && cardRoot.viewMode === "grid") ? -3 : 0
        width: parent.width
        height: parent.height
        visible: parent.width >= 50 && parent.height >= 30
        color: cardRoot.viewMode === "grid" ? appTheme.colors.cardBg : "transparent"
        radius: cardRoot.viewMode === "grid" ? 14 : 0
        border.color: cardRoot.viewMode === "grid" ? (hoverArea.containsMouse ? appTheme.colors.borderHover : appTheme.colors.line) : "transparent"
        border.width: cardRoot.viewMode === "grid" ? 1 : 0
        clip: false // do not clip outer border, prevents chipped corners

        // Tactile lift and scale animation (for Grid mode only)
        scale: (hoverArea.containsMouse && cardRoot.viewMode === "grid") ? 1.015 : 1.0
        
        Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
        Behavior on y { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }

        // Horizontal list divider line under each item in list mode
        Rectangle {
            id: rowDivider
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 1
            color: appTheme.colors.line
            visible: cardRoot.viewMode === "list"
        }

        // Subtle background fill on hover for the entire row in list mode
        Rectangle {
            id: rowBgHover
            anchors.fill: parent
            color: appTheme.colors.hoverBg
            visible: cardRoot.viewMode === "list" && hoverArea.containsMouse
            z: -1
            radius: 6
        }

        // Cover container holds the background gradients, custom overlays, or the cover image
        Rectangle {
            id: coverContainer
            x: cardRoot.viewMode === "grid" ? 10 : 12
            y: cardRoot.viewMode === "grid" ? 10 : 8
            width: cardRoot.viewMode === "grid" ? (parent.width - 20) : 40
            height: cardRoot.viewMode === "grid" ? Math.max(1, (parent.width - 20) * 1.414) : Math.max(1, parent.height - 16)
            radius: cardRoot.viewMode === "grid" ? 12 : 6
            clip: true
            z: 1
            color: appTheme.colors.hoverBg // soft grey/sand placeholder background

            border.width: 0
            border.color: "transparent"

            layer.enabled: true
            layer.effect: MultiEffect {
                maskEnabled: true
                maskSource: coverMaskShaderSource
            }

            // Actual thumbnail image
            Image {
                id: mainCoverImage
                anchors.fill: parent
                source: cardRoot.thumbnailPath
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
                visible: cardRoot.thumbnailPath !== ""
            }

            // Fallback content when no thumbnail is present - Styled as an elegant classic journal cover
            Rectangle {
                anchors.fill: parent
                visible: cardRoot.thumbnailPath === ""
                gradient: Gradient {
                    GradientStop { position: 0.0; color: "#2D2B2A" } // Warm Soft Deep Charcoal
                    GradientStop { position: 1.0; color: "#1A1A1A" } // Serene rich charcoal
                }

                // Stylish book binding stripe on the left edge
                Rectangle {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: cardRoot.viewMode === "grid" ? 10 : 3
                    color: Qt.rgba(0, 0, 0, 0.25)
                }

                // Decorative embossed line
                Rectangle {
                    anchors.left: parent.left
                    anchors.leftMargin: cardRoot.viewMode === "grid" ? 12 : 4
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: 1
                    color: Qt.rgba(255, 255, 255, 0.1)
                }

                // Elegant Typographic Cover for Grid Mode
                ColumnLayout {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.margins: cardRoot.viewMode === "grid" ? 14 : 2
                    anchors.leftMargin: cardRoot.viewMode === "grid" ? 22 : 6
                    spacing: 4
                    visible: cardRoot.viewMode === "grid"

                    Rectangle {
                        height: 18; width: 32; radius: 4
                        color: Qt.rgba(255, 255, 255, 0.15)
                        Layout.alignment: Qt.AlignLeft
                        Text {
                            anchors.centerIn: parent
                            text: "PDF"
                            font.family: "Inter"
                            font.pixelSize: 8
                            font.weight: Font.Bold
                            color: "#ffffff"
                        }
                    }

                    Item { Layout.fillHeight: true } // spacer

                    Text {
                        Layout.fillWidth: true
                        text: cardRoot.title
                        font.family: "Georgia, Serif"
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                        color: "#F4F4F1"
                        elide: Text.ElideRight
                        maximumLineCount: 3
                        wrapMode: Text.Wrap
                    }

                }

                // Micro Typographic Cover for List Mode (very small space)
                Text {
                    anchors.centerIn: parent
                    anchors.horizontalCenterOffset: 1
                    visible: cardRoot.viewMode === "list"
                    text: "\ue873" // page icon
                    font.family: window.iconFont
                    font.pixelSize: 14
                    color: appTheme.colors.textMuted
                }
            }

            // Soft overlay glow with play/read button when mouse enters card bounds (Grid mode only)
            Rectangle {
                anchors.fill: parent
                color: Qt.rgba(26/255, 26/255, 26/255, 0.05)
                opacity: (hoverArea.containsMouse && cardRoot.viewMode === "grid") ? 1.0 : 0
                Behavior on opacity { NumberAnimation { duration: 150 } }
                visible: cardRoot.viewMode === "grid"

                Rectangle {
                    width: 32; height: 32; radius: 16
                    color: "#ffffff"
                    anchors.centerIn: parent

                    Text {
                        anchors.centerIn: parent
                        anchors.horizontalCenterOffset: 1
                        text: window.icons.play
                        font.family: window.iconFont
                        font.pixelSize: 16
                        color: appTheme.colors.textMain
                    }
                }
            }

            // Star favorite sticker badge inside top-left corner (Grid mode only)
            Rectangle {
                id: favoriteBadge
                width: 26
                height: 26
                radius: 13
                color: "#eab308" // glorious gold background
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.margins: 10
                z: 6
                visible: cardRoot.isFavorite && cardRoot.viewMode === "grid"

                // Sparkly entrance pop when favorited matches status
                scale: cardRoot.isFavorite ? 1.0 : 0.0
                Behavior on scale {
                    NumberAnimation { duration: 300; easing.type: Easing.OutBack }
                }

                Text {
                    anchors.centerIn: parent
                    text: "\ue838"
                    font.family: window.iconFont
                    font.pixelSize: 13
                    color: "#ffffff"
                }
            }

            // Hover Star button trigger inside top-right corner (Grid mode only)
            Item {
                id: hoverStarBtn
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.margins: 8
                width: 32
                height: 32
                z: 6

                // Smooth fade-in on hover
                opacity: (hoverArea.containsMouse || starMouse.containsMouse || cardRoot.isFavorite) && cardRoot.viewMode === "grid" ? 1.0 : 0.0
                visible: opacity > 0.0
                Behavior on opacity {
                    NumberAnimation { duration: 150 }
                }

                // Inner visual zone that scales safely without changing the hover detection bounds
                Rectangle {
                    id: hoverStarVisual
                    anchors.fill: parent
                    radius: 16
                    color: Qt.rgba(1, 1, 1, 0.95)
                    border.color: cardRoot.isFavorite ? "#f59e0b" : "#cbd5e1"
                    border.width: starMouse.containsMouse || cardRoot.isFavorite ? 1.5 : 1

                    scale: starMouse.containsMouse ? (starMouse.pressed ? 0.91 : 1.25) : 1.0
                    Behavior on scale {
                        NumberAnimation { duration: 150; easing.type: Easing.OutBack }
                    }
                    Behavior on border.color { ColorAnimation { duration: 150 } }

                    Text {
                        anchors.centerIn: parent
                        text: cardRoot.isFavorite ? window.icons.favorite : window.icons.favorite_border
                        font.family: window.iconFont
                        font.pixelSize: 16
                        color: cardRoot.isFavorite ? "#eab308" : "#64748b"

                        scale: cardRoot.isFavorite ? 1.15 : 1.0
                        Behavior on scale {
                            NumberAnimation { duration: 250; easing.type: Easing.OutBack }
                        }
                    }
                }

                MouseArea {
                    id: starMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: cardRoot.toggleFavorite()
                }
            }

            // Options menu trigger button in bottom-right of cover (Grid mode only)
            Rectangle {
                id: hoverOptionsBtn
                anchors.bottom: parent.bottom
                anchors.right: parent.right
                anchors.margins: 8
                width: 24
                height: 24
                radius: 12
                color: Qt.rgba(1, 1, 1, 0.9)
                border.color: "#e2e8f0"
                border.width: 1
                visible: (hoverArea.containsMouse || optionsMouse.containsMouse || fileMenu.visible) && cardRoot.viewMode === "grid"
                z: 6

                Text {
                    anchors.centerIn: parent
                    text: "⋮"
                    font.family: "Inter"
                    font.pixelSize: 12
                    font.weight: Font.Bold
                    color: "#64748b"
                }

                MouseArea {
                    id: optionsMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: fileMenu.open()
                }

                Menu {
                    id: fileMenu
                    y: hoverOptionsBtn.height + 2
                    width: 170

                    background: Rectangle {
                        implicitWidth: 170
                        color: "#ffffff"
                        radius: 8
                        border.color: "#e2e8f0"
                        border.width: 1

                        layer.enabled: true
                        layer.effect: MultiEffect {
                            shadowEnabled: true
                            shadowColor: Qt.rgba(0, 0, 0, 0.08)
                            shadowBlur: 10
                            shadowVerticalOffset: 3
                            shadowHorizontalOffset: 0
                        }
                    }

                    topPadding: 5
                    bottomPadding: 5

                    StyledMenuItem {
                        text: cardRoot.isFavorite ? "Remove Favorite" : "Add Favorite"
                        onTriggered: cardRoot.toggleFavorite()
                    }
                    StyledMenuItem {
                        text: "Delete"
                        onTriggered: cardRoot.removeDocument()
                    }
                    StyledMenuSeparator {}
                    StyledMenuItem {
                        text: "Move to Folder..."
                        enabled: cardRoot.foldersModel && cardRoot.foldersModel.count > 0
                        onTriggered: cardRoot.moveToFolder()
                    }
                    StyledMenuItem {
                        text: "Remove from Folder"
                        enabled: cardRoot.inSelectedFolder
                        onTriggered: cardRoot.removeFromFolder()
                    }
                }
            }

        }

        // Offscreen mask source properly parented in the scene graph to avoid circular dependencies and offset shifting
        ShaderEffectSource {
            id: coverMaskShaderSource
            width: Math.max(1, coverContainer.width)
            height: Math.max(1, coverContainer.height)
            visible: false
            live: true
            sourceItem: Rectangle {
                width: Math.max(1, coverContainer.width)
                height: Math.max(1, coverContainer.height)
                radius: coverContainer.radius
                color: "black"
            }
        }

        // Click focus/trigger bounds across the whole block
        MouseArea {
            id: hoverArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: cardRoot.openDocument()
        }

        // Details metadata container below the covers (Matches design mockup)
        ColumnLayout {
            id: detailsLayout
            anchors.top: cardRoot.viewMode === "grid" ? coverContainer.bottom : parent.top
            anchors.left: cardRoot.viewMode === "grid" ? parent.left : coverContainer.right
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            
            anchors.topMargin: cardRoot.viewMode === "grid" ? 4 : 12
            anchors.bottomMargin: cardRoot.viewMode === "grid" ? 10 : 12
            anchors.leftMargin: cardRoot.viewMode === "grid" ? 12 : 16
            anchors.rightMargin: cardRoot.viewMode === "grid" ? 12 : 220 // Space on right for actions
            spacing: cardRoot.viewMode === "grid" ? 3 : 4

            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                Text {
                    id: bookTitle
                    Layout.fillWidth: true
                    text: cardRoot.title
                    font.family: "Georgia, Serif"
                    font.pixelSize: cardRoot.viewMode === "grid" ? 13 : 14
                    font.weight: Font.Bold
                    color: appTheme.colors.textMain
                    elide: Text.ElideRight
                }

                // Small elegant page Tag (Grid mode only)
                Rectangle {
                    id: pageTag
                    Layout.alignment: Qt.AlignVCenter
                    height: 18
                    width: pageText.implicitWidth + 10
                    color: appTheme.colors.primaryLight
                    radius: 4
                    visible: cardRoot.viewMode === "grid"

                    Text {
                        id: pageText
                        anchors.centerIn: parent
                        text: "P. " + cardRoot.pageCount
                        font.family: "Inter"
                        font.pixelSize: 8
                        font.weight: Font.Medium
                        color: appTheme.colors.textSub
                    }
                }
            }

            // In List Mode, render extra summary metadata inline
            Text {
                visible: cardRoot.viewMode === "list"
                text: "Document size: " + cardRoot.pageCount + " pages"
                font.family: "Inter"
                font.pixelSize: 10
                color: appTheme.colors.textMuted
                Layout.fillWidth: true
                elide: Text.ElideRight
            }
        }

        // Action Bar for List Mode (Pristine right side layout)
        RowLayout {
            id: listActions
            visible: cardRoot.viewMode === "list"
            anchors.right: parent.right
            anchors.rightMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            spacing: 12
            z: 10



            // Favorite Star Button in list
            Item {
                id: listFavBtn
                width: 32
                height: 32

                Rectangle {
                    id: listFavBtnVisual
                    anchors.fill: parent
                    radius: 16
                    color: favMouse.containsMouse ? appTheme.colors.primaryLight : "transparent"
                    border.color: cardRoot.isFavorite ? "#f59e0b" : appTheme.colors.line
                    border.width: favMouse.containsMouse || cardRoot.isFavorite ? 1.5 : 1
                    
                    scale: favMouse.containsMouse ? (favMouse.pressed ? 0.91 : 1.25) : 1.0
                    Behavior on scale {
                        NumberAnimation { duration: 150; easing.type: Easing.OutBack }
                    }
                    Behavior on border.color { ColorAnimation { duration: 150 } }

                    Text {
                        anchors.centerIn: parent
                        text: cardRoot.isFavorite ? "\ue838" : "\ue83a"
                        font.family: window.iconFont
                        font.pixelSize: 16
                        color: cardRoot.isFavorite ? "#eab308" : appTheme.colors.textSub
                        
                        scale: cardRoot.isFavorite ? 1.15 : 1.0
                        Behavior on scale {
                            NumberAnimation { duration: 250; easing.type: Easing.OutBack }
                        }
                    }
                }

                MouseArea {
                    id: favMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: cardRoot.toggleFavorite()
                }
            }

            // Move to Folder button in list
            Rectangle {
                id: listFolderBtn
                width: 28
                height: 28
                radius: 14
                color: folderMouse.containsMouse ? appTheme.colors.primaryLight : "transparent"
                border.color: appTheme.colors.line
                border.width: 1
                visible: cardRoot.foldersModel && cardRoot.foldersModel.count > 0

                Text {
                    anchors.centerIn: parent
                    text: "\ue2c7" // folder icon
                    font.family: window.iconFont
                    font.pixelSize: 14
                    color: appTheme.colors.textSub
                }

                MouseArea {
                    id: folderMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: cardRoot.moveToFolder()
                }
            }

            // Remove from Folder button (only if in custom folder)
            Rectangle {
                id: listRemoveFolderBtn
                width: 28
                height: 28
                radius: 14
                color: removeFolderMouse.containsMouse ? "#fee2e2" : "transparent"
                border.color: removeFolderMouse.containsMouse ? "#ef4444" : appTheme.colors.line
                border.width: 1
                visible: cardRoot.inSelectedFolder

                Text {
                    anchors.centerIn: parent
                    text: "\ue15c" // list minus / circle minus icon
                    font.family: window.iconFont
                    font.pixelSize: 14
                    color: removeFolderMouse.containsMouse ? "#ef4444" : appTheme.colors.textSub
                }

                MouseArea {
                    id: removeFolderMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: cardRoot.removeFromFolder()
                }
            }

            // Direct Delete button (Immediate, satisfying, professional action)
            Rectangle {
                id: listDeleteBtn
                width: 28
                height: 28
                radius: 14
                color: deleteMouse.containsMouse ? "#fee2e2" : "transparent"
                border.color: deleteMouse.containsMouse ? "#ef4444" : appTheme.colors.line
                border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: window.icons.delete
                    font.family: window.iconFont
                    font.pixelSize: 13
                    color: deleteMouse.containsMouse ? "#ef4444" : appTheme.colors.textSub
                }

                MouseArea {
                    id: deleteMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: cardRoot.removeDocument()
                }
            }
        }
    }
}
