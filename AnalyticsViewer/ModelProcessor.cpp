#include "ModelProcessor.h"
#include "GameAnalytics.h"

#include "analytics.pb.h"

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

		mMutex.lock();
		if ( !mMessageQueue.isEmpty() )
		{
			msg = mMessageQueue.front();
			mMessageQueue.pop_front();
		}
		mMutex.unlock();

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
		mMutex.lock();
		mMessageQueue.push_back( msg );
		mMutex.unlock();

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
			osg::MatrixTransform* grp = new osg::MatrixTransform();
			
			size_t totalVertices = 0;
			size_t totalFaces = 0;

			for ( int m = 0; m < scene.meshes_size(); ++m )
			{
				const modeldata::Mesh& mesh = scene.meshes( m );
				GeometryPtr & cachedGeom = mGeomCache[ mesh.name() ];
				
				//const modeldata::Material& mtrl = scene.materials( mesh.materialindex() );

				const osg::Vec3 * vertices = reinterpret_cast<const osg::Vec3*>( &mesh.vertices()[ 0 ] );
				const size_t numVertices = mesh.vertices().size() / sizeof( osg::Vec3 );

				osg::Vec3Array* meshVertices = new osg::Vec3Array;
				meshVertices->resizeArray( numVertices );

				osg::Vec3Array* meshNormals = new osg::Vec3Array;
				meshNormals->resizeArray( numVertices );

				const unsigned int * indices = reinterpret_cast<const unsigned int*>( &mesh.faces()[ 0 ] );
				const size_t numFaces = mesh.faces().size() / ( sizeof( unsigned int ) * 3 );
				osg::DrawElementsUInt* drawElements = new osg::DrawElementsUInt( osg::PrimitiveSet::TRIANGLES, 0 );
				drawElements->resizeElements( numVertices );

				totalVertices += numVertices;
				totalFaces += numFaces;

				// copy the vertices and create normals
				for ( size_t v = 0; v < numVertices; v += 3 )
				{
					( *meshVertices )[ v + 0 ] = vertices[ v + 0 ];
					( *meshVertices )[ v + 1 ] = vertices[ v + 1 ];
					( *meshVertices )[ v + 2 ] = vertices[ v + 2 ];

					const osg::Vec3 ab = ( *meshVertices )[ v + 1 ] - ( *meshVertices )[ v + 0 ];
					const osg::Vec3 ac = ( *meshVertices )[ v + 2 ] - ( *meshVertices )[ v + 0 ];
					osg::Vec3 normal( ab ^ ac );
					normal.normalize();

					( *meshNormals )[ v + 0 ] = normal;
					( *meshNormals )[ v + 1 ] = normal;
					( *meshNormals )[ v + 2 ] = normal;
				}

				for ( size_t f = 0; f < numVertices; f += 3 )
				{
					( *drawElements )[ f + 0 ] = indices[ f + 0 ];
					( *drawElements )[ f + 1 ] = indices[ f + 1 ];
					( *drawElements )[ f + 2 ] = indices[ f + 2 ];
				}

				if ( cachedGeom == NULL )
				{
					cachedGeom = new osg::Geometry();
					cachedGeom->setName( mesh.name() );
				}

				cachedGeom->setVertexArray( meshVertices );
				cachedGeom->setNormalArray( meshNormals, osg::Array::BIND_PER_VERTEX );
				cachedGeom->addPrimitiveSet( drawElements );
			}

			processNode( scene, scene.rootnode(), grp );

			std::string cacheFileName = msg.modelname();
			std::replace( cacheFileName.begin(), cacheFileName.end(), '/', '_' );
			std::replace( cacheFileName.begin(), cacheFileName.end(), '\\', '_' );
			cacheFileName.insert( 0, "cache/" );
			cacheFileName += ".osgb";
			//osgDB::writeNodeFile( *grp, cacheFileName );
			
			//osgDB::Registry::instance()->getFileCache()->writeNode( *grp, cacheFileName, NULL );
			osgDB::Registry::instance()->getObjectCache()->addEntryToObjectCache( cacheFileName, grp );

			emit info( tr( "Cached model %1" ).arg( cacheFileName.c_str() ), tr( "Vertices %1, Faces %2" ).arg( totalVertices ).arg( totalFaces ) );

			emit allocModel( cacheFileName.c_str() );
		}
	}
	catch ( const std::exception & ex )
	{
	}
}

void ModelProcessor::processNode( const modeldata::Scene& scene, const modeldata::Node& node, osg::MatrixTransform* grp )
{
	osg::Matrix mat;
	mat.makeIdentity();
	
	if ( node.has_eulerrotation() )
	{
		const modeldata::Vec3& euler = node.eulerrotation();
		mat.rotate( euler.x(), osg::Vec3( 1.0f, 0.0f, 0.0f ) );
		mat.rotate( euler.y(), osg::Vec3( 0.0f, 1.0f, 0.0f ) );
		mat.rotate( euler.z(), osg::Vec3( 0.0f, 0.0f, 1.0f ) );
	}

	if ( node.has_translation() )
	{
		const modeldata::Vec3& vec = node.translation();
		mat.setTrans( vec.x(), vec.y(), vec.z() );
	}

	grp->setMatrix( mat );

	if ( node.has_meshname() )
	{
		GeomMap::iterator it = mGeomCache.find( node.meshname() );
		if ( it != mGeomCache.end() )
			grp->addChild( it->second );
	}

	for ( int c = 0; c < node.children_size(); ++c )
	{
		processNode( scene, node.children( c ), grp );
	}
}
