import Qt3D 2.0
import Qt3D.Renderer 2.0
import QtQuick 2.2 as QQ2
import Analytics 1.0 as Analytics

Analytics.Scene {
    id: sceneRoot

    Camera {
        id: camera
        projectionType: CameraLens.PerspectiveProjection
        fieldOfView: 45
        aspectRatio: _window.width / _window.height
        nearPlane : 0.5
        farPlane : 10000.0
        position: Qt.vector3d( 5000.0, 0.0, 0.0 )
        upVector: Qt.vector3d( 0.0, 0.0, 1.0 )
        viewCenter: Qt.vector3d( 0.0, 0.0, 0.0 )
    }

    Configuration  {
        controlledCamera: camera
    }

    components: [
        FrameGraph {
            activeFrameGraph: ForwardRenderer {
                clearColor: Qt.rgba(0.0, 1.0, 0.0, 1)
                camera: camera
            }
        }
    ]
}
