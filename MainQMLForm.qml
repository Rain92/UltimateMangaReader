import QtQuick 2.4
import QtQuick.Controls 2.13
import QtQuick.Layouts 1.3
import Qt.labs.animation 1.0

Item {
    id: element
    anchors.fill: parent

    Rectangle {
        id: rectangle
        height: 80
        color: "#ffffff"
        anchors.bottom: parent.bottom
        Layout.fillWidth: true
        anchors.bottomMargin: 80

        RowLayout {
            id: row
            spacing: 0
            anchors.fill: parent



            Button {
                id: button1
                text: qsTr("Button1")
                Layout.rightMargin: 2
                Layout.bottomMargin: 2
                Layout.leftMargin: 2
                Layout.topMargin: 2
                Layout.fillWidth: true
                Layout.fillHeight: true

            }
            Button {
                id: button2
                text: qsTr("Button2")
                Layout.rightMargin: 0
                Layout.bottomMargin: 2
                Layout.leftMargin: 0
                Layout.topMargin: 2
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
            }

            Button {
                id: button3
                text: qsTr("Button3")
                Layout.rightMargin: 2
                Layout.bottomMargin: 2
                Layout.leftMargin: 2
                Layout.topMargin: 2
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
            }
            }

    }
}

/*##^##
Designer {
    D{i:0;autoSize:true;height:480;width:640}
}
##^##*/
