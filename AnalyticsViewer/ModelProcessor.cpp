#include "ModelProcessor.h"
#include "GameAnalytics.h"

#include "analytics.pb.h"

ModelProcessor::ModelProcessor( QObject *parent )
	: QThread( parent )
{
	// connect message handling to the parent
}

void ModelProcessor::run()
{
	while ( true )
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
			osg::Group* grp = new osg::Group();

			GeomList meshGeoms( scene.meshes_size() );

			size_t totalVertices = 0;
			size_t totalFaces = 0;

			for ( int m = 0; m < scene.meshes_size(); ++m )
			{
				const modeldata::Mesh& mesh = scene.meshes( m );
				const modeldata::Material& mtrl = scene.materials( mesh.materialindex() );

				const osg::Vec3 * vertices = reinterpret_cast<const osg::Vec3*>( &mesh.vertices()[ 0 ] );
				const size_t numVertices = mesh.vertices().size() / sizeof( osg::Vec3 );

				osg::Vec3Array* meshVertices = new osg::Vec3Array;
				meshVertices->resizeArray( numVertices );

				osg::Vec3Array* meshNormals = new osg::Vec3Array;
				meshNormals->resizeArray( numVertices );

				const unsigned int * indices = reinterpret_cast<const unsigned int*>( &mesh.faces()[ 0 ] );
				const size_t numFaces = mesh.faces().size() / sizeof( unsigned int );
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

				for ( size_t f = 0; f < numFaces; ++f )
				{
					( *drawElements )[ f ] = indices[ f ];
				}

				meshGeoms[ m ] = new osg::Geometry();
				meshGeoms[ m ]->setName( mesh.name() );
				meshGeoms[ m ]->setVertexArray( meshVertices );
				meshGeoms[ m ]->setNormalArray( meshNormals, osg::Array::BIND_PER_VERTEX );
				meshGeoms[ m ]->addPrimitiveSet( drawElements );
			}

			processNode( scene, meshGeoms, scene.rootnode(), grp );

			std::string cacheFileName = msg.modelname() + ".osgb";
			std::replace( cacheFileName.begin(), cacheFileName.end(), '/', '_' );
			std::replace( cacheFileName.begin(), cacheFileName.end(), '\\', '_' );
			cacheFileName.insert( 0, "cache/" );
			//osgDB::writeNodeFile( *grp, cacheFileName );

			osgDB::Registry::instance()->getObjectCache()->addEntryToObjectCache( cacheFileName, grp );;
			
			emit info( tr( "Cached model %1" ).arg( cacheFileName.c_str() ), tr( "Vertices %1, Faces %2" ).arg( totalVertices ).arg( totalFaces ) );

			emit allocModel( cacheFileName.c_str() );
		}
	}
	catch ( const std::exception & ex )
	{
	}
}

void ModelProcessor::processNode( const modeldata::Scene& scene, const GeomList& geoms, const modeldata::Node& node, osg::Group* grp )
{
	osg::Group* addGrp = grp;

	if ( node.has_transformation() )
	{
		const modeldata::Matrix4& trans = node.transformation();

		osg::Matrix mat;
		mat.makeIdentity();
		/*mat.set(
		trans.row0().x(), trans.row0().y(), trans.row0().z(), trans.row0().w(),
		trans.row1().x(), trans.row1().y(), trans.row1().z(), trans.row1().w(),
		trans.row2().x(), trans.row2().y(), trans.row2().z(), trans.row2().w(),
		trans.row3().x(), trans.row3().y(), trans.row3().z(), trans.row3().w() );*/

		osg::MatrixTransform* xform = new osg::MatrixTransform;
		xform->setMatrix( mat );
		addGrp = xform;

		addGrp->setName( node.name() );
	}

	if ( node.meshindex_size() > 0 )
	{
		for ( int m = 0; m < node.meshindex_size(); ++m )
		{
			addGrp->addChild( geoms[ node.meshindex( m ) ] );
		}
	}

	for ( int c = 0; c < node.children_size(); ++c )
	{
		processNode( scene, geoms, node.children( c ), addGrp );
	}
}
