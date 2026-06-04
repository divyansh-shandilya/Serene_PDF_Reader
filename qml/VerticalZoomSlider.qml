import QtQuick
import QtQuick.Controls

Rectangle {
    id: zoomSliderRoot
    property real zoom: 1.0
    signal zoomRequest(real value)
    
    readonly property bool active: touchArea.touchPoints.length > 0 || mouseArea.pressed

    width: 44
    height: 320
    color: "#FAFAF8"
    radius: 22
    border.color: "#E8E7E0"
    border.width: 1
    
    layer.enabled: true
    layer.smooth: true
    
    // Smooth transition when appearing
    opacity: 0
    visible: opacity > 0
    Component.onCompleted: opacity = 1
    Behavior on opacity { NumberAnimation { duration: 300 } }

    Column {
        anchors.fill: parent
        anchors.margins: 4
        spacing: 8

        Rectangle {
            width: 36
            height: 36
            radius: 18
            color: mouseArea.containsMouse ? "#E8E7E0" : "transparent"
            Text {
                anchors.centerIn: parent
                text: window.icons.zoom_in
                font.family: window.iconFont
                font.pixelSize: 22
                color: "#2D3436"
            }
            MouseArea {
                anchors.fill: parent
                onClicked: zoomSliderRoot.zoomRequest(Math.min(3.0, zoomSliderRoot.zoom + 0.2))
            }
        }

        Item {
            id: sliderTrack
            width: 36
            height: parent.height - 36 - 36 - 24
            anchors.horizontalCenter: parent.horizontalCenter

            // Background Track
            Rectangle {
                id: bgRect
                width: 6
                height: parent.height
                radius: 3
                color: "#F0F0EE"
                anchors.centerIn: parent

                Rectangle {
                    width: parent.width
                    height: (1.0 - sliderPos) * parent.height
                    color: "#466B50"
                    radius: 3
                    anchors.bottom: parent.bottom
                }
            }

            // Handle
            Rectangle {
                id: handleRect
                anchors.horizontalCenter: parent.horizontalCenter
                y: sliderPos * (parent.height - height)
                width: 28
                height: 28
                radius: 14
                color: zoomSliderRoot.active ? "#466B50" : "white"
                border.color: "#466B50"
                border.width: 2
                scale: zoomSliderRoot.active ? 1.1 : 1.0
                
                antialiasing: true
                Behavior on scale { NumberAnimation { duration: 100 } }
                Behavior on color { ColorAnimation { duration: 100 } }
                
                // Value indicator
                Rectangle {
                    anchors.right: parent.left
                    anchors.rightMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    width: 48
                    height: 24
                    radius: 6
                    color: "#2D3436"
                    visible: zoomSliderRoot.active || mouseArea.containsMouse
                    
                    Text {
                        anchors.centerIn: parent
                        text: Math.round(zoomSliderRoot.zoom * 100) + "%"
                        color: "white"
                        font.pixelSize: 11
                        font.weight: Font.Bold
                    }
                    
                    // Small arrow
                    Rectangle {
                        width: 6; height: 6; color: "#2D3436"
                        anchors.right: parent.right
                        anchors.rightMargin: -3
                        anchors.verticalCenter: parent.verticalCenter
                        rotation: 45
                    }
                }
            }
        }

        Rectangle {
            width: 36
            height: 36
            radius: 18
            color: mouseArea.containsMouse ? "#E8E7E0" : "transparent"
            Text {
                anchors.centerIn: parent
                text: window.icons.zoom_out
                font.family: window.iconFont
                font.pixelSize: 22
                color: "#2D3436"
            }
            MouseArea {
                anchors.fill: parent
                onClicked: zoomSliderRoot.zoomRequest(Math.max(0.1, zoomSliderRoot.zoom - 0.2))
            }
        }
    }

    // --- Interaction Logic ---
    property real sliderPos: zoomToValue(zoomSliderRoot.zoom)
    property real lastInputY: 0

    function zoomToValue(z) {
        let cz = Math.max(0.1, Math.min(3.0, z));
        return 1.0 - (Math.log(cz) - Math.log(0.1)) / (Math.log(3.0) - Math.log(0.1))
    }
    function valueToZoom(v) {
        let cv = Math.max(0.0, Math.min(1.0, v));
        return Math.exp((1.0 - cv) * (Math.log(3.0) - Math.log(0.1)) + Math.log(0.1))
    }

    function handleInput(localY, isRelative = false) {
        let trackHeight = sliderTrack.height;
        if (trackHeight <= 0) return;
        
        let pos = 0;
        if (isRelative) {
            let dy = localY - touchArea.startY;
            // 0.4 multiplier for smoother, less sensitive control as per user request
            pos = Math.min(1.0, Math.max(0.0, touchArea.startSliderPos + (dy / trackHeight) * 0.4));
        } else {
            pos = localY / trackHeight;
            pos = Math.min(1.0, Math.max(0.0, pos));
        }
        
        let targetZoom = valueToZoom(pos);
        
        // Snap to exactly 1.0 (100%) when close to it
        if (Math.abs(targetZoom - 1.0) < 0.05) {
            targetZoom = 1.0;
        }
        
        if (Math.abs(targetZoom - zoomSliderRoot.zoom) > 0.001) {
            zoomSliderRoot.zoomRequest(targetZoom);
        }
    }

    Item {
        id: interactionOverlay
        anchors.fill: parent // Interaction over the whole panel
        anchors.margins: -12 

        MultiPointTouchArea {
            id: touchArea
            anchors.fill: parent
            mouseEnabled: true
            
            property real startY: 0
            property real startSliderPos: 0

            onPressed: (touchPoints) => {
                startY = touchPoints[0].y;
                startSliderPos = zoomSliderRoot.sliderPos;
                
                // Allow direct jump only if they click near the handle or specific areas?
                // Actually, user said "how much I slide", so relative is better.
                // But let's keep a direct jump on internal click for convenience.
                let trackY = sliderTrack.mapFromItem(touchArea, 0, touchPoints[0].y).y;
                if (Math.abs(trackY / sliderTrack.height - startSliderPos) < 0.1) {
                    // Holding near handle
                } else {
                    handleInput(trackY);
                    startSliderPos = zoomSliderRoot.sliderPos; // Update after jump
                }
            }
            
            onUpdated: (touchPoints) => {
                if (touchPoints.length > 0) {
                    let avgY = 0;
                    for (let i = 0; i < touchPoints.length; i++) avgY += touchPoints[i].y;
                    avgY /= touchPoints.length;
                    
                    handleInput(avgY, true);
                }
            }
            onReleased: (touchPoints) => {
                startY = 0;
                startSliderPos = 0;
            }
        }

        MouseArea {
            id: mouseArea
            anchors.fill: parent
            enabled: touchArea.touchPoints.length === 0
            hoverEnabled: true
            preventStealing: true
            
            property real startY: 0
            property real startSliderPos: 0

            onPressed: (mouse) => {
                startY = mouse.y;
                startSliderPos = zoomSliderRoot.sliderPos;
                
                let trackY = sliderTrack.mapFromItem(mouseArea, 0, mouse.y).y;
                handleInput(trackY);
                startSliderPos = zoomSliderRoot.sliderPos;
            }
            onPositionChanged: (mouse) => {
                if (pressed) {
                    handleInput(mouse.y, true);
                }
            }
        }
    }

    // Precision Indicator
    Rectangle {
        anchors.bottom: parent.top
        anchors.bottomMargin: 12
        anchors.horizontalCenter: parent.horizontalCenter
        width: 80; height: 26; radius: 13
        color: "#466B50"
        visible: touchArea.touchPoints.length === 2
        opacity: visible ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: 200 } }
        
        Text {
            anchors.centerIn: parent
            text: "PRECISION"
            color: "white"
            font.pixelSize: 10
            font.weight: Font.Bold
        }
        
        SequentialAnimation on opacity {
            running: touchArea.touchPoints.length === 2
            loops: Animation.Infinite
            NumberAnimation { from: 1; to: 0.6; duration: 500 }
            NumberAnimation { from: 0.6; to: 1; duration: 500 }
        }
    }
}
