import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: settingsDialog
    modal: true
    
    // Parent to Overlay and center it
    parent: Overlay.overlay
    x: Math.round((parent.width - width) / 2)
    y: Math.round((parent.height - height) / 2)
    
    width: 480
    height: 600
    padding: 0

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: 250; easing.type: Easing.OutCubic }
        NumberAnimation { property: "scale"; from: 0.94; to: 1.0; duration: 250; easing.type: Easing.OutCubic }
    }
    
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1.0; to: 0.0; duration: 200; easing.type: Easing.InCubic }
        NumberAnimation { property: "scale"; from: 1.0; to: 0.94; duration: 200; easing.type: Easing.InCubic }
    }
    
    background: Rectangle {
        color: "white"
        radius: 28 // Slightly more rounded for a modern look
        border.color: "#EAEAE8"
        border.width: 1
        antialiasing: true
        
        // Shadow disabled for performance on low-end devices
        layer.enabled: false
    }

    header: Item {
        height: 84 // Increased height for better balance
        width: parent.width
        
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 36
            anchors.rightMargin: 20
            
            Text { 
                text: "Settings"
                font.pixelSize: 28
                color: "#1A1A1A"
                font.family: "Georgia, Serif"
                Layout.alignment: Qt.AlignVCenter
            }
            
            Item { Layout.fillWidth: true }
            
            ToolButton {
                id: closeBtn
                padding: 0
                width: 40
                height: 40
                contentItem: Text {
                    text: window.icons.close
                    font.family: window.iconFont
                    font.pixelSize: 24
                    color: "#999"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    radius: 20
                    color: closeBtn.hovered ? "#F5F5F3" : "transparent"
                }
                onClicked: settingsDialog.close()
            }
        }
    }

    contentItem: Item {
        // Enforce a "safe zone" for content to prevent hitting rounded corners
        clip: true
        
        ScrollView {
            id: settingsScroll
            anchors.fill: parent
            ScrollBar.vertical.policy: ScrollBar.AsNeeded
            
            // Content container with explicit side padding
            Item {
                width: settingsScroll.availableWidth
                implicitHeight: mainLayout.height
                
                ColumnLayout {
                    id: mainLayout
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: 36
                    anchors.rightMargin: 36
                    anchors.top: parent.top
                    anchors.topMargin: 20
                    anchors.bottomMargin: 60
                    spacing: 40 // Increased spacing between sections

                    // Reader Mode
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 14
                        RowLayout {
                            spacing: 12
                            Text { 
                                text: window.icons.reader
                                font.family: window.iconFont
                                font.pixelSize: 20
                                color: "#B0B0A8"
                            }
                            Text { 
                                text: "READER MODE"
                                font.pixelSize: 11
                                font.weight: Font.Bold
                                color: "#9A9992"
                                font.letterSpacing: 1.5
                            }
                        }
                        Rectangle {
                            Layout.fillWidth: true; height: 60; radius: 16; color: "#F9F9F7"
                            RowLayout {
                                anchors.fill: parent; anchors.margins: 6; spacing: 0
                                Rectangle { 
                                    Layout.fillWidth: true; Layout.fillHeight: true; radius: 12
                                    color: window.readerMode === "paginated" ? "white" : "transparent"
                                    border.color: window.readerMode === "paginated" ? "#EAEAE8" : "transparent"
                                    Text { anchors.centerIn: parent; text: "Paginated"; color: "#2D3436"; font.weight: window.readerMode === "paginated" ? Font.Medium : Font.Normal; font.pixelSize: 15 }
                                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: window.readerMode = "paginated" }
                                }
                                Rectangle { 
                                    Layout.fillWidth: true; Layout.fillHeight: true; radius: 12
                                    color: window.readerMode === "continuous" ? "white" : "transparent"
                                    border.color: window.readerMode === "continuous" ? "#EAEAE8" : "transparent"
                                    Text { anchors.centerIn: parent; text: "Continuous"; color: "#2D3436"; font.weight: window.readerMode === "continuous" ? Font.Medium : Font.Normal; font.pixelSize: 15 }
                                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: window.readerMode = "continuous" }
                                }
                            }
                        }
                    }

                    // Performance
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 14
                        RowLayout {
                            spacing: 12
                            Text { 
                                text: window.icons.performance
                                font.family: window.iconFont
                                font.pixelSize: 20
                                color: "#B0B0A8"
                            }
                            Text { 
                                text: "PERFORMANCE"
                                font.pixelSize: 11
                                font.weight: Font.Bold
                                color: "#9A9992"
                                font.letterSpacing: 1.5
                            }
                        }
                        Rectangle {
                            Layout.fillWidth: true; height: 56; radius: 28; color: "#F4F4F1"
                            RowLayout {
                                anchors.fill: parent; anchors.margins: 6; spacing: 0
                                Repeater {
                                    model: ["eco", "base", "sport"]
                                    delegate: Rectangle {
                                        Layout.fillWidth: true; Layout.fillHeight: true; radius: 22
                                        color: window.performanceMode === modelData ? "white" : "transparent"
                                        border.color: window.performanceMode === modelData ? "#EAEAE8" : "transparent"
                                        Text { 
                                            anchors.centerIn: parent; text: modelData.toUpperCase()
                                            font.pixelSize: 11; font.weight: Font.Bold; color: window.performanceMode === modelData ? "#2D3436" : "#999"
                                        }
                                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: window.performanceMode = modelData }
                                    }
                                }
                            }
                        }
                    }

                    // Interface
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 14
                        RowLayout {
                            spacing: 12
                            Text { 
                                text: window.icons.interface
                                font.family: window.iconFont
                                font.pixelSize: 20
                                color: "#B0B0A8"
                            }
                            Text { 
                                text: "INTERFACE"
                                font.pixelSize: 11
                                font.weight: Font.Bold
                                color: "#9A9992"
                                font.letterSpacing: 1.5
                            }
                        }
                        
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            // Show page count toggle
                            Rectangle {
                                Layout.fillWidth: true; height: 72; radius: 16; color: "#F9F9F7"
                                RowLayout {
                                    anchors.fill: parent; anchors.leftMargin: 20; anchors.rightMargin: 20; spacing: 16
                                    Rectangle {
                                        width: 44; height: 24; radius: 12; color: window.showPageCount ? "#10B981" : "#D1D1CB"
                                        Rectangle {
                                            width: 18; height: 18; radius: 9; color: "white"
                                            anchors.verticalCenter: parent.verticalCenter
                                            x: window.showPageCount ? 22 : 4
                                            Behavior on x { NumberAnimation { duration: 200 } }
                                        }
                                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: window.showPageCount = !window.showPageCount }
                                    }
                                    Text { text: "Show page count"; font.pixelSize: 15; color: "#2D3436" }
                                    Item { Layout.fillWidth: true }
                                }
                            }

                            // Free Pan / Smooth Scroll toggle
                            Rectangle {
                                Layout.fillWidth: true; height: 72; radius: 16; color: "#F9F9F7"
                                RowLayout {
                                    anchors.fill: parent; anchors.leftMargin: 20; anchors.rightMargin: 20; spacing: 16
                                    Rectangle {
                                        width: 44; height: 24; radius: 12; color: window.freePan ? "#10B981" : "#D1D1CB"
                                        Rectangle {
                                            width: 18; height: 18; radius: 9; color: "white"
                                            anchors.verticalCenter: parent.verticalCenter
                                            x: window.freePan ? 22 : 4
                                            Behavior on x { NumberAnimation { duration: 200 } }
                                        }
                                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: window.freePan = !window.freePan }
                                    }
                                    ColumnLayout {
                                        spacing: 2
                                        Text { text: "Free Panning"; font.pixelSize: 15; color: "#2D3436" }
                                        Text { text: "Pinch and scroll anywhere when zoomed"; font.pixelSize: 12; color: "#999" }
                                    }
                                    Item { Layout.fillWidth: true }
                                }
                            }

                            // Show FPS toggle
                            Rectangle {
                                Layout.fillWidth: true; height: 72; radius: 16; color: "#F9F9F7"
                                RowLayout {
                                    anchors.fill: parent; anchors.leftMargin: 20; anchors.rightMargin: 20; spacing: 16
                                    Rectangle {
                                        width: 44; height: 24; radius: 12; color: window.showFps ? "#10B981" : "#D1D1CB"
                                        Rectangle {
                                            width: 18; height: 18; radius: 9; color: "white"
                                            anchors.verticalCenter: parent.verticalCenter
                                            x: window.showFps ? 22 : 4
                                            Behavior on x { NumberAnimation { duration: 200 } }
                                        }
                                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: window.showFps = !window.showFps }
                                    }
                                    ColumnLayout {
                                        spacing: 2
                                        Text { text: "Show Rendering FPS"; font.pixelSize: 15; color: "#2D3436" }
                                        Text { text: "Display real-time rendering performance metrics"; font.pixelSize: 12; color: "#999" }
                                    }
                                    Item { Layout.fillWidth: true }
                                }
                            }
                        }
                    }

                    // Toolbar Position
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 14
                        RowLayout {
                            spacing: 12
                            Text { 
                                text: window.icons.toolbar
                                font.family: window.iconFont
                                font.pixelSize: 20
                                color: "#B0B0A8"
                            }
                            Text { 
                                text: "TOOLBAR POSITION"
                                font.pixelSize: 11
                                font.weight: Font.Bold
                                color: "#9A9992"
                                font.letterSpacing: 1.5
                            }
                        }
                        Rectangle {
                            Layout.fillWidth: true; height: 56; radius: 28; color: "#F4F4F1"
                            RowLayout {
                                anchors.fill: parent; anchors.margins: 6; spacing: 0
                                Repeater {
                                    model: ["left", "bottom", "right"]
                                    delegate: Rectangle {
                                        Layout.fillWidth: true; Layout.fillHeight: true; radius: 22
                                        color: window.toolbarPosition === modelData ? "white" : "transparent"
                                        border.color: window.toolbarPosition === modelData ? "#EAEAE8" : "transparent"
                                        Text { anchors.centerIn: parent; text: modelData.toUpperCase(); font.pixelSize: 11; font.weight: Font.Bold; color: window.toolbarPosition === modelData ? "#2D3436" : "#999" }
                                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: window.toolbarPosition = modelData }
                                    }
                                }
                            }
                        }
                    }

                    Item { Layout.preferredHeight: 20 }

                    RowLayout {
                        Layout.fillWidth: true; spacing: 10; opacity: 0.6; Layout.alignment: Qt.AlignHCenter
                        Text { 
                            text: window.icons.info
                            font.family: window.iconFont
                            font.pixelSize: 18
                            color: "#999"
                        }
                        Text { text: "Settings are applied instantly."; font.pixelSize: 12; color: "#999" }
                        Item { Layout.fillWidth: true }
                    }
                }
            }
        }
    }
}
