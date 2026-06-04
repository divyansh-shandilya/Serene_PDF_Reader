import QtQuick
import QtQuick.Controls

Item {
    id: splashContainer
    signal finished()
    signal requestFinish()

    onRequestFinish: finished()

    Rectangle {
        anchors.fill: parent
        color: "#FDFCF7"

        Column {
            anchors.centerIn: parent
            spacing: 20

            // Beautiful Brand Logo
            Image {
                width: 96
                height: 96
                source: "logo.svg"
                sourceSize.width: 96
                sourceSize.height: 96
                smooth: true
                antialiasing: true
                anchors.horizontalCenter: parent.horizontalCenter

                SequentialAnimation on scale {
                    loops: Animation.Infinite
                    PropertyAnimation { from: 1.0; to: 1.05; duration: 1200; easing.type: Easing.InOutQuad }
                    PropertyAnimation { from: 1.05; to: 1.0; duration: 1200; easing.type: Easing.InOutQuad }
                }
            }

            Text {
                text: "Serene Reader"
                font.family: "Serif"
                font.pointSize: 26
                font.letterSpacing: 2
                color: "#4A5D4E"
                anchors.horizontalCenter: parent.horizontalCenter
                
                opacity: 0
                ScaleAnimator on scale {
                    from: 0.9; to: 1.0; duration: 800; easing.type: Easing.OutCubic
                    running: true
                }
                OpacityAnimator on opacity {
                    from: 0; to: 1; duration: 800; easing.type: Easing.OutCubic
                    running: true
                }
            }
        }
    }

    Timer {
        id: splashTimer
        interval: 1500 // Slightly longer to appreciate the logo
        running: true
        onTriggered: splashContainer.finished()
    }
}
