import Qt3D.Core 2.0
import Qt3D.Render 2.0
import QtQuick 2.5 as QQ2
import Qt3D.Input 2.0
import Qt3D.Extras 2.0
import Analytics 1.0 as Analytics

Analytics.Scene 
{
    id: sceneRoot

    Camera 
    {
        id: camera
        projectionType: CameraLens.PerspectiveProjection
        fieldOfView: 45
        aspectRatio: 16/9
        nearPlane : 0.1
        farPlane : 10000.0
        position: Qt.vector3d( 0.0, 0.0, -40.0 )
        upVector: Qt.vector3d( 0.0, 1.0, 0.0 )
        viewCenter: Qt.vector3d( 0.0, 0.0, 0.0 )
    }

    FirstPersonCameraController  
    {
        camera: camera
		linearSpeed: 1000		
    }

    components: 
    [
        RenderSettings 
        {
            activeFrameGraph: ForwardRenderer 
            {
                clearColor: Qt.rgba(0, 0.5, 1, 1)
                camera: camera
            }
        },
        // Event Source will be set by the Qt3DQuickWindow
        InputSettings 
        {            
        },
        ObjectPicker 
        {
            dragEnabled : false
            hoverEnabled : false

            onClicked: 
            {
                console.trace("onClicked")
                if (pick.button == PickEvent.LeftButton) 
                {
                    console.log("picked ", pick);
                }
            }
        }
    ]
}