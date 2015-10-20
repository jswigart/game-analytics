#include <QtWidgets/QApplication>
#include <Qt3DRenderer/QMesh>
#include <Qt3DRenderer/QMeshData>
#include <Qt3DRenderer/BufferPtr>
#include <Qt3DRenderer/AttributePtr>
#include <Qt3DCore/QTransform>
#include <Qt3DCore/QMatrixTransform>
#include <Qt3DRenderer/QCylinderMesh>
#include <Qt3DRenderer/QPhongMaterial>
#include <Qt3DRenderer/QPerVertexColorMaterial>
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

QAnalyticModel::QAnalyticModel( QNode *parent )
	: Qt3D::QAbstractMesh( parent )
{
}

void QAnalyticModel::copy( const Qt3D::QNode *ref )
{
	Qt3D::QAbstractMesh::copy( ref );

	const QAnalyticModel * other = qobject_cast<const QAnalyticModel*>( ref );
	m_data = other->m_data;
}

void QAnalyticModel::SetMesh( Qt3D::QMeshDataPtr ptr )
{
	m_data = ptr;
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

		//processMessage( msg->systemmodeldata() );
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

				/*if ( !mesh.has_vertexcolors() )
					continue;*/

				Qt3D::QMeshDataPtr& cachedGeom = mGeomCache[ mesh.name() ];
				if ( cachedGeom == NULL )
					cachedGeom.reset( new Qt3D::QMeshData( Qt3D::QMeshData::Triangles ) );

				const QVector3D * vertices = reinterpret_cast<const QVector3D*>( &mesh.vertices()[ 0 ] );
				const size_t numVertices = mesh.vertices().size() / sizeof( QVector3D );

				// Vertex Buffer
				QByteArray posBytes;
				posBytes.resize( numVertices * sizeof( QVector3D ) );
				memcpy( posBytes.data(), vertices, mesh.vertices().size() );

				Qt3D::BufferPtr vertexBuffer( new Qt3D::Buffer( QOpenGLBuffer::VertexBuffer ) );
				vertexBuffer->setUsage( QOpenGLBuffer::StaticDraw );
				vertexBuffer->setData( posBytes );
				
				cachedGeom->addAttribute( Qt3D::QMeshData::defaultPositionAttributeName(), Qt3D::AttributePtr( new Qt3D::Attribute( vertexBuffer, GL_FLOAT_VEC3, numVertices ) ) );

				// Normal Buffer
				QByteArray normalBytes;
				normalBytes.resize( numVertices * sizeof( QVector3D ) );
				QVector3D * normalPtr = (QVector3D *)normalBytes.data();
				for ( size_t v = 0; v < numVertices; ++v )
				{
					const QVector3D * baseVert = &vertices[ v / 3 * 3 ];
					normalPtr[ v ] = QVector3D::normal( baseVert[ 0 ], baseVert[ 1 ], baseVert[ 2 ] );
				}

				Qt3D::BufferPtr normalBuffer( new Qt3D::Buffer( QOpenGLBuffer::VertexBuffer ) );
				normalBuffer->setUsage( QOpenGLBuffer::StaticDraw );
				normalBuffer->setData( normalBytes );
				cachedGeom->addAttribute( Qt3D::QMeshData::defaultNormalAttributeName(), Qt3D::AttributePtr( new Qt3D::Attribute( normalBuffer, GL_FLOAT_VEC3, numVertices ) ) );

				// Index List
				/*QByteArray indexBytes;
				indexBytes.resize( numVertices * sizeof( unsigned int ) * 3 );
				unsigned int* indexPtr = (unsigned int*)indexBytes.data();
				for ( size_t v = 0; v < numVertices; ++v )
				indexPtr[v] = (unsigned int)v;

				Qt3D::BufferPtr indexBuffer( new Qt3D::Buffer( QOpenGLBuffer::IndexBuffer ) );
				indexBuffer->setUsage( QOpenGLBuffer::StaticDraw );
				indexBuffer->setData( indexBytes );

				cachedGeom->setIndexAttribute( Qt3D::AttributePtr( new Qt3D::Attribute( indexBuffer, GL_UNSIGNED_INT_VEC3, numVertices ) ) );*/
								
				// Color Buffer
				
				const QColor dColor( "#606060" );

				QByteArray clrBytes;
				clrBytes.resize( numVertices * sizeof( QVector4D ) );
				//memcpy( clrBytes.data(), mesh.vertices().c_str(), mesh.vertices().size() );
				QVector4D * clrPtr = (QVector4D *)clrBytes.data();

				// initialize to default color
				for ( int i = 0; i < numVertices; ++i )
					clrPtr[ i ] = QVector4D( dColor.redF(), dColor.greenF(), dColor.blueF(), dColor.alphaF() );

				// optionally use the input color data
				if ( mesh.has_vertexcolors() )
				{
					// incoming colors are packed smaller than floats, so we need to expand them
					const QRgb * vertexColors = reinterpret_cast<const QRgb*>( &mesh.vertexcolors()[ 0 ] );
					const size_t numColors = mesh.vertexcolors().size() / sizeof( QRgb );
					if ( numColors == numVertices )
					{
						for ( int i = 0; i < numColors; ++i )
						{
							QColor color = QColor::fromRgba( vertexColors[ i ] );
							clrPtr[ i ] = QVector4D( color.redF(), color.greenF(), color.blueF(), color.alphaF() );
						}
					}
				}

				Qt3D::BufferPtr colorBuffer( new Qt3D::Buffer( QOpenGLBuffer::VertexBuffer ) );
				colorBuffer->setUsage( QOpenGLBuffer::StaticDraw );
				colorBuffer->setData( clrBytes );

				cachedGeom->addAttribute( Qt3D::QMeshData::defaultColorAttributeName(), Qt3D::AttributePtr( new Qt3D::Attribute( colorBuffer, GL_FLOAT_VEC4, numVertices ) ) );
			}

			Qt3D::QEntity * entity = new Qt3D::QEntity();
			entity->setObjectName( scene.name().c_str() );
			processNode( scene, scene.rootnode(), entity );
			entity->moveToThread( QApplication::instance()->thread() );

			emit info( QString( "Created Group %1 from data size %2" ).arg( entity->objectName() ).arg( msg.modelbytes().size() ), QString() );
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

	/*Qt3D::QPhongMaterial * mtrl = new Qt3D::QPhongMaterial();
	mtrl->setDiffuse( QColor( "grey" ) );*/

	Qt3D::QPerVertexColorMaterial * mtrl = new Qt3D::QPerVertexColorMaterial();

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
			QAnalyticModel* cmp = new QAnalyticModel( NULL );
			cmp->SetMesh( it->second );
			cmp->setObjectName( node.meshname().c_str() );
			cmp->update();

			entity->addComponent( cmp );
		}
	}

	for ( int c = 0; c < node.children_size(); ++c )
	{
		const modeldata::Node& child = node.children( c );

		Qt3D::QEntity * childEntity = new Qt3D::QEntity();
		childEntity->setObjectName( child.meshname().c_str() );
		processNode( scene, child, childEntity );
		childEntity->setParent( entity );
	}
}
