#include <QtWidgets/QApplication>
#include <Qt3DRenderer/QMesh>
#include <Qt3DRenderer/QMeshData>
#include <Qt3DRenderer/BufferPtr>
#include <Qt3DRenderer/AttributePtr>
#include <Qt3DCore/QTransform>
#include <Qt3DCore/QMatrixTransform>
#include <Qt3DRenderer/QCylinderMesh>
#include <Qt3DRenderer/QPhongMaterial>
#include <QDebug>

#include "ModelProcessor.h"
#include "GameAnalytics.h"

#include "analytics.pb.h"

//////////////////////////////////////////////////////////////////////////

class QAnalyticMeshFunctor : public Qt3D::QAbstractMeshFunctor
{
public:
	QAnalyticMeshFunctor( Qt3D::QMeshDataPtr data );
	Qt3D::QMeshDataPtr operator()() Q_DECL_OVERRIDE;
	bool operator ==( const Qt3D::QAbstractMeshFunctor &other ) const Q_DECL_OVERRIDE;
private:
	Qt3D::QMeshDataPtr m_data;
};

//////////////////////////////////////////////////////////////////////////

QAnalyticModel::QAnalyticModel( QNode *parent, Qt3D::QMeshDataPtr data )
	: Qt3D::QAbstractMesh( parent )
	, m_data( data )
{
}

Qt3D::QAbstractMeshFunctorPtr QAnalyticModel::meshFunctor() const
{
	return Qt3D::QAbstractMeshFunctorPtr( new QAnalyticMeshFunctor( m_data ) );
}

QAnalyticMeshFunctor::QAnalyticMeshFunctor( Qt3D::QMeshDataPtr data )
	: QAbstractMeshFunctor()
	, m_data( data )
{
}

Qt3D::QMeshDataPtr QAnalyticMeshFunctor::operator()()
{
	return m_data;
}

bool QAnalyticMeshFunctor::operator ==( const QAbstractMeshFunctor &other ) const
{
	const QAnalyticMeshFunctor *otherFunctor = dynamic_cast<const QAnalyticMeshFunctor *>( &other );
	if ( otherFunctor != Q_NULLPTR )
		return ( otherFunctor->m_data == m_data );
	return false;
}

//////////////////////////////////////////////////////////////////////////

ModelProcessor::ModelProcessor( QObject *parent )
	: QThread( parent )
	, mRunning( false )
{
	// connect message handling to the parent
}

void ModelProcessor::run()
{
	mRunning = true;

	while ( mRunning )
	{
		sleep( 0 );

		MessageUnionPtr msg;

		{
			QMutexLocker lock( &mMutex );
			if ( !mMessageQueue.isEmpty() )
			{
				msg = mMessageQueue.front();
				mMessageQueue.pop_front();
			}
		}

		if ( msg.isNull() )
			continue;

		if ( msg->has_systemmodeldata() )
			processMessage( msg->systemmodeldata() );
	}
}

void ModelProcessor::processMessage( MessageUnionPtr msg )
{
	if ( msg->has_systemmodeldata() )
	{
		QMutexLocker lock( &mMutex );

		mMessageQueue.push_back( msg );

		processMessage( msg->systemmodeldata() );
	}
}

void ModelProcessor::processMessage( const Analytics::SystemModelData& msg )
{
	try
	{
		modeldata::Scene scene;
		if ( scene.ParseFromString( msg.modelbytes() ) && scene.IsInitialized() )
		{
			for ( int m = 0; m < scene.meshes_size(); ++m )
			{
				const modeldata::Mesh& mesh = scene.meshes( m );
				Qt3D::QMeshDataPtr& cachedGeom = mGeomCache[ mesh.name() ];
				if ( cachedGeom == NULL )
					cachedGeom.reset( new Qt3D::QMeshData( Qt3D::QMeshData::Triangles ) );

				const QVector3D * vertices = reinterpret_cast<const QVector3D*>( &mesh.vertices()[ 0 ] );
				const size_t numVertices = mesh.vertices().size() / sizeof( QVector3D );

				// Vertex Buffer
				QByteArray posBytes;
				posBytes.resize( mesh.vertices().size() );
				memcpy( posBytes.data(), mesh.vertices().c_str(), mesh.vertices().size() );
				
				Qt3D::BufferPtr vertexBuffer( new Qt3D::Buffer( QOpenGLBuffer::VertexBuffer ) );
				vertexBuffer->setUsage( QOpenGLBuffer::StaticDraw );
				vertexBuffer->setData( posBytes );
				
				cachedGeom->addAttribute( Qt3D::QMeshData::defaultPositionAttributeName(), 
					Qt3D::AttributePtr( new Qt3D::Attribute( vertexBuffer, GL_FLOAT_VEC3, numVertices ) ) );
			}
			
			Qt3D::QEntity * entity = new Qt3D::QEntity();
			entity->setObjectName( scene.name().c_str() );
			processNode( scene, scene.rootnode(), entity );
			entity->moveToThread( QApplication::instance()->thread() );

			emit info( QString( "Created Group %1" ).arg( entity->objectName() ), QString() );
			emit allocModel( entity );
		}
	}
	catch ( const std::exception & ex )
	{
	}
}

void ModelProcessor::processNode( const modeldata::Scene& scene, const modeldata::Node& node, Qt3D::QEntity* entity )
{
	QMatrix4x4 xform;
	xform.setToIdentity();

	if ( node.has_eulerrotation() )
	{
		const modeldata::Vec3& euler = node.eulerrotation();

		xform.rotate( euler.x(), QVector3D( 1.0f, 0.0f, 0.0f ) );
		xform.rotate( euler.y(), QVector3D( 0.0f, 1.0f, 0.0f ) );
		xform.rotate( euler.z(), QVector3D( 0.0f, 0.0f, 1.0f ) );
	}

	if ( node.has_translation() )
	{
		const modeldata::Vec3& vec = node.translation();
		xform.translate( vec.x(), vec.y(), vec.z() );
	}

	Qt3D::QPhongMaterial * mtrl = new Qt3D::QPhongMaterial();
	mtrl->setDiffuse( QColor( "green" ) );

	Qt3D::QMatrixTransform * matrixXform = new Qt3D::QMatrixTransform();
	matrixXform->setMatrix( xform );

	Qt3D::QTransform * cmpXform = new Qt3D::QTransform();
	cmpXform->addTransform( matrixXform );

	entity->addComponent( mtrl );
	entity->addComponent( cmpXform );

	if ( node.has_meshname() )
	{
		GeomMap::iterator it = mGeomCache.find( node.meshname() );
		if ( it != mGeomCache.end() )
		{
			QAnalyticModel* cmp = new QAnalyticModel( NULL, it->second );
			cmp->setObjectName( node.meshname().c_str() );
			cmp->update();
			
			entity->addComponent( cmp );
		}
	}
	
	/*Qt3D::QCylinderMesh * cmpCylinder = new Qt3D::QCylinderMesh();
	cmpCylinder->setLength( 640.0f );
	cmpCylinder->setRadius( 320.0f );
	cmpCylinder->setRings( 3.0f );
	cmpCylinder->setSlices( 12.0f );
	entity->addComponent( cmpCylinder );*/

	/*for ( int c = 0; c < node.children_size(); ++c )
	{
	processNode( scene, node.children( c ), entity );
	}*/
}
