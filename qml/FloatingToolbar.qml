import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: toolbarRoot
    
    // Bindings for main window settings
    property int currentPage: 1
    property int pageCount: 1
    property int zoomLevel: 100
    property string performanceMode: "BASE"
    property string orientation: "vertical"
    property int pageRotation: 0
    property bool nightMode: false

    width: orientation === "vertical" ? 64 : 540
    height: orientation === "vertical" ? 780 : 64
    color: "#FAFAF8"
    radius: 20
    
    layer.enabled: false
    border.color: "#E8E7E0"
    border.width: 1

    Behavior on width { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
    Behavior on height { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
    
    // Entrance
    opacity: 0
    visible: opacity > 0
    Component.onCompleted: opacity = 1
    Behavior on opacity { NumberAnimation { duration: 300 } }

    component SidebarButton : ToolButton {
        id: btn
        Layout.alignment: Qt.AlignHCenter
        padding: 0
        background: Rectangle {
            implicitWidth: 44
            implicitHeight: 44
            radius: 12
            color: btn.hovered ? "#F0F0EE" : "transparent"
            scale: btn.pressed ? 0.9 : 1.0
        }
        contentItem: Text {
            text: btn.text
            font.family: window.iconFont ? window.iconFont : "Arial"
            font.pixelSize: 22
            color: "#2D3436"
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    signal back()
    signal nextPage()
    signal prevPage()
    signal zoomIn()
    signal zoomOut()
    signal toggleFocus()
    signal goToPage(int page)
    signal fitToWidth()
    signal fitToPage()

    GridLayout {
        anchors.fill: parent
        anchors.topMargin: 15
        anchors.bottomMargin: 15
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        columns: orientation === "vertical" ? 1 : 16
        rows: orientation === "vertical" ? 16 : 1
        columnSpacing: 0
        rowSpacing: orientation === "vertical" ? 6 : 0

        SidebarButton {
            text: orientation === "vertical" ? window.icons.chevron_left : window.icons.arrow_back
            onClicked: toolbarRoot.prevPage()
            contentItem: Text {
                text: parent.text; font.family: window.iconFont; font.pixelSize: 24; color: "#2D3436"
                horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                rotation: orientation === "vertical" ? 90 : 0
            }
        }

        Item {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: orientation === "vertical" ? 44 : 80
            Layout.preferredHeight: orientation === "vertical" ? 54 : 44
            
            Column {
                anchors.centerIn: parent
                spacing: 1
                Text { 
                    text: toolbarRoot.currentPage
                    font.pixelSize: 14
                    font.bold: true
                    anchors.horizontalCenter: parent.horizontalCenter
                    color: "#2D3436" 
                }
                Rectangle { 
                    width: 12; height: 1; color: "#CCC"
                    anchors.horizontalCenter: parent.horizontalCenter 
                }
                Text { 
                    text: toolbarRoot.pageCount
                    font.pixelSize: 10
                    color: "#999"
                    anchors.horizontalCenter: parent.horizontalCenter 
                }
            }
            
            MouseArea {
                anchors.fill: parent
                onClicked: pageInputPopup.open()
                cursorShape: Qt.PointingHandCursor
                hoverEnabled: true
            }
        }

        Popup {
            id: pageInputPopup
            y: orientation === "vertical" ? 0 : -60
            x: orientation === "vertical" ? 60 : 0
            width: 180
            height: 54
            padding: 8
            background: Rectangle {
                color: "#FAFAF8"
                border.color: "#E8E7E0"
                border.width: 1
                radius: 10
                layer.enabled: false
            }
            
            RowLayout {
                anchors.fill: parent
                anchors.margins: 4
                spacing: 6
                TextField {
                    id: pageInputField
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    placeholderText: "Page #"
                    font.pixelSize: 14
                    leftPadding: 12
                    rightPadding: 12
                    verticalAlignment: TextInput.AlignVCenter
                    horizontalAlignment: TextInput.AlignLeft
                    color: "#2D3436"
                    selectByMouse: true
                    validator: IntValidator { bottom: 1; top: toolbarRoot.pageCount }
                    background: Rectangle { 
                        color: "#F2F2F0" 
                        radius: 8
                        border.color: pageInputField.activeFocus ? "#466B50" : "transparent"
                        border.width: 1
                    }
                    onAccepted: {
                        let p = parseInt(text)
                        if (p >= 1 && p <= toolbarRoot.pageCount) {
                            toolbarRoot.goToPage(p - 1)
                            pageInputPopup.close()
                        }
                    }
                }
                ToolButton {
                    Layout.preferredWidth: 50
                    Layout.fillHeight: true
                    text: "Go"
                    onClicked: {
                        let p = parseInt(pageInputField.text)
                        if (p >= 1 && p <= toolbarRoot.pageCount) {
                            toolbarRoot.goToPage(p - 1)
                            pageInputPopup.close()
                        }
                    }
                    contentItem: Text {
                        text: parent.text
                        font.bold: true
                        font.pixelSize: 13
                        color: parent.pressed ? "#FFF" : "#466B50"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: parent.pressed ? "#466B50" : (parent.hovered ? "#E8E7E0" : "transparent")
                        radius: 8
                    }
                }
            }
            onOpened: {
                pageInputField.text = ""
                pageInputField.forceActiveFocus()
            }
        }

        SidebarButton {
            text: window.icons.chevron_right
            onClicked: toolbarRoot.nextPage()
            contentItem: Text {
                text: parent.text; font.family: window.iconFont; font.pixelSize: 24; color: "#2D3436"
                horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                rotation: orientation === "vertical" ? 90 : 0
            }
        }

        Rectangle { 
            Layout.alignment: Qt.AlignHCenter
            width: orientation === "vertical" ? 32 : 1
            height: orientation === "vertical" ? 1 : 32
            color: "#E8E7E0" 
        }

        SidebarButton { text: window.icons.zoom_in; onClicked: toolbarRoot.zoomIn() }
        
        Text {
            text: toolbarRoot.zoomLevel + "%"
            font.pixelSize: 11
            font.weight: Font.Bold
            color: "#466B50" // A bit of color for the zoom level
            Layout.alignment: Qt.AlignHCenter
        }

        SidebarButton { text: window.icons.zoom_out; onClicked: toolbarRoot.zoomOut() }

        SidebarButton { text: window.icons.fit_width; onClicked: toolbarRoot.fitToWidth() }
        SidebarButton { text: window.icons.fit_page; onClicked: toolbarRoot.fitToPage() }

        Rectangle { 
            Layout.alignment: Qt.AlignHCenter
            width: orientation === "vertical" ? 32 : 1
            height: orientation === "vertical" ? 1 : 32
            color: "#E8E7E0" 
        }

        SidebarButton { 
            text: window.icons.fullscreen
            onClicked: toolbarRoot.toggleFocus() 
        }
        
        SidebarButton {
            text: window.icons.dark_mode
            onClicked: toolbarRoot.nightMode = !toolbarRoot.nightMode
            contentItem: Text {
                text: parent.text; font.family: window.iconFont; font.pixelSize: 24; color: "#2D3436"
                opacity: toolbarRoot.nightMode ? 1.0 : 0.4
                horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                Behavior on opacity { NumberAnimation { duration: 200 } }
            }
        }

        SidebarButton {
            text: window.icons.rotate_right
            onClicked: toolbarRoot.pageRotation = (toolbarRoot.pageRotation + 90) % 360
            contentItem: Text {
                text: parent.text; font.family: window.iconFont; font.pixelSize: 24; color: "#2D3436"
                horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
            }
        }
        
        Item { 
            id: spacer
            Layout.fillHeight: orientation === "vertical"
            Layout.fillWidth: orientation === "horizontal"
            Layout.minimumHeight: 20
        }

        SidebarButton {
            text: window.icons.close
            onClicked: toolbarRoot.back()
            contentItem: Text {
                text: parent.text; font.family: window.iconFont; font.pixelSize: 24; color: "#E74C3C"
                horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
            }
        }
    }
}

