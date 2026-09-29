pragma ComponentBehavior: Bound

import QtQuick
import org.kde.kirigami as Kirigami

// A station favicon, falling back to a themed icon while loading or when missing.
Item {
    id: icon

    property alias source: image.source

    Image {
        id: image
        anchors.fill: parent
        sourceSize.width: width * Screen.devicePixelRatio
        sourceSize.height: height * Screen.devicePixelRatio
        fillMode: Image.PreserveAspectFit
        asynchronous: true
        smooth: true
        visible: status === Image.Ready
    }
    Kirigami.Icon {
        anchors.fill: parent
        source: "radio"
        visible: !image.visible
    }
}
