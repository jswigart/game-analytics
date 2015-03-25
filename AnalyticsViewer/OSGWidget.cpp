#include "OSGWidget.h"
#include "PickHandler.h"

#include <osg/Camera>

#include <osg/DisplaySettings>
#include <osg/Geode>
#include <osg/Material>
#include <osg/Shape>
#include <osg/ShapeDrawable>
#include <osg/StateSet>
#include <osg/Point>
#include <osg/PositionAttitudeTransform>
#include <osg/ProxyNode>

#include <osgDB/ReadFile> 
#include <osgDB/WriteFile>

#include <osgGA/EventQueue>
#include <osgGA/TrackballManipulator>
#include <osgGA/FirstPersonManipulator>

#include <osgManipulator/TabBoxDragger>
#include <osgManipulator/TabBoxTrackballDragger>
#include <osgManipulator/TabPlaneDragger>
#include <osgManipulator/TabPlaneTrackballDragger>
#include <osgManipulator/Scale1DDragger>
#include <osgManipulator/Scale2DDragger>
#include <osgManipulator/TrackballDragger>
#include <osgManipulator/Translate1DDragger>
#include <osgManipulator/Translate2DDragger>
#include <osgManipulator/TranslateAxisDragger>
#include <osgManipulator/TranslatePlaneDragger>
#include <osgManipulator/RotateCylinderDragger>

#include <osgUtil/IntersectionVisitor>
#include <osgUtil/PolytopeIntersector>

#include <osgViewer/View>
#include <osgViewer/ViewerEventHandlers>

#include <cassert>

#include <stdexcept>
#include <vector>

#include <QDebug>
#include <QKeyEvent>
#include <QWheelEvent>

namespace QtToOSG
{
	osgGA::GUIEventAdapter::KeySymbol convertSymbol( int k );
};

namespace
{
#if(WITH_SELECTION_PROCESSING)
	QRect makeRectangle( const QPoint& first, const QPoint& second )
	{
		// Relative to the first point, the second point may be in either one of the
		// four quadrants of an Euclidean coordinate system.
		//
		// We enumerate them in counter-clockwise order, starting from the lower-right
		// quadrant that corresponds to the default case:
		//
		//            |
		//       (3)  |  (4)
		//            |
		//     -------|-------
		//            |
		//       (2)  |  (1)
		//            |

		if( second.x() >= first.x() && second.y() >= first.y() )
			return QRect( first, second );
		else if( second.x() < first.x() && second.y() >= first.y() )
			return QRect( QPoint( second.x(), first.y() ), QPoint( first.x(), second.y() ) );
		else if( second.x() < first.x() && second.y() < first.y() )
			return QRect( second, first );
		else if( second.x() >= first.x() && second.y() < first.y() )
			return QRect( QPoint( first.x(), second.y() ), QPoint( second.x(), first.y() ) );

		// Should never reach that point...
		return QRect();
	}
#endif

}

void createSimpleMaterial( osg::StateSet * stateset, osg::Vec4 color )
{
	osg::Material *material = new osg::Material();
	material->setColorMode( osg::Material::AMBIENT_AND_DIFFUSE );
	material->setDiffuse( osg::Material::FRONT, color );
	//material->setEmission( osg::Material::FRONT, color );
	stateset->setAttributeAndModes( material, osg::StateAttribute::ON );
	stateset->setMode( GL_DEPTH_TEST, osg::StateAttribute::ON );
}

OSGWidget::OSGWidget( QWidget* parent, const QGLWidget* shareWidget, Qt::WindowFlags f )
	: QGLWidget( parent, shareWidget, f )
	, mGraphicsWindow( new osgViewer::GraphicsWindowEmbedded( x(), y(), width(), height() ) )
	, mViewer( new osgViewer::CompositeViewer )
#if(WITH_SELECTION_PROCESSING)
	, selectionActive_( false )
	, selectionFinished_( true )
#endif
{
	mRoot = new osg::Group();
	mRoot->setName( "SceneRoot" );

	mEntityGroup = new osg::Group();
	mEntityGroup->setName( "Entities" );
	mRoot->addChild( mEntityGroup );

	mMapGroup = new osg::Group();
	mMapGroup->setName( "Map" );
	mRoot->addChild( mMapGroup );

	osg::ref_ptr<osg::Material> material = new osg::Material;
	material->setColorMode( osg::Material::DIFFUSE );
	material->setAmbient( osg::Material::FRONT_AND_BACK, osg::Vec4( 0, 0, 0, 1 ) );
	material->setSpecular( osg::Material::FRONT_AND_BACK, osg::Vec4( 1, 1, 1, 1 ) );
	material->setShininess( osg::Material::FRONT_AND_BACK, 64.0f );
	mRoot->getOrCreateStateSet()->setMode( GL_DEPTH_TEST, osg::StateAttribute::ON );
	mRoot->getOrCreateStateSet()->setAttributeAndModes( material.get(), osg::StateAttribute::ON );
		
	{
		osg::MatrixTransform* transform = new osg::MatrixTransform;
		transform->setName( "LightTransform" );
		transform->setMatrix(
			osg::Matrix::rotate( osg::DegreesToRadians( 180.0 ), 1.0, 0.0, 0.0 ) * osg::Matrix::translate( osg::Vec3d( 0.0, 0.0, 500.0 ) ) );
		mRoot->addChild( transform );

		osg::Group * lightGroup = new osg::Group();
		lightGroup->setName( "LightGroup" );
		transform->addChild( lightGroup );

		osg::ShapeDrawable* lightmarker = new osg::ShapeDrawable( new osg::Sphere( osg::Vec3( 0.f, 0.f, 0.f ), 32.f ) );
		lightmarker->setName( "Sphere" );

		osg::Geode* geode = new osg::Geode;
		geode->setName( "LightMarker" );
		geode->addDrawable( lightmarker );
		createSimpleMaterial( geode->getOrCreateStateSet(), osg::Vec4( 0.0f, 1.0f, 0.0f, 1.0f ) );

		// create a spot light.
		osg::Light* dirLight = new osg::Light;
		dirLight->setName( "Light" );
		dirLight->setLightNum( 0 );
		dirLight->setPosition( osg::Vec4( 0.0f, 0.0f, 200.0f, 0.0f ) );
		dirLight->setAmbient( osg::Vec4( 0.2f, 0.2f, 0.2f, 1.0f ) );
		dirLight->setDiffuse( osg::Vec4( 0.6f, 0.6f, 0.6f, 1.0f ) );
		/*myLight1->setSpotCutoff( 2000.0f );
		myLight1->setSpotExponent( 5000.0f );*/
		dirLight->setDirection( osg::Vec3( 0.0f, 0.0f, -1.0f ) );

		osg::LightSource* lightSrc = new osg::LightSource;
		lightSrc->setName( "LightSource" );
		lightSrc->setLight( dirLight );
		lightSrc->setLocalStateSetModes( osg::StateAttribute::ON );

		lightGroup->addChild( geode );
		lightGroup->addChild( lightSrc );

		osgManipulator::TrackballDragger* dragger = new osgManipulator::TrackballDragger();
		dragger->setName( "Trackball Dragger" );
		dragger->setupDefaultGeometry();
		lightGroup->addChild( dragger );

		float scale = geode->getBound().radius() * 1.6;
		dragger->setMatrix( osg::Matrix::scale( scale, scale, scale ) *
			osg::Matrix::translate( geode->getBound().center() ) );
		dragger->addTransformUpdating( transform );
		dragger->setHandleEvents( true );
		//dragger->setActivationModKeyMask( osgGA::GUIEventAdapter::MODKEY_CTRL );
		//dragger->setActivationKeyEvent( 'a' );
	}

	const float aspectRatio = static_cast<float>( width() / 2 ) / static_cast<float>( height() );

	osg::Camera* camera = new osg::Camera;
	camera->setName( "Main Camera" );
	camera->setViewport( 0, 0, width() /*/ 2*/, height() );
	camera->setClearColor( osg::Vec4( 0.5f, 0.5f, 1.5f, 1.f ) );
	camera->setProjectionMatrixAsPerspective( 60.f, aspectRatio, 1.f, 1000.f );
	camera->setGraphicsContext( mGraphicsWindow.get() );

	osgViewer::View* view = new osgViewer::View;
	view->setCamera( camera );
	view->setSceneData( mRoot );
	view->addEventHandler( new osgViewer::StatsHandler );
#if(WITH_PICK_HANDLER)
	view->addEventHandler( new PickHandler );
#endif
	view->setCameraManipulator( new osgGA::TrackballManipulator );
	//view->setCameraManipulator( new osgGA::FirstPersonManipulator );
	
	/*osg::Camera* sideCamera = new osg::Camera;
	sideCamera->setViewport( width() / 2, 0,
	width() / 2, height() );

	sideCamera->setClearColor( osg::Vec4( 0.f, 0.f, 1.f, 1.f ) );
	sideCamera->setProjectionMatrixAsPerspective( 60.f, aspectRatio, 1.f, 1000.f );
	sideCamera->setGraphicsContext( graphicsWindow_ );

	osgViewer::View* sideView = new osgViewer::View;
	sideView->setCamera( sideCamera );
	sideView->setSceneData( geode );
	sideView->addEventHandler( new osgViewer::StatsHandler );
	sideView->setCameraManipulator( new osgGA::TrackballManipulator );*/

	mViewer->addView( view );
	//mViewer->addView( sideView );
	mViewer->setThreadingModel( osgViewer::CompositeViewer::SingleThreaded );

	// This ensures that the widget will receive keyboard events. This focus
	// policy is not set by default. The default, Qt::NoFocus, will result in
	// keyboard events that are ignored.
	setFocusPolicy( Qt::StrongFocus );
	setMinimumSize( 100, 100 );

	// Ensures that the widget receives mouse move events even though no
	// mouse button has been pressed. We require this in order to let the
	// graphics window switch viewports properly.
	setMouseTracking( true );
}

OSGWidget::~OSGWidget()
{
}

void OSGWidget::paintGL()
{
	osg::Vec3f eye, dir, up;
	mViewer->getView( 0 )->getCamera()->getViewMatrixAsLookAt( eye, dir, up );
	/*qDebug() << "Eye " << 
		QVector3D( eye.x(), eye.y(), eye.z() ) << "\nDir" << 
		QVector3D( dir.x(), dir.y(), dir.z() ) << "\nUp" << 
		QVector3D( up.x(), up.y(), up.z() ) << "\n";*/
		
	mViewer->frame();
	update();
}

void OSGWidget::resizeGL( int width, int height )
{
	getEventQueue()->windowResize( x(), y(), width, height );
	mGraphicsWindow->resized( x(), y(), width, height );

	onResize( width, height );
}

void OSGWidget::keyPressEvent( QKeyEvent* event )
{
	getEventQueue()->keyPress( QtToOSG::convertSymbol( event->key() ) );
}

void OSGWidget::keyReleaseEvent( QKeyEvent* event )
{
	if ( event->text() == "k" )
		onHome();

	getEventQueue()->keyRelease( QtToOSG::convertSymbol( event->key() ) );
}

void OSGWidget::mouseMoveEvent( QMouseEvent* event )
{
	getEventQueue()->mouseMotion( static_cast<float>( event->x() ), static_cast<float>( event->y() ) );
}

void OSGWidget::mousePressEvent( QMouseEvent* event )
{
	// Selection processing
#if(WITH_SELECTION_PROCESSING)
	if ( selectionActive_ && event->button() == Qt::LeftButton )
	{
		selectionStart_ = event->pos();
		selectionEnd_ = selectionStart_; // Deletes the old selection
		selectionFinished_ = false;           // As long as this is set, the rectangle will be drawn
	}
	
	else
#endif
	{
		// Normal processing

		// 1 = left mouse button
		// 2 = middle mouse button
		// 3 = right mouse button

		unsigned int button = 0;

		switch ( event->button() )
		{
			case Qt::LeftButton:
				button = 1;
				break;

			case Qt::MiddleButton:
				button = 2;
				break;

			case Qt::RightButton:
				button = 3;
				break;

			default:
				break;
		}

		getEventQueue()->mouseButtonPress( static_cast<float>( event->x() ),
			static_cast<float>( event->y() ),
			button );
	}
}

void OSGWidget::mouseReleaseEvent( QMouseEvent* event )
{
	// Selection processing: Store end position and obtain selected objects
	// through polytope intersection.
#if(WITH_SELECTION_PROCESSING)
	if ( selectionActive_ && event->button() == Qt::LeftButton )
	{
		selectionEnd_ = event->pos();
		selectionFinished_ = true; // Will force the painter to stop drawing the
		// selection rectangle

		processSelection();
	}
	else
#endif
	{
		// Normal processing
		// 1 = left mouse button
		// 2 = middle mouse button
		// 3 = right mouse button

		unsigned int button = 0;

		switch ( event->button() )
		{
			case Qt::LeftButton:
				button = 1;
				break;

			case Qt::MiddleButton:
				button = 2;
				break;

			case Qt::RightButton:
				button = 3;
				break;

			default:
				break;
		}

		getEventQueue()->mouseButtonRelease( static_cast<float>( event->x() ),
			static_cast<float>( event->y() ),
			button );
	}
}

void OSGWidget::wheelEvent( QWheelEvent* event )
{
	// Ignore wheel events as long as the selection is active.
#if(WITH_SELECTION_PROCESSING)
	if ( selectionActive_ )
		return;
#endif

	event->accept();
	int delta = event->delta();

	osgGA::GUIEventAdapter::ScrollingMotion motion = delta > 0 ? osgGA::GUIEventAdapter::SCROLL_UP
		: osgGA::GUIEventAdapter::SCROLL_DOWN;

	getEventQueue()->mouseScroll( motion );
}

bool OSGWidget::event( QEvent* event )
{
	bool handled = QGLWidget::event( event );

	// This ensures that the OSG widget is always going to be repainted after the
	// user performed some interaction. Doing this in the event handler ensures
	// that we don't forget about some event and prevents duplicate code.
	switch ( event->type() )
	{
		case QEvent::KeyPress:
		case QEvent::KeyRelease:
		case QEvent::MouseButtonDblClick:
		case QEvent::MouseButtonPress:
		case QEvent::MouseButtonRelease:
		case QEvent::MouseMove:
		case QEvent::Wheel:
			update();
			break;

		default:
			break;
	}

	return( handled );
}

void OSGWidget::onHome()
{
	osgViewer::ViewerBase::Views views;
	mViewer->getViews( views );

	for ( std::size_t i = 0; i < views.size(); i++ )
	{
		osgViewer::View* view = views.at( i );
		view->home();
	}
}

void OSGWidget::onResize( int width, int height )
{
	std::vector<osg::Camera*> cameras;
	mViewer->getCameras( cameras );

	const float camPortWidth = width / cameras.size();
	for ( size_t i = 0; i < cameras.size(); ++i )
	{
		const float aspect = camPortWidth / height;
		cameras[ i ]->setViewport( i * camPortWidth, 0, camPortWidth, height );
		cameras[ i ]->setProjectionMatrixAsPerspective( 60.f, aspect, 1.f, 1000.f );
	}
}

osgGA::EventQueue* OSGWidget::getEventQueue() const
{
	osgGA::EventQueue* eventQueue = mGraphicsWindow->getEventQueue();
	if ( eventQueue )
		return( eventQueue );
	else
		throw( std::runtime_error( "Unable to obtain valid event queue" ) );
}

void OSGWidget::processSelection()
{
#if(WITH_SELECTION_PROCESSING)
	QRect selectionRectangle = makeRectangle( selectionStart_, selectionEnd_ );
	int widgetHeight         = height();

	double xMin = selectionRectangle.left();
	double xMax = selectionRectangle.right();
	double yMin = widgetHeight - selectionRectangle.bottom();
	double yMax = widgetHeight - selectionRectangle.top();

	osgUtil::PolytopeIntersector* polytopeIntersector
		= new osgUtil::PolytopeIntersector( osgUtil::PolytopeIntersector::WINDOW,
		xMin, yMin,
		xMax, yMax );

	// This limits the amount of intersections that are reported by the
	// polytope intersector. Using this setting, a single drawable will
	// appear at most once while calculating intersections. This is the
	// preferred and expected behaviour.
	polytopeIntersector->setIntersectionLimit( osgUtil::Intersector::LIMIT_ONE_PER_DRAWABLE );

	osgUtil::IntersectionVisitor iv( polytopeIntersector );

	for( unsigned int viewIndex = 0; viewIndex < mViewer->getNumViews(); viewIndex++ )
	{
		osgViewer::View* view = mViewer->getView( viewIndex );

		if( !view )
			throw std::runtime_error( "Unable to obtain valid view for selection processing" );

		osg::Camera* camera = view->getCamera();

		if( !camera )
			throw std::runtime_error( "Unable to obtain valid camera for selection processing" );

		camera->accept( iv );

		if( !polytopeIntersector->containsIntersections() )
			continue;

		auto intersections = polytopeIntersector->getIntersections();

		for( auto&& intersection : intersections )
			qDebug() << "Selected a drawable:" << QString::fromStdString( intersection.drawable->getName() );
	}
#endif
}

bool OSGWidget::importModelToScene( const QString & filePath, bool clearExisting )
{
	/*if ( clearExisting )
		mViewer->getView(0)->getSceneData()->*/

	// load the scene async
	osg::ref_ptr<osgDB::Options> options = new osgDB::Options( "generateFacetNormals noTriStripPolygons mergeMeshes noRotation" );
	osg::ProxyNode * pn = new osg::ProxyNode();
	pn->setName( filePath.toStdString().c_str() );
	pn->setFileName( 0, filePath.toStdString() );
	pn->setDatabaseOptions( options );
	createSimpleMaterial( pn->getOrCreateStateSet(), osg::Vec4( 0.9f, 0.9f, 0.9f, 1.0f ) );
	mRoot->addChild( pn );

	// todo: report errors
	return true;
}

bool OSGWidget::exportSceneToFile( const QString & filePath )
{
	return osgDB::writeNodeFile( *mRoot, filePath.toStdString() );	
}

void OSGWidget::visitSceneNodes( osg::NodeVisitor & visitor )
{
	mViewer->getView( 0 )->getSceneData()->accept( visitor );
}

osg::MatrixTransform * OSGWidget::findOrCreateEntityNode( int entityId )
{
	EntityMap::iterator it = mEntityMap.find( entityId );
	if ( it != mEntityMap.end() )
		return it.value();

	// create it
	osg::MatrixTransform * xform = new osg::MatrixTransform;
	xform->setName( tr( "Entity %1" ).arg( entityId ).toStdString() );
	
	mEntityGroup->addChild( xform );
	mEntityMap.insert( entityId, xform );
	
	createSimpleMaterial( xform->getOrCreateStateSet(), osg::Vec4( 0.0f, 1.0f, 0.0f, 1.0f ) );

	return xform;
}

void OSGWidget::allocProxyNode( const std::string& filename )
{
	osg::ref_ptr<osgDB::Options> options = new osgDB::Options( "generateFacetNormals noTriStripPolygons mergeMeshes noRotation" );
	osg::ProxyNode * proxy = new osg::ProxyNode();
	proxy->setFileName( 0, filename );
	proxy->setDatabaseOptions( options );
	createSimpleMaterial( proxy->getOrCreateStateSet(), osg::Vec4( 0.9f, 0.9f, 0.9f, 1.0f ) );
	mMapGroup->addChild( proxy );
}
