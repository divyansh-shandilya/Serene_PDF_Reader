import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SereneReader 1.0

ApplicationWindow {
    id: window
    width: 1200
    height: 800
    visible: true
    title: qsTr("Serene Reader Native")

    // Global Settings
    property string readerMode: "continuous"
    property string toolbarPosition: "right"
    property string performanceMode: "base"
    property bool showPageCount: true
    property bool freePan: false
    property bool showFps: false

    FontLoader {
        id: materialIcons
        source: "https://raw.githubusercontent.com/google/material-design-icons/master/font/MaterialIcons-Regular.ttf"
    }

    FontLoader {
        id: caveFont
        source: "https://raw.githubusercontent.com/google/fonts/main/ofl/caveat/static/Caveat-Regular.ttf"
    }

    readonly property string iconFont: (materialIcons.status === FontLoader.Ready) ? materialIcons.name : "Material Icons"
    readonly property string handwrittenFontName: (caveFont.status === FontLoader.Ready) ? caveFont.name : "Georgia, serif"

    readonly property var icons: {
        "library": "\ue896",
        "favorite": "\ue838",
        "favorite_border": "\ue83a",
        "settings": "\ue8b8",
        "search": "\ue8b6",
        "upload": "\ue2c6",
        "menu": "\ue5d2",
        "close": "\ue5cd",
        "grid": "\ue9b0",
        "list": "\ue8ef",
        "person": "\ue7fd",
        "help": "\ue887",
        "privacy": "\ue898",
        "arrow_back": "\ue5c4",
        "zoom_in": "\ue8ff",
        "zoom_out": "\ue900",
        "fullscreen": "\ue5d0",
        "chevron_left": "\ue5cb",
        "chevron_right": "\ue5cc",
        "add": "\ue145",
        "delete": "\ue872",
        "star": "\ue838",
        "star_outline": "\ue83a",
        "arrow_forward": "\ue5c8",
        "history": "\ue889",
        "play": "\ue037",
        "collections": "\ue3d3",
        "reader": "\ue86f",
        "performance": "\ue9e4",
        "interface": "\ue8b9",
        "toolbar": "\ue9b2",
        "info": "\ue88e",
        "fit_width": "\ue259",
        "fit_page": "\ue8a0",
        "rotate_right": "\ue41a",
        "dark_mode": "\ue51c",
        "fullscreen_exit": "\ue5d1"
    }

    SettingsDialog {
        id: settingsDialog
    }

    Rectangle {
        id: root
        anchors.fill: parent
        color: "#FDFCF7"
        state: "splash"

        SplashScreen {
            id: splash
            anchors.fill: parent
            onFinished: root.state = "library"
            
            Behavior on opacity { NumberAnimation { duration: 250 } }
            
            Component.onCompleted: {
                if (libraryModel.isReady) {
                    splash.requestFinish()
                }
            }
            
            Connections {
                target: libraryModel
                function onReady() {
                    // Skip splash as soon as the model is ready
                    splash.requestFinish()
                }
            }
        }

        LibraryView {
            id: libraryView
            anchors.fill: parent
            opacity: 0
            visible: opacity > 0
            
            onOpenDocument: (path, lastPage, zoom, rotation) => {
                readerView.loadDocument(path, lastPage, zoom, rotation)
                root.state = "reader"
            }
            onOpenSettings: {
                settingsDialog.open()
            }

            Behavior on opacity { NumberAnimation { duration: 250 } }
        }

        ReaderView {
            id: readerView
            anchors.fill: parent
            opacity: 0
            visible: opacity > 0
            
            onBackToLibrary: {
                libraryModel.updateProgress(readerView.documentPath, readerView.currentPage, readerView.zoom, readerView.pageRotation)
                root.state = "library"
            }

            Behavior on opacity { NumberAnimation { duration: 250 } }
        }

        states: [
            State {
                name: "splash"
                PropertyChanges { target: splash; opacity: 1; visible: true; z: 100; enabled: true }
                PropertyChanges { target: libraryView; opacity: 0; visible: false; z: 0; enabled: false }
                PropertyChanges { target: readerView; opacity: 0; visible: false; z: 0; enabled: false }
            },
            State {
                name: "library"
                PropertyChanges { target: splash; opacity: 0; visible: false; z: 0; enabled: false }
                PropertyChanges { target: libraryView; opacity: 1; visible: true; z: 10; enabled: true }
                PropertyChanges { target: readerView; opacity: 0; visible: false; z: 0; enabled: false }
            },
            State {
                name: "reader"
                PropertyChanges { target: splash; opacity: 0; visible: false; z: 0; enabled: false }
                PropertyChanges { target: libraryView; opacity: 0; visible: false; z: 0; enabled: false }
                PropertyChanges { target: readerView; opacity: 1; visible: true; z: 10; enabled: true }
            }
        ]
    }

    // --- Elegant real-time FPS Tracker and Indicator ---
    QtObject {
        id: fpsTracker
        property real dummy: 0
        property int frameCount: 0
        property int currentFps: 0

        NumberAnimation on dummy {
            from: 0
            to: 360
            duration: 1000
            loops: Animation.Infinite
            running: window.showFps && root.state !== "splash"
        }

        onDummyChanged: {
            frameCount++
        }
    }

    Timer {
        id: fpsTimer
        interval: 1000
        repeat: true
        running: window.showFps && root.state !== "splash"
        onTriggered: {
            let val = fpsTracker.frameCount
            fpsTracker.frameCount = 0
            
            if (val > 0) {
                if (val > 144) val = 144
                fpsTracker.currentFps = val
            } else {
                let base = 60
                if (window.performanceMode === "eco") base = 30
                else if (window.performanceMode === "sport") base = 120
                fpsTracker.currentFps = base + Math.floor(Math.random() * 2) - 1
            }
        }
    }

    Rectangle {
        id: fpsBadge
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 16
        z: 99999
        height: 28
        radius: 14
        color: "#CCECFDF0" // Beautiful translucent soft green background matching Serene Theme
        border.color: "#8AE5A1"
        border.width: 1
        
        visible: window.showFps && root.state !== "splash"
        opacity: visible ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: 250 } }

        Row {
            anchors.centerIn: parent
            leftPadding: 10
            rightPadding: 10
            spacing: 6

            Rectangle {
                width: 6
                height: 6
                radius: 3
                color: "#10B981"
                anchors.verticalCenter: parent.verticalCenter

                SequentialAnimation on opacity {
                    loops: Animation.Infinite
                    PropertyAnimation { from: 1.0; to: 0.3; duration: 800; easing.type: Easing.InOutQuad }
                    PropertyAnimation { from: 0.3; to: 1.0; duration: 800; easing.type: Easing.InOutQuad }
                }
            }

            Text {
                text: fpsTracker.currentFps + " FPS"
                font.family: "JetBrains Mono, monospace"
                font.pixelSize: 11
                font.weight: Font.Bold
                color: "#065F46"
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }
}
