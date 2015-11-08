import Qt3D 2.0
import Qt3D.Renderer 2.0
import QtQuick 2.5
import Qt3D.Input 2.0
import Analytics 1.0 as Analytics

Analytics.Scene {
    id: scenecameraSpeed
	objectName: "Scene"
	
	property var entityComponent: Qt.createComponent("entity.qml")
	property vector3d cameraSpeed: Qt.vector3d(256.0,256.0,0.0)
	property real cameraShiftScale: 5.0
	
    KeyboardController {
        id: keyboardController
    }
	MouseController {
        id: mouseController
    }
	
	KeyboardInput { 
		id: inputKb 
		focus: true
		controller: keyboardController
		
		property real moveFwd: 0.0
		property real moveSide: 0.0
		property real moveScale: 1.0
		
		onTabPressed: {
			console.log("onTabPressed");
		}
		
		onPressed: {
			console.log("onPressed " + event.text);
			
			if (event.key == Qt.Key_Shift) {
				moveScale = cameraShiftScale;
			}
			
			if ( event.key == Qt.Key_W ) {
				moveFwd = cameraSpeed.x;
			} else if ( event.key == Qt.Key_S ) {
				moveFwd = -cameraSpeed.x;
			} else if ( event.key == Qt.Key_A ) {
				moveSide = -cameraSpeed.y;
			} else if ( event.key == Qt.Key_D ) {
				moveSide = cameraSpeed.y;
			}
		}
		onReleased: {
			console.log("onReleased " + event.text);
			
			if ( event.key == Qt.Key_W || event.key == Qt.Key_S ) {
				moveFwd = 0.0;			
			} else if ( event.key == Qt.Key_A || event.key == Qt.Key_D ) {
				moveSide = 0.0;
			}
			
			if ( event.key == Qt.Key_Shift ) {
				moveScale = 1.0;
			}
		}	
	}
	
	MouseInput {
		id: inputMouse
		controller: mouseController
		
		property bool rotating: false
		
		property int lastMouseX: 0
		property int lastMouseY: 0
		
		onPressed: {
			console.log("onPressed " + mouse.button);
		}
		onReleased: {
			console.log("onReleased " + mouse.button);
			
			if ( mouse.button == 2 ) {
				rotating = false;
			}
		}
		onPressAndHold: {
			console.log("onPressAndHold " + mouse.button);
			
			if ( mouse.button == 2 ) {
				rotating = true;
			}
		}
		onPositionChanged: {
			if ( rotating ) {
				var deltaX = mouse.x - lastMouseX;
				var deltaY = mouse.y - lastMouseY;
				
				console.log("delta " + deltaX + " " + deltaY);
			}
			
			lastMouseX = mouse.x;
			lastMouseY = mouse.y;
		}
		onWheel: {
			console.log("onWheel");
		}
	}
	
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
        upVector: Qt.vector3d( 0.0, 0.0, 1.0 )
        viewCenter: Qt.vector3d( 0.0, 0.0, 0.0 )
		
		Timer { 
			id: time 
			interval: 5;
			running: true;
			repeat: true
			
			property real dt: interval/1000;
			
			onTriggered:{ 
				if ( inputKb.moveFwd != 0.0 ) {
					var fwd = camera.viewCenter.minus(camera.position).normalized();
					var delta = fwd.times( dt ).times( inputKb.moveFwd ).times( inputKb.moveScale );
					
					//console.log("delta " + delta );
					
					camera.position = camera.position.plus( delta );
					camera.viewCenter = camera.viewCenter.plus( delta );
				}
				if ( inputKb.moveSide != 0.0 ) {
					var fwd = camera.viewCenter.minus(camera.position).normalized();
					var side = fwd.crossProduct( Qt.vector3d(0,0,1) );					
					var delta = side.times( dt ).times( inputKb.moveSide ).times( inputKb.moveScale );
					
					//console.log("delta " + delta );
					
					camera.position = camera.position.plus( delta );
					camera.viewCenter = camera.viewCenter.plus( delta );
				}
			}
		}
    }

	FrameGraph {
		id: frameGraph
		objectName: "FrameGraph"
		
		activeFrameGraph: ForwardRenderer {
			clearColor: Qt.rgba(0.0, 1.0, 0.0, 1)
			camera: camera
		}
	}
	
    //Configuration  {
    //    controlledCamera: camera
    //}

    components: [
        frameGraph,
		inputKb,
		inputMouse,
    ]
	
	
}
