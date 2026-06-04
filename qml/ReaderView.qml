import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SereneReader 1.0

Item {
    id: readerRoot
    signal backToLibrary()

    property string documentPath: ""
    property string documentFileName: ""
    property alias currentPage: pdfReader.currentPage
    property alias zoom: pdfReader.zoom
    
    property alias pageRotation: pdfReader.pageRotation

    // Helper to format clean, readable titles inside the reader view
    function cleanTitle(raw) {
        if (!raw) return "Untitled Document";
        let t = raw.replace(/\.pdf$/i, "").trim();
        let lastSlash = Math.max(t.lastIndexOf("/"), t.lastIndexOf("\\"));
        if (lastSlash !== -1) {
            t = t.substring(lastSlash + 1);
        }
        let lower = t.toLowerCase();
        if (lower.indexOf("maths(10)") !== -1 || lower === "maths(10)" || lower === "math(10)") {
            return "Mathematics Class 10";
        }
        if (lower.indexOf("physics(11)") !== -1 || lower === "physics(11)" || lower === "physics 11") {
            return "Physics Class 11";
        }
        if (lower.indexOf("chemistry(11)") !== -1 || lower === "chemistry(11)" || lower === "chemistry 11") {
            return "Chemistry Class 11";
        }
        if (lower.indexOf("science(9)") !== -1 || lower === "science(9)") {
            return "Science Class 9";
        }
        if (lower.indexOf("prelim") !== -1) {
            return "Prelims Mock Paper";
        }
        if (lower.indexOf("chemistry") !== -1 && lower.indexOf("manjari") !== -1) {
            return "Organic Chemistry";
        }
        if (lower.indexOf("flow") !== -1) {
            return "Reader Flow Guide";
        }
        t = t.replace(/[_\-\+]+/g, " ");
        return t.replace(/\b\w/g, function(c) { return c.toUpperCase(); });
    }

    function loadDocument(path, lastPage, zoomVal, rotation) {
        documentPath = path
        documentFileName = path.split(/[\\/]/).pop()
        pdfReader.source = path
        pdfReader.currentPage = lastPage
        pdfReader.pageRotation = rotation
        // Respect saved zoom when available; otherwise let backend fit-to-page logic decide.
        if (zoomVal > 0) {
            pdfReader.zoom = zoomVal
        }
    }

    property bool isFullscreen: false

    Rectangle {
        anchors.fill: parent
        color: "#F0F0EE" 
    }

    Rectangle {
        id: header
        width: parent.width
        height: 64
        y: readerRoot.isFullscreen ? -height : 0
        clip: false // Clip is expensive
        opacity: readerRoot.isFullscreen ? 0 : 1
        visible: opacity > 0
        color: "#FDFCF7"
        z: 10
        border.color: "#E8E7E0"
        border.width: 1
        
        Behavior on y { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
        Behavior on opacity { NumberAnimation { duration: 200 } }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 24
            anchors.rightMargin: 24
            spacing: 16
            visible: !readerRoot.isFullscreen

            ToolButton {
                id: backBtn
                contentItem: Text {
                    text: window.icons.arrow_back
                    font.family: window.iconFont
                    font.pixelSize: 24
                    color: "#2D3436"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Item {}
                onClicked: readerRoot.backToLibrary()
            }

            Rectangle { width: 1; height: 24; color: "#E8E7E0" }

            Column {
                Layout.fillWidth: true
                spacing: 1
                Text {
                    text: readerRoot.cleanTitle(readerRoot.documentFileName)
                    font.family: "Inter"
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                    color: "#2D3436"
                    elide: Text.ElideRight
                }
                Text {
                    text: "Page " + (pdfReader.currentPage + 1) + " of " + pdfReader.pageCount
                    font.family: "Inter"
                    font.pixelSize: 11
                    color: "#94a3b8"
                }
            }

            Rectangle {
                width: 80; height: 32; radius: 16
                color: pdfReader.isZooming ? "#E8F5E9" : "#F0F0EE"
                
                Behavior on color { ColorAnimation { duration: 200 } }

                Text {
                    anchors.centerIn: parent
                    text: Math.round(pdfReader.zoom * 100) + "%"
                    font.pixelSize: 12; font.weight: Font.Bold; color: "#4A5D4E"
                    opacity: pdfReader.isZooming ? 0.6 : 1.0
                    Behavior on opacity { NumberAnimation { duration: 200 } }
                }
            }
        }
    }

    PdfReaderItem {
        id: pdfReader
        anchors.fill: parent
        // Respect the header height when not in fullscreen so the top of the page isn't covered
        anchors.topMargin: readerRoot.isFullscreen ? 0 : 64
        readerMode: window.readerMode
        freePan: window.freePan

        onCurrentPageChanged: saveTimer.restart()
    }

    Timer {
        id: saveTimer
        interval: 1000 // Save after 1 second of inactivity on a page
        repeat: false
        onTriggered: {
            if (!pdfReader.loading && readerRoot.documentPath !== "") {
                libraryModel.updateProgress(readerRoot.documentPath, pdfReader.currentPage, pdfReader.zoom, pdfReader.pageRotation)
            }
        }
    }

    // Beautiful activity indicator during background page decoding
    Rectangle {
        id: decodingIndicator
        anchors.top: parent.top
        anchors.topMargin: readerRoot.isFullscreen ? 24 : 88 // Push below header if visible
        anchors.horizontalCenter: parent.horizontalCenter
        height: 34
        radius: 17
        color: pdfReader.nightMode ? "#CC1A1A1A" : "#CCF8F9FA"
        border.color: pdfReader.nightMode ? "#33FFFFFF" : "#1A000000"
        border.width: 1
        z: 150
        visible: pdfReader.decoding && !pdfReader.loading && !pdfReader.currentPageReady
        width: contentLayout.width + 24

        Behavior on opacity {
            NumberAnimation { duration: 150 }
        }
        opacity: visible ? 1.0 : 0.0

        Row {
            id: contentLayout
            anchors.centerIn: parent
            spacing: 8
            
            Rectangle {
                width: 12
                height: 12
                radius: 6
                color: "transparent"
                border.color: pdfReader.nightMode ? "#39D353" : "#0066CC"
                border.width: 2
                anchors.verticalCenter: parent.verticalCenter

                RotationAnimator on rotation {
                    from: 0
                    to: 360
                    duration: 900
                    loops: Animation.Infinite
                    running: decodingIndicator.visible
                }
            }

            Text {
                text: "Rendering page..."
                font.family: "Inter"
                font.pixelSize: 11
                font.bold: true
                color: pdfReader.nightMode ? "#E0E0E0" : "#2C2C2C"
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }

    // Loading Overlay
    Rectangle {
        anchors.fill: parent
        color: "#FDFCF7" // Fully opaque cream background
        visible: pdfReader.loading
        z: 100
        
        ColumnLayout {
            anchors.centerIn: parent
            spacing: 24
            
            BusyIndicator {
                Layout.alignment: Qt.AlignHCenter
                running: pdfReader.loading
                palette.dark: "#4A5D4E"
            }
            
            Text {
                text: "Waking up your document..."
                font.family: "Serif"
                font.italic: true
                font.pixelSize: 18
                color: "#4A5D4E"
                Layout.alignment: Qt.AlignHCenter
            }
        }
    }

    // Google-like right scrollbar: standard drag behavior, no custom easing quirks.
    Item {
        id: rightScrollHost
        anchors.right: parent.right
        anchors.rightMargin: 10
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        anchors.topMargin: 12
        anchors.bottomMargin: 12
        width: 14
        z: 150
        visible: pdfReader.pageCount > 1

        ScrollBar {
            id: rightScrollBar
            anchors.fill: parent
            orientation: Qt.Vertical
            policy: ScrollBar.AlwaysOn
            active: hovered || pressed
            size: Math.max(0.06, Math.min(0.45, 1.0 / Math.max(1, pdfReader.pageCount)))
            position: Math.max(0.0, Math.min(1.0 - size, pdfReader.verticalScrollRatio * (1.0 - size)))

            onPressedChanged: {
                pdfReader.isDragging = pressed
            }

            onPositionChanged: {
                if (pressed) {
                    let denom = Math.max(0.0001, (1.0 - size))
                    pdfReader.scrollToRatio(position / denom)
                }
            }

            contentItem: Rectangle {
                implicitWidth: 10
                radius: 5
                color: rightScrollBar.pressed ? "#6D7378" : (rightScrollBar.hovered ? "#8A9096" : "#B8BDC3")
                opacity: rightScrollBar.active ? 0.95 : 0.65
            }

            background: Rectangle {
                radius: 7
                color: "#22000000"
                border.color: "#16000000"
            }
        }
    }

    PinchHandler {
        id: pinchHandler
        target: null
        property real initialZoom: 1.0
        property point initialCentroid: Qt.point(0, 0)
        property point initialScroll: Qt.point(0, 0)

        onActiveChanged: {
            pdfReader.isPinching = active;
            if (active) {
                initialZoom = pdfReader.zoom;
                initialCentroid = centroid.position;
                initialScroll = Qt.point(pdfReader.scrollX, pdfReader.scrollY);
            }
        }
        
        onScaleChanged: {
            let newZoom = Math.min(4.0, Math.max(0.1, initialZoom * scale));
            let actualScale = newZoom / initialZoom;
            
            // Zoom at pinch centroid
            let centerX = pdfReader.width / 2.0;
            let centerY = pdfReader.height / 2.0;
            
            let focusX = initialCentroid.x - centerX;
            let focusY = initialCentroid.y - centerY;
            
            let targetScrollX = focusX - (focusX - initialScroll.x) * actualScale;
            let targetScrollY = focusY - (focusY - initialScroll.y) * actualScale;
            
            pdfReader.setZoomAndScroll(newZoom, targetScrollX, targetScrollY);
        }
    }

    // Floating Toolbar
    FloatingToolbar {
        id: toolbar
        z: 110
        
        // Placement logic
        anchors.left: window.toolbarPosition === "left" ? parent.left : undefined
        anchors.right: window.toolbarPosition === "right" ? parent.right : undefined
        anchors.bottom: window.toolbarPosition === "bottom" ? parent.bottom : undefined
        anchors.verticalCenter: (window.toolbarPosition !== "bottom") ? parent.verticalCenter : undefined
        anchors.horizontalCenter: (window.toolbarPosition === "bottom" || window.toolbarPosition === "center") ? parent.horizontalCenter : undefined
        
        anchors.margins: 32
        
        // Improved vertical sizing for side-bars
        height: orientation === "vertical" ? Math.min(parent.height - 100, 560) : 64
        width: orientation === "horizontal" ? Math.min(parent.width - 100, 600) : 64

        orientation: window.toolbarPosition === "bottom" ? "horizontal" : "vertical"
        performanceMode: window.performanceMode
        
        currentPage: pdfReader.currentPage + 1
        pageCount: pdfReader.pageCount
        zoomLevel: Math.round(pdfReader.zoom * 100)
        pageRotation: pdfReader.pageRotation
        nightMode: pdfReader.nightMode
        
        onPageRotationChanged: pdfReader.pageRotation = pageRotation
        onNightModeChanged: pdfReader.nightMode = nightMode
        
        onBack: readerRoot.backToLibrary()
        onNextPage: pdfReader.currentPage = pdfReader.currentPage + 1
        onPrevPage: pdfReader.currentPage = pdfReader.currentPage - 1
        onZoomIn: {
            let next = Math.round((pdfReader.zoom + 0.1) * 10) / 10;
            pdfReader.zoom = Math.min(4.0, Math.max(0.1, next));
        }
        onZoomOut: {
            let next = Math.round((pdfReader.zoom - 0.1) * 10) / 10;
            pdfReader.zoom = Math.min(4.0, Math.max(0.1, next));
        }
        onGoToPage: (page) => pdfReader.currentPage = page
        onFitToWidth: pdfReader.fitToWidth()
        onFitToPage: pdfReader.fitToPage()
        onToggleFocus: readerRoot.isFullscreen = !readerRoot.isFullscreen
    }

    // Vertical Zoom Slider
    VerticalZoomSlider {
        id: zoomSlider
        zoom: pdfReader.zoom
        z: 120
        
        // Placement logic: strictly opposite of toolbar
        // toolbarPosition can be "left", "right", "bottom", or "center"
        property string tPos: window.toolbarPosition
        anchors.left: (tPos === "right" || tPos === "bottom" || tPos === "center") ? parent.left : undefined
        anchors.right: tPos === "left" ? parent.right : undefined
        
        anchors.verticalCenter: parent.verticalCenter
        anchors.margins: 48
        
        onZoomRequest: (val) => {
            pdfReader.zoom = val
        }
        
        visible: !pdfReader.loading
    }

}

