#include <QtWidgets/QApplication>
#include <Qt3DRenderer/QMesh>
#include <Qt3DCore/QTransform>
#include <Qt3DCore/QMatrixTransform>
#include <Qt3DRenderer/QAttribute>
#include <Qt3DRenderer/QBuffer>
#include <Qt3DRenderer/QCylinderMesh>
#include <Qt3DRenderer/QPhongMaterial>
#include <Qt3DRenderer/QPerVertexColorMaterial>
#include <QDebug>

#include "ModelProcessor.h"
#include "GameAnalytics.h"

#include "analytics.pb.h"

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

				Qt3D::QGeometry*& cachedGeom = mGeomCache[ mesh.name() ];
				if ( cachedGeom == NULL )
					cachedGeom = new Qt3D::QGeometry();

				const QVector3D * vertices = reinterpret_cast<const QVector3D*>( &mesh.vertices()[ 0 ] );
				const size_t numVertices = mesh.vertices().size() / sizeof( QVector3D );

				QByteArray posBytes;
				posBytes.resize( numVertices * sizeof( QVector3D ) );
				memcpy( posBytes.data(), vertices, mesh.vertices().size() );
				
				// Normal Buffer
				QByteArray normalBytes;
				normalBytes.resize( numVertices * sizeof( QVector3D ) );
				QVector3D * normalPtr = (QVector3D *)normalBytes.data();
				for ( size_t v = 0; v < numVertices; ++v )
				{
					const QVector3D * baseVert = &vertices[ v / 3 * 3 ];
					normalPtr[ v ] = QVector3D::normal( baseVert[ 0 ], baseVert[ 1 ], baseVert[ 2 ] );
				}

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
				
				// Qt 5.6
				Qt3D::QBuffer *vertexDataBuffer = new Qt3D::QBuffer( Qt3D::QBuffer::VertexBuffer, cachedGeom );
				Qt3D::QBuffer *normalDataBuffer = new Qt3D::QBuffer( Qt3D::QBuffer::VertexBuffer, cachedGeom );
				Qt3D::QBuffer *colorDataBuffer = new Qt3D::QBuffer( Qt3D::QBuffer::VertexBuffer, cachedGeom );
				//Qt3D::QBuffer *indexDataBuffer = new Qt3D::QBuffer( Qt3D::QBuffer::IndexBuffer, cachedGeom );

				vertexDataBuffer->setData( posBytes );
				normalDataBuffer->setData( normalBytes );
				colorDataBuffer->setData( clrBytes );

				// Attributes
				Qt3D::QAttribute *positionAttribute = new Qt3D::QAttribute();
				positionAttribute->setAttributeType( Qt3D::QAttribute::VertexAttribute );
				positionAttribute->setBuffer( vertexDataBuffer );
				positionAttribute->setDataType( Qt3D::QAttribute::Float );
				positionAttribute->setDataSize( 3 );
				positionAttribute->setByteOffset( 0 );
				positionAttribute->setByteStride( 3 * sizeof( float ) );
				positionAttribute->setCount( numVertices );
				positionAttribute->setName( Qt3D::QAttribute::defaultPositionAttributeName() );

				Qt3D::QAttribute *normalAttribute = new Qt3D::QAttribute();
				normalAttribute->setAttributeType( Qt3D::QAttribute::VertexAttribute );
				normalAttribute->setBuffer( normalDataBuffer );
				normalAttribute->setDataType( Qt3D::QAttribute::Float );
				normalAttribute->setDataSize( 3 );
				normalAttribute->setByteOffset( 0 );
				normalAttribute->setByteStride( 3 * sizeof( float ) );
				normalAttribute->setCount( numVertices );
				normalAttribute->setName( Qt3D::QAttribute::defaultNormalAttributeName() );

				Qt3D::QAttribute *colorAttribute = new Qt3D::QAttribute();
				colorAttribute->setAttributeType( Qt3D::QAttribute::VertexAttribute );
				colorAttribute->setBuffer( colorDataBuffer );
				colorAttribute->setDataType( Qt3D::QAttribute::Float );
				colorAttribute->setDataSize( 4 );
				colorAttribute->setByteOffset( 0 );
				colorAttribute->setByteStride( 4 * sizeof( float ) );
				colorAttribute->setCount( numVertices );
				colorAttribute->setName( Qt3D::QAttribute::defaultColorAttributeName() );

				/*Qt3D::QAttribute *indexAttribute = new Qt3D::QAttribute();
				indexAttribute->setAttributeType( Qt3D::QAttribute::IndexAttribute );
				indexAttribute->setBuffer( indexDataBuffer );
				indexAttribute->setDataType( Qt3D::QAttribute::UnsignedShort );
				indexAttribute->setDataSize( 1 );
				indexAttribute->setByteOffset( 0 );
				indexAttribute->setByteStride( 0 );
				indexAttribute->setCount( 12 );*/

				cachedGeom->addAttribute( positionAttribute );
				cachedGeom->addAttribute( normalAttribute );
				cachedGeom->addAttribute( colorAttribute );
				//cachedGeom->addAttribute( indexAttribute );
				cachedGeom->setVerticesPerPatch( numVertices );
				//cachedGeom->setProperty( "numPrimitives", numVertices );
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
			Qt3D::QGeometryRenderer *cmp = new Qt3D::QGeometryRenderer();
			cmp->setInstanceCount( 1 );
			cmp->setBaseVertex( 0 );
			cmp->setBaseInstance( 0 );
			cmp->setPrimitiveType( Qt3D::QGeometryRenderer::Triangles );
			cmp->setGeometry( it->second );
			cmp->setPrimitiveCount( it->second->verticesPerPatch() );
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
