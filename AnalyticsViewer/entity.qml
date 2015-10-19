import Qt3D 2.0
import Qt3D.Renderer 2.0
import QtQuick 2.2 as QQ2
import Analytics 1.0 as Analytics

Entity {
	id: entity
	objectName: "Entity"
	
	property double positionX: 0.0
	property double positionY: 0.0
	property double positionZ: 0.0	
	
	property double heading: 0.0
	property double pitch: 0.0
	property double roll: 0.0
	
	CylinderMesh {
		id: mesh
		radius: 32
		length: 72
		rings: 100
		slices: 20
	}

	Material {
		id: material
		effect : Effect {
		}
	}
	
	Transform {
		id: xform
		
		Rotate {
            axis : Qt.vector3d(0, 0, 1)
            angle : heading
        }

        Rotate {
            axis : Qt.vector3d(1, 0, 0)
            angle : pitch
        }

        Rotate {
            axis: Qt.vector3d(0, 1, 0)
            angle : roll
        }
		
		Translate {
			id: trans
			dx : positionX
			dy : positionY
			dz : positionZ
		}
	}
	components: [ mesh, xform, material ]
}
