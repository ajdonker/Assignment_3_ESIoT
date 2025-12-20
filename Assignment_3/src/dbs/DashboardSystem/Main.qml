import QtQuick 2.15
import QtQuick.Controls 2.15

ApplicationWindow {
    visible: true
    width: 400
    height: 300
    title: "Water Control System"
    Component.onCompleted: {
        console.log("QML alive")
    }
    property string state: "UNKNOWN"
    property real level: 0.0

    Timer {
        interval: 5000
        running: true
        repeat: true
        onTriggered: {
            var xhr = new XMLHttpRequest()
            xhr.open("GET", "http://127.0.0.1:5000/status")
            xhr.onreadystatechange = function() {
                if (xhr.readyState === XMLHttpRequest.DONE && xhr.status === 200) {
                    console.log("Response:", xhr.responseText)
                    var data = JSON.parse(xhr.responseText)
                    state = data.state
                    level = (data.water_level !== null && data.water_level !== undefined)
                            ? data.water_level
                            : 0
                }
            }
            xhr.send()
        }
    }

    Column {
        anchors.centerIn: parent
        spacing: 10

        Text { text: "State: " + state }
        Text { text: "Water level: " + level }

        Button {
            text: "Toggle AUTO / MANUAL"
            onClicked: {
                var newMode = state === "AUTOMATIC" ? "MANUAL" : "AUTOMATIC"
                var xhr = new XMLHttpRequest()
                xhr.open("POST", "http://127.0.0.1:5000/mode")
                xhr.setRequestHeader("Content-Type", "application/json")
                xhr.send(JSON.stringify({ mode: newMode }))
            }
        }

        Slider {
            from: 0
            to: 90
            enabled: state === "MANUAL"
            onValueChanged: {
                var xhr = new XMLHttpRequest()
                xhr.open("POST", "http://127.0.0.1:5000/motor")
                xhr.setRequestHeader("Content-Type", "application/json")
                xhr.send(JSON.stringify({ position: Math.round(value) }))
            }
        }
    }
}

