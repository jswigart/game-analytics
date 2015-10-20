import Qt3D 2.0
import Qt3D.Renderer 2.0
import QtQuick 2.2 as QQ2
import Analytics 1.0 as Analytics

Analytics.Scene {
    id: sceneRoot
	objectName: "Scene"
	
	property var entityComponent: Qt.createComponent("entity.qml")
	
    Camera {
        id: camera
		objectName: "Camera"
		
		// the signal feedback loop that propagates properties to the UI and back mess up when they set
		// fields like position, viewCenter, upVector, because the properties have internal dependencies
		// on each other, so for example the viewCenter starts moving unexpectedly
		property variant uneditableProperties: ["position","upVector","viewCenter"]
		
		property bool propertiesEditable: false
		
        projectionType: CameraLens.PerspectiveProjection
        fieldOfView: 45
        aspectRatio: _window.width / _window.height
        nearPlane : 0.5
        farPlane : 10000.0
        position: Qt.vector3d( 2500.0, 2500.0, 1000.0 )
        upVector: Qt.vector3d( 1.0, 0.0, 1.0 )
        viewCenter: Qt.vector3d( 0.0, 0.0, 0.0 )
    }

	FrameGraph {
		id: frameGraph
		objectName: "FrameGraph"
		
		activeFrameGraph: ForwardRenderer {
			clearColor: Qt.rgba(0.0, 1.0, 0.0, 1)
			camera: camera
		}
	}
	
    Configuration  {
        controlledCamera: camera
    }

    components: [
        frameGraph
    ]
	
	
}
