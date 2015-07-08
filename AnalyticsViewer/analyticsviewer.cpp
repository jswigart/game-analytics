#include <osgDB/OutputStream>
#include <osg/MatrixTransform>
#include <osg/ShapeDrawable>
#include <osg/ProxyNode>
#include <osgDB/Registry>
#include <osgDB/WriteFile>
#include <osgDB/ClassInterface>

#include <QCheckBox>
#include <QTextEdit>
#include <QFileDialog>
#include <strstream>

#include "analyticsviewer.h"
#include "Messaging.h"
#include "ModelProcessor.h"

#include "analytics.pb.h"

AnalyticsViewer::AnalyticsViewer( QWidget *parent )
	: QMainWindow( parent )
{
	ui.setupUi( this );
	
	// Hook up signals
	connect( ui.actionOpen, SIGNAL( triggered( bool ) ), this, SLOT( FileOpen() ) );
	connect( ui.actionSaveScene, SIGNAL( triggered( bool ) ), this, SLOT( FileSave() ) );
	connect( ui.toggleMessages, SIGNAL( toggled( bool ) ), this, SLOT( RebuildLogTable() ) );
	connect( ui.toggleWarnings, SIGNAL( toggled( bool ) ), this, SLOT( RebuildLogTable() ) );
	connect( ui.toggleErrors, SIGNAL( toggled( bool ) ), this, SLOT( RebuildLogTable() ) );
	connect( ui.toggleDebug, SIGNAL( toggled( bool ) ), this, SLOT( RebuildLogTable() ) );
	connect( ui.editOutputFilter, SIGNAL( textChanged( const QString & ) ), this, SLOT( RebuildLogTable() ) );
	connect( ui.treeHierarchy, SIGNAL( itemSelectionChanged() ), this, SLOT( SelectionChanged() ) );
	
	// schedule some processor time intervals
	{
		QTimer *timer = new QTimer( this );
		connect( timer, SIGNAL( timeout() ), this, SLOT( UpdateHierarchy() ) );
		timer->start( 1000 );
	}

	{
		// Configure the scene hierarchy tree
		QStringList labels;
		labels.push_back( "Name" );
		labels.push_back( "Type" );
		labels.push_back( "Description" );

		ui.treeHierarchy->setColumnCount( 3 );
		ui.treeHierarchy->setHeaderLabels( labels );
		ui.treeHierarchy->setColumnWidth( 0, 300 );
		ui.treeHierarchy->setColumnWidth( 1, 100 );
		ui.treeHierarchy->setColumnWidth( 2, 300 );
	}

	{
		QStringList labels;
		labels.push_back( "Property" );
		labels.push_back( "Value" );

		// Configure the selection tree
		ui.treeSelection->setColumnCount( 2 );
		ui.treeSelection->setHeaderLabels( labels );
		ui.treeSelection->setColumnWidth( 0, 300 );
		ui.treeSelection->setColumnWidth( 1, 100 );
	}

	{
		QTableWidgetItem * typeItem = new QTableWidgetItem( "Type" );
		typeItem->setData( Qt::TextAlignmentRole, Qt::AlignLeft );
		QTableWidgetItem * descItem = new QTableWidgetItem( "Description" );
		descItem->setData( Qt::TextAlignmentRole, Qt::AlignLeft );
		QTableWidgetItem * detailsItem = new QTableWidgetItem();
		detailsItem->setIcon( QIcon( QStringLiteral( "Resources/svg/document98.svg" ) ) );

		// Configure the output table
		ui.tableOutput->setColumnCount( 3 );
		ui.tableOutput->setHorizontalHeaderItem( 0, typeItem );
		ui.tableOutput->setHorizontalHeaderItem( 1, descItem );
		ui.tableOutput->setHorizontalHeaderItem( 2, detailsItem );
		ui.tableOutput->horizontalHeader()->setSectionResizeMode( 0, QHeaderView::Fixed );
		ui.tableOutput->horizontalHeader()->setSectionResizeMode( 1, QHeaderView::Stretch );
		ui.tableOutput->horizontalHeader()->resizeSection( 1, 400 );
		ui.tableOutput->horizontalHeader()->setSectionResizeMode( 2, QHeaderView::Interactive );
		ui.tableOutput->horizontalHeader()->resizeSection( 2, 30 );
	}

	{
		QLineEdit * networkIp = new QLineEdit( this );
		networkIp->setFixedWidth( 100 );
		networkIp->setText( "127.0.0.1" );
		
		QLineEdit * networkPort = new QLineEdit( this );
		networkPort->setFixedWidth( 50 );
		networkPort->setText( "5050" );
		
		mNetworkLabel = new QLabel( this );

		statusBar()->addWidget( networkIp );
		statusBar()->addWidget( networkPort );
		statusBar()->addWidget( mNetworkLabel );
	}
	
	qRegisterMetaType<MessageUnionPtr>( "MessageUnionPtr" );
	
	{
		mMessageThread = new HostThread0MQ( this );

		connect( mMessageThread, SIGNAL( info( QString, QString ) ), this, SLOT( LogInfo( QString, QString ) ) );
		connect( mMessageThread, SIGNAL( warn( QString, QString ) ), this, SLOT( LogWarn( QString, QString ) ) );
		connect( mMessageThread, SIGNAL( error( QString, QString ) ), this, SLOT( LogError( QString, QString ) ) );
		connect( mMessageThread, SIGNAL( debug( QString, QString ) ), this, SLOT( LogDebug( QString, QString ) ) );
		connect( mMessageThread, SIGNAL( status( QString ) ), this, SLOT( StatusInfo( QString ) ) );
		
		// message processing functions
		connect( mMessageThread, SIGNAL( onmsg( MessageUnionPtr ) ), this, SLOT( processMessage( MessageUnionPtr ) ) );
		
		mModelProcessorThread = new ModelProcessor( this );
		connect( mModelProcessorThread, SIGNAL( info( QString, QString ) ), this, SLOT( LogInfo( QString, QString ) ) );
		connect( mModelProcessorThread, SIGNAL( warn( QString, QString ) ), this, SLOT( LogWarn( QString, QString ) ) );
		connect( mModelProcessorThread, SIGNAL( error( QString, QString ) ), this, SLOT( LogError( QString, QString ) ) );
		connect( mModelProcessorThread, SIGNAL( debug( QString, QString ) ), this, SLOT( LogDebug( QString, QString ) ) );
		connect( mModelProcessorThread, SIGNAL( allocModel( QString ) ), ui.renderBg, SLOT( allocProxyNode( QString ) ) );

		// message processing functions
		connect( mMessageThread, SIGNAL( onmsg( MessageUnionPtr ) ), mModelProcessorThread, SLOT( processMessage( MessageUnionPtr ) ) );

		mMessageThread->start();
		mModelProcessorThread->start();
	}
	
	installEventFilter( ui.renderBg );

	// temp
	//FileLoad( "D:/Sourcecode/AnalyticsViewer/AnalyticsViewer/etf_duel.obj" );

	AppendToLog( LOG_INFO, "Sample Information", QString( "Sample Additional Details\nFor the log entry" ) );
	AppendToLog( LOG_WARNING, "Sample Warning", QString() );
	AppendToLog( LOG_ERROR, "Sample Error", QString() );
	AppendToLog( LOG_DEBUG, "Sample Debug", QString() );
}

AnalyticsViewer::~AnalyticsViewer()
{
	mMessageThread->mRunning = false;
	mModelProcessorThread->mRunning = false;
	
	mMessageThread->wait( 5000 );
	mModelProcessorThread->wait( 5000 );
}

void AnalyticsViewer::AppendToLog( const LogCategory category, const QString & message, const QString & details )
{
	LogEntry log;
	log.mCategory = category;
	log.mMessage = message;
	log.mDetails = details;
	mLogEntries.push_back( log );

	AddToTable( log );
}

void AnalyticsViewer::AddToTable( const LogEntry & log )
{
	QRegExp expression( ui.editOutputFilter->text(), Qt::CaseInsensitive, QRegExp::Wildcard );
	if ( !expression.isEmpty() )
	{
		if ( expression.indexIn( log.mMessage ) == -1 &&
			expression.indexIn( log.mDetails ) == -1 )
			return;
	}

	QTableWidgetItem * categoryItem = NULL;
	switch ( log.mCategory )
	{
		case LOG_INFO:
			if ( !ui.toggleMessages->isChecked() )
				return;

			categoryItem = new QTableWidgetItem();
			categoryItem->setText( "INFO" );
			categoryItem->setForeground( QColor::fromRgb( 127, 127, 127 ) );
			break;
		case LOG_WARNING:
			if ( !ui.toggleWarnings->isChecked() )
				return;

			categoryItem = new QTableWidgetItem();
			categoryItem->setText( "WARN" );
			categoryItem->setForeground( QColor::fromRgb( 255, 127, 0 ) );
			break;
		case LOG_ERROR:
			if ( !ui.toggleErrors->isChecked() )
				return;

			categoryItem = new QTableWidgetItem();
			categoryItem->setText( "ERROR" );
			categoryItem->setForeground( QColor::fromRgb( 255, 0, 0 ) );
			break;
		case LOG_DEBUG:
			if ( !ui.toggleDebug->isChecked() )
				return;

			categoryItem = new QTableWidgetItem();
			categoryItem->setText( "DEBUG" );
			categoryItem->setForeground( QColor::fromRgb( 0, 0, 255 ) );
			break;
		default:
			categoryItem = new QTableWidgetItem();
			categoryItem->setText( "?" );
			categoryItem->setForeground( QColor::fromRgb( 255, 0, 255 ) );
			break;
	}

	QTableWidgetItem * messageItem = new QTableWidgetItem( log.mMessage );

	const int numRows = ui.tableOutput->rowCount();
	ui.tableOutput->insertRow( ui.tableOutput->rowCount() );
	ui.tableOutput->setItem( numRows, 0, categoryItem );
	ui.tableOutput->setItem( numRows, 1, messageItem );

	if ( !log.mDetails.isEmpty() )
	{
		QTableWidgetItem * detailsItem = new QTableWidgetItem();
		detailsItem->setIcon( QIcon( QStringLiteral( "Resources/svg/document98.svg" ) ) );
		detailsItem->setData( Qt::ToolTipRole, log.mDetails );
		ui.tableOutput->setItem( numRows, 2, detailsItem );
	}
}

void AnalyticsViewer::RebuildLogTable()
{
	ui.tableOutput->setUpdatesEnabled( false );
	while ( ui.tableOutput->rowCount() > 0 )
		ui.tableOutput->removeRow( 0 );
	for ( int i = 0; i < mLogEntries.size(); ++i )
		AddToTable( mLogEntries[ i ] );
	ui.tableOutput->setUpdatesEnabled( true );
}

void AnalyticsViewer::FileLoad( const QString & filePath )
{
	if ( ui.renderBg->importModelToScene( filePath, false ) )
		AppendToLog( LOG_INFO, tr( "Loading %1" ).arg( filePath ), QString() );
	else
		AppendToLog( LOG_INFO, tr( "Problem Loading %1" ).arg( filePath ), QString() );
}

void AnalyticsViewer::FileSave( const QString & filePath )
{
	if ( ui.renderBg->exportSceneToFile( filePath ) )
		AppendToLog( LOG_INFO, tr( "Saved %1" ).arg( filePath ), QString() );
	else
		AppendToLog( LOG_INFO, tr( "Problem Saving %1" ).arg( filePath ), QString() );
}

void AnalyticsViewer::FileOpen()
{
	QString openFileName = QFileDialog::getOpenFileName( this, tr( "Open Model" ), "", tr( "Model Files (*.obj, *.osgb)" ) );

	if ( !openFileName.isEmpty() )
	{
		FileLoad( openFileName );
	}
}

void AnalyticsViewer::FileSave()
{
	QString saveFileName = QFileDialog::getSaveFileName( this, tr( "Save Scene Graph" ), "", tr( "Model Files (*.*)" ) );

	if ( !saveFileName.isEmpty() )
	{
		FileSave( saveFileName );
	}
}

void AnalyticsViewer::SelectionChanged()
{
	ui.treeSelection->clear();
}

static void CheckSelection_r( QTreeWidgetItem * treeItem, const osg::Shape* shape )
{
	QTreeWidgetItem * shp = new QTreeWidgetItem();
	treeItem->addChild( shp );

	shp->setData( 0, Qt::DisplayRole, shape->className() );
	shp->setData( 1, Qt::DisplayRole, shape->getName().c_str() );

	if ( const osg::Sphere * shapeSphere = dynamic_cast<const osg::Sphere*>( shape ) )
	{
		shp->addChild( new QTreeWidgetItem( QStringList( { "Radius", QString( "%1" )
			.arg( shapeSphere->getRadius() ) } ) ) );

		shp->addChild( new QTreeWidgetItem( QStringList( { "Center", QString( "%1 %2 %3" )
			.arg( shapeSphere->getCenter()[ 0 ] )
			.arg( shapeSphere->getCenter()[ 1 ] )
			.arg( shapeSphere->getCenter()[ 2 ] ) } ) ) );
	}
	else
	{
		shp->setData( 1, Qt::DisplayRole, "Unhandled" );
	}
}

static void CheckSelection_r( QTreeWidgetItem * treeItem, const osg::PrimitiveSet* primset )
{
	QTreeWidgetItem * ps = new QTreeWidgetItem();
	treeItem->addChild( ps );

	ps->setData( 0, Qt::DisplayRole, primset->className() );
	ps->setData( 1, Qt::DisplayRole, primset->getName().c_str() );

	ps->addChild( new QTreeWidgetItem( QStringList( { "# Primitives", QString( "%1" )
		.arg( primset->getNumPrimitives() ) } ) ) );

	ps->addChild( new QTreeWidgetItem( QStringList( { "# Indices", QString( "%1" )
		.arg( primset->getNumIndices() ) } ) ) );

	ps->addChild( new QTreeWidgetItem( QStringList( { "# Instances", QString( "%1" )
		.arg( primset->getNumInstances() ) } ) ) );
	
	switch ( primset->getMode() )
	{
		case osg::PrimitiveSet::POINTS:
			ps->addChild( new QTreeWidgetItem( QStringList( { "Mode", "POINTS" } ) ) );
			break;
		case osg::PrimitiveSet::LINES:
			ps->addChild( new QTreeWidgetItem( QStringList( { "Mode", "LINES" } ) ) );
			break;
		case osg::PrimitiveSet::LINE_STRIP:
			ps->addChild( new QTreeWidgetItem( QStringList( { "Mode", "LINE_STRIP" } ) ) );
			break;
		case osg::PrimitiveSet::LINE_LOOP:
			ps->addChild( new QTreeWidgetItem( QStringList( { "Mode", "LINE_LOOP" } ) ) );
			break;
		case osg::PrimitiveSet::TRIANGLES:
			ps->addChild( new QTreeWidgetItem( QStringList( { "Mode", "TRIANGLES" } ) ) );
			break;
		case osg::PrimitiveSet::TRIANGLE_STRIP:
			ps->addChild( new QTreeWidgetItem( QStringList( { "Mode", "TRIANGLE_STRIP" } ) ) );
			break;
		case osg::PrimitiveSet::TRIANGLE_FAN:
			ps->addChild( new QTreeWidgetItem( QStringList( { "Mode", "TRIANGLE_FAN" } ) ) );
			break;
		case osg::PrimitiveSet::QUADS:
			ps->addChild( new QTreeWidgetItem( QStringList( { "Mode", "QUADS" } ) ) );
			break;
		case osg::PrimitiveSet::QUAD_STRIP:
			ps->addChild( new QTreeWidgetItem( QStringList( { "Mode", "QUAD_STRIP" } ) ) );
			break;
		case osg::PrimitiveSet::POLYGON:
			ps->addChild( new QTreeWidgetItem( QStringList( { "Mode", "POLYGON" } ) ) );
			break;
		case osg::PrimitiveSet::LINES_ADJACENCY:
			ps->addChild( new QTreeWidgetItem( QStringList( { "Mode", "LINES_ADJACENCY" } ) ) );
			break;
		case osg::PrimitiveSet::LINE_STRIP_ADJACENCY:
			ps->addChild( new QTreeWidgetItem( QStringList( { "Mode", "LINE_STRIP_ADJACENCY" } ) ) );
			break;
		case osg::PrimitiveSet::TRIANGLES_ADJACENCY:
			ps->addChild( new QTreeWidgetItem( QStringList( { "Mode", "TRIANGLES_ADJACENCY" } ) ) );
			break;
		case osg::PrimitiveSet::TRIANGLE_STRIP_ADJACENCY:
			ps->addChild( new QTreeWidgetItem( QStringList( { "Mode", "TRIANGLE_STRIP_ADJACENCY" } ) ) );
			break;
		case osg::PrimitiveSet::PATCHES:
			ps->addChild( new QTreeWidgetItem( QStringList( { "Mode", "PATCHES" } ) ) );
			break;
		default:
			ps->addChild( new QTreeWidgetItem( QStringList( { "Mode", "*UNKNOWN*" } ) ) );
			break;
	}
}

static void CheckSelection_r( QTreeWidgetItem * treeItem, const osg::Drawable* drawable )
{
	const osg::Geometry * geometry = drawable->asGeometry();
	if ( geometry )
	{
		QTreeWidgetItem * children = new QTreeWidgetItem();
		children->setData( 0, Qt::DisplayRole, "Primitive Sets" );
		children->setData( 1, Qt::DisplayRole, QVariant( geometry->getNumPrimitiveSets() ) );
		treeItem->addChild( children );

		for ( unsigned int i = 0; i < geometry->getNumPrimitiveSets(); ++i )
		{
			QTreeWidgetItem * child = new QTreeWidgetItem();
			child->setData( 0, Qt::DisplayRole, geometry->getPrimitiveSet( i )->className() );
			//child->setData( 1, Qt::DisplayRole, QVariant(  ) );
			children->addChild( child );

			CheckSelection_r( child, geometry->getPrimitiveSet( i ) );
		}
	}
	else
	{
		CheckSelection_r( treeItem, drawable->getShape() );
	}
}

static void CheckSelection_r( QTreeWidgetItem * treeItem, const osg::Node* node )
{
	if ( node->asGroup() )
	{
		const osg::Group * group = node->asGroup();

		QTreeWidgetItem * children = new QTreeWidgetItem();
		children->setData( 0, Qt::DisplayRole, "Children" );
		children->setData( 1, Qt::DisplayRole, QVariant( group->getNumChildren() ) );
		treeItem->addChild( children );

		for ( unsigned int i = 0; i < group->getNumChildren(); ++i )
		{
			QTreeWidgetItem * child = new QTreeWidgetItem();
			child->setData( 0, Qt::DisplayRole, group->getChild( i )->className() );
			child->setData( 1, Qt::DisplayRole, group->getChild( i )->getName().c_str() );
			children->addChild( child );

			CheckSelection_r( child, group->getChild( i ) );
		}
	}
	else if ( node->asGeode() )
	{
		const osg::Geode * geode = node->asGeode();

		QTreeWidgetItem * children = new QTreeWidgetItem();
		children->setData( 0, Qt::DisplayRole, "Drawables" );
		children->setData( 1, Qt::DisplayRole, QVariant( geode->getNumDrawables() ) );
		treeItem->addChild( children );

		for ( unsigned int i = 0; i < geode->getNumDrawables(); ++i )
		{
			/*QTreeWidgetItem * child = new QTreeWidgetItem();
			child->setData( 0, Qt::DisplayRole, geode->getDrawable( i )->className() );
			child->setData( 1, Qt::DisplayRole, geode->getDrawable( i )->getName().c_str() );
			children->addChild( child );*/

			CheckSelection_r( children, geode->getDrawable( i ) );
		}
	}
}


static std::string lookUpGLenumString( GLenum value )
{
	osgDB::ObjectWrapperManager* ow = osgDB::Registry::instance()->getObjectWrapperManager();

	{
		const osgDB::IntLookup& lookup = ow->getLookupMap()[ "GL" ];
		const osgDB::IntLookup::ValueToString& vts = lookup.getValueToString();
		osgDB::IntLookup::ValueToString::const_iterator itr = vts.find( value );
		if ( itr != vts.end() ) return itr->second;
	}

	{
		const osgDB::IntLookup& lookup = ow->getLookupMap()[ "PrimitiveType" ];
		const osgDB::IntLookup::ValueToString& vts = lookup.getValueToString();
		osgDB::IntLookup::ValueToString::const_iterator itr = vts.find( value );
		if ( itr != vts.end() ) return itr->second;
	}

	OSG_NOTICE << "Warning: LuaScriptEngine did not find valid GL enum value for GLenum value: " << value << std::endl;

	return std::string();
}


template<typename T>
static void CheckSelection( QTreeWidget * tree, QTreeWidgetItem * item, const T* obj )
{
	return;

	if ( item->isSelected() )
	{
		osgDB::ClassInterface::PropertyMap props;
		osgDB::ClassInterface ci;
		ci.getSupportedProperties( obj, props );

		tree->clear();
		
		for ( osgDB::ClassInterface::PropertyMap::iterator it = props.begin();
			it != props.end();
			++it )
		{			
			QTreeWidgetItem * item = new QTreeWidgetItem();
			item->setData( 0, Qt::DisplayRole, it->first.c_str() );
			item->setData( 1, Qt::DisplayRole, ci.getTypeName( it->second ).c_str() );
			tree->invisibleRootItem()->addChild( item );

			switch ( it->second )
			{
				case osgDB::BaseSerializer::RW_UNDEFINED:
				{
					break;
				}
				case osgDB::BaseSerializer::RW_USER:
				{
					break;
				}
				case osgDB::BaseSerializer::RW_OBJECT:
				case osgDB::BaseSerializer::RW_IMAGE:
				{
					osg::Object * objRef = NULL;
					if ( ci.getProperty( obj, it->first, objRef ) )
						item->setData( 1, Qt::DisplayRole, objRef ? objRef->getName().c_str() : "<NONE>" );
					break;
				}
				case osgDB::BaseSerializer::RW_LIST:
					break;
				case osgDB::BaseSerializer::RW_BOOL:
				{
					bool value;
					if ( ci.getProperty( obj, it->first, value ) )
						tree->setItemWidget( item, 1, new QCheckBox( tree ) );
					break;
				}
				case osgDB::BaseSerializer::RW_CHAR:
				{
					char value;
					if ( ci.getProperty( obj, it->first, value ) )
						item->setData( 1, Qt::DisplayRole, QVariant( value ) );
					break;
				}
				case osgDB::BaseSerializer::RW_UCHAR:
				{
					unsigned char value;
					if ( ci.getProperty( obj, it->first, value ) )
						item->setData( 1, Qt::DisplayRole, QVariant( value ) );
					break;
				}
				case osgDB::BaseSerializer::RW_SHORT:
				{
					short value;
					if ( ci.getProperty( obj, it->first, value ) )
						item->setData( 1, Qt::DisplayRole, QVariant( value ) );
					break;
				}
				case osgDB::BaseSerializer::RW_USHORT:
				{
					unsigned short value;
					if ( ci.getProperty( obj, it->first, value ) )
						item->setData( 1, Qt::DisplayRole, QVariant( value ) );
					break;
				}
				case osgDB::BaseSerializer::RW_INT:
				{
					int value;
					if ( ci.getProperty( obj, it->first, value ) )
						item->setData( 1, Qt::DisplayRole, QVariant( value ) );
					break;
				}
				case osgDB::BaseSerializer::RW_UINT:
				{
					unsigned int value;
					if ( ci.getProperty( obj, it->first, value ) )
						item->setData( 1, Qt::DisplayRole, QVariant( value ) );
					break;
				}
				case osgDB::BaseSerializer::RW_FLOAT:
				{
					float value;
					if ( ci.getProperty( obj, it->first, value ) )
						item->setData( 1, Qt::DisplayRole, QVariant( value ) );
					break;
				}
				case osgDB::BaseSerializer::RW_DOUBLE:
				{
					double value;
					if ( ci.getProperty( obj, it->first, value ) )
						item->setData( 1, Qt::DisplayRole, QVariant( value ) );
					break;
				}
				case osgDB::BaseSerializer::RW_VEC2F:
				{
					osg::Vec2f value;
					if ( ci.getProperty( obj, it->first, value ) )
						item->setData( 1, Qt::DisplayRole, QString( "%1 %2" ).arg( value.x() ).arg( value.y() ) );
					break;
				}
				case osgDB::BaseSerializer::RW_VEC2D:
				{
					osg::Vec2d value;
					if ( ci.getProperty( obj, it->first, value ) )
						item->setData( 1, Qt::DisplayRole, QString( "%1 %2" ).arg( value.x() ).arg( value.y() ) );
					break;
				}
				case osgDB::BaseSerializer::RW_VEC3F:
				{
					osg::Vec3f value;
					if ( ci.getProperty( obj, it->first, value ) )
						item->setData( 1, Qt::DisplayRole, QString( "%1 %2 %3" ).arg( value.x() ).arg( value.y() ).arg( value.z() ) );
					break;
				}
				case osgDB::BaseSerializer::RW_VEC3D:
				{
					osg::Vec3d value;
					if ( ci.getProperty( obj, it->first, value ) )
						item->setData( 1, Qt::DisplayRole, QString( "%1 %2 %3" ).arg( value.x() ).arg( value.y() ).arg( value.z() ) );
					break;
				}
				case osgDB::BaseSerializer::RW_VEC4F:
				{
					osg::Vec4f value;
					if ( ci.getProperty( obj, it->first, value ) )
					{
						QString str = QString::fromUtf8( "%1 %2 %3 %4" ).arg( value.x() ).arg( value.y() ).arg( value.z() ).arg( value.w() );
						item->setData( 1, Qt::DisplayRole, str );
					}
					break;
				}
				case osgDB::BaseSerializer::RW_VEC4D:
				{
					osg::Vec4d value;
					if ( ci.getProperty( obj, it->first, value ) )
						item->setData( 1, Qt::DisplayRole, QString( "%1 %2 %3 %4" ).arg( value.x() ).arg( value.y() ).arg( value.z() ).arg( value.w() ) );
					break;
				}
				case osgDB::BaseSerializer::RW_QUAT:
				{
					osg::Quat value;
					if ( ci.getProperty( obj, it->first, value ) )
						item->setData( 1, Qt::DisplayRole, QString( "%1 %2 %3 %4" ).arg( value.x() ).arg( value.y() ).arg( value.z() ).arg( value.w() ) );
					break;
				}
				case osgDB::BaseSerializer::RW_PLANE:
				{
					/*osg::Plane value;
					if ( ci.getProperty( obj, it->first, value ) )
						item->setData( 1, Qt::DisplayRole, QString( "%f %f %f %f" ).arg( value.x() ).arg( value.y() ).arg( value.z() ).arg( value.w() ) );*/
					break;
				}
				case osgDB::BaseSerializer::RW_MATRIXF:
				{
					break;
				}
				case osgDB::BaseSerializer::RW_MATRIXD:
				{
					break;
				}
				case osgDB::BaseSerializer::RW_MATRIX:
				{
					break;
				}
				case osgDB::BaseSerializer::RW_GLENUM:
				{
					GLenum value;
					if ( ci.getProperty( obj, it->first, value ) )
						item->setData( 1, Qt::DisplayRole, lookUpGLenumString( value ).c_str() );
					break;
				}
				case osgDB::BaseSerializer::RW_STRING:
				{
					std::string value;
					if ( ci.getProperty( obj, it->first, value ) )
						tree->setItemWidget( item, 1, new QTextEdit( tree ) );
					break;
				}
				case osgDB::BaseSerializer::RW_ENUM:
					break;
				case osgDB::BaseSerializer::RW_VEC2B:
				{
					/*osg::Vec2b value;
					if ( ci.getProperty( obj, it->first, value ) )
						item->setData( 1, Qt::DisplayRole, QString( "%f %f" ).arg( value.x() ).arg( value.y() ) );*/
					break;
				}
				case osgDB::BaseSerializer::RW_VEC2UB:
				{
					/*osg::Vec2ub value;
					if ( ci.getProperty( obj, it->first, value ) )
						item->setData( 1, Qt::DisplayRole, QString( "%f %f" ).arg( value.x() ).arg( value.y() ) );*/
					break;
				}
				case osgDB::BaseSerializer::RW_VEC2S:
				{
					/*osg::Vec2s value;
					if ( ci.getProperty( obj, it->first, value ) )
						item->setData( 1, Qt::DisplayRole, QString( "%f %f" ).arg( value.x() ).arg( value.y() ) );*/
					break;
				}
				case osgDB::BaseSerializer::RW_VEC2US:
				{
					/*osg::Vec2us value;
					if ( ci.getProperty( obj, it->first, value ) )
						item->setData( 1, Qt::DisplayRole, QString( "%f %f" ).arg( value.x() ).arg( value.y() ) );*/
					break;
				}
				case osgDB::BaseSerializer::RW_VEC2I:
				{
					/*osg::Vec2i value;
					if ( ci.getProperty( obj, it->first, value ) )
						item->setData( 1, Qt::DisplayRole, QString( "%f %f" ).arg( value.x() ).arg( value.y() ) );*/
					break;
				}
				case osgDB::BaseSerializer::RW_VEC2UI:
				{
					/*osg::Vec2ui value;
					if ( ci.getProperty( obj, it->first, value ) )
						item->setData( 1, Qt::DisplayRole, QString( "%f %f" ).arg( value.x() ).arg( value.y() ) );*/
					break;
				}
				case osgDB::BaseSerializer::RW_VEC3B:
				{
					/*osg::Vec3b value;
					if ( ci.getProperty( obj, it->first, value ) )
						item->setData( 1, Qt::DisplayRole, QString( "%f %f %f" ).arg( value.x() ).arg( value.y() ).arg( value.z() ) );*/
					break;
				}
				case osgDB::BaseSerializer::RW_VEC3UB:
				{
					/*osg::Vec3ub value;
					if ( ci.getProperty( obj, it->first, value ) )
						item->setData( 1, Qt::DisplayRole, QString( "%f %f %f" ).arg( value.x() ).arg( value.y() ).arg( value.z() ) );*/
					break;
				}
				case osgDB::BaseSerializer::RW_VEC3S:
				{
					/*osg::Vec3s value;
					if ( ci.getProperty( obj, it->first, value ) )
						item->setData( 1, Qt::DisplayRole, QString( "%f %f %f" ).arg( value.x() ).arg( value.y() ).arg( value.z() ) );*/
					break;
				}
				case osgDB::BaseSerializer::RW_VEC3US:
				{
					/*osg::Vec3us value;
					if ( ci.getProperty( obj, it->first, value ) )
						item->setData( 1, Qt::DisplayRole, QString( "%f %f %f" ).arg( value.x() ).arg( value.y() ).arg( value.z() ) );*/
					break;
				}
				case osgDB::BaseSerializer::RW_VEC3I:
				{
					/*osg::Vec3i value;
					if ( ci.getProperty( obj, it->first, value ) )
						item->setData( 1, Qt::DisplayRole, QString( "%f %f %f" ).arg( value.x() ).arg( value.y() ).arg( value.z() ) );*/
					break;
				}
				case osgDB::BaseSerializer::RW_VEC3UI:
				{
					/*osg::Vec3ui value;
					if ( ci.getProperty( obj, it->first, value ) )
						item->setData( 1, Qt::DisplayRole, QString( "%f %f %f" ).arg( value.x() ).arg( value.y() ).arg( value.z() ) );*/
					break;
				}
				case osgDB::BaseSerializer::RW_VEC4B:
				{
					/*osg::Vec4b value;
					if ( ci.getProperty( obj, it->first, value ) )
						item->setData( 1, Qt::DisplayRole, QString( "%f %f %f %f" ).arg( value.x() ).arg( value.y() ).arg( value.z() ).arg( value.w() ) );*/
					break;
				}
				case osgDB::BaseSerializer::RW_VEC4UB:
				{
					/*osg::Vec4ub value;
					if ( ci.getProperty( obj, it->first, value ) )
						item->setData( 1, Qt::DisplayRole, QString( "%f %f %f %f" ).arg( value.r() ).arg( value.g() ).arg( value.b() ).arg( value.a() ) );*/
					break;
				}
				case osgDB::BaseSerializer::RW_VEC4S:
				{
					/*osg::Vec4s value;
					if ( ci.getProperty( obj, it->first, value ) )
						item->setData( 1, Qt::DisplayRole, QString( "%f %f %f %f" ).arg( value.x() ).arg( value.y() ).arg( value.z() ).arg( value.w() ) );*/
					break;
				}
				case osgDB::BaseSerializer::RW_VEC4US:
				{
					/*osg::Vec4us value;
					if ( ci.getProperty( obj, it->first, value ) )
						item->setData( 1, Qt::DisplayRole, QString( "%f %f %f %f" ).arg( value.x() ).arg( value.y() ).arg( value.z() ).arg( value.w() ) );*/
					break;
				}
				case osgDB::BaseSerializer::RW_VEC4I:
				{
					/*osg::Vec4i value;
					if ( ci.getProperty( obj, it->first, value ) )
						item->setData( 1, Qt::DisplayRole, QString( "%f %f %f %f" ).arg( value.x() ).arg( value.y() ).arg( value.z() ).arg( value.w() ) );*/
					break;
				}
				case osgDB::BaseSerializer::RW_VEC4UI:
				{
					/*osg::Vec4ui value;
					if ( ci.getProperty( obj, it->first, value ) )
					item->setData( 1, Qt::DisplayRole, QString( "%f %f %f %f" ).arg( value.x() ).arg( value.y() ).arg( value.z() ).arg( value.w() ) );*/
					break;
				}
				case osgDB::BaseSerializer::RW_BOUNDINGBOXF:
				{
					osg::BoundingBox value;
					if ( ci.getProperty( obj, it->first, value ) )
						item->setData( 1, Qt::DisplayRole, QString( "BBOX" ) );
					break;
				}
				case osgDB::BaseSerializer::RW_BOUNDINGBOXD:
				{
					osg::BoundingBoxd value;
					if ( ci.getProperty( obj, it->first, value ) )
						item->setData( 1, Qt::DisplayRole, QString( "BBOXD" ) );
					break;
				}
				case osgDB::BaseSerializer::RW_BOUNDINGSPHEREF:
				{
					osg::BoundingSphere value;
					if ( ci.getProperty( obj, it->first, value ) )
						item->setData( 1, Qt::DisplayRole, QString( "BSPHERE" ) );
					break;
				}
				case osgDB::BaseSerializer::RW_BOUNDINGSPHERED:
				{
					osg::BoundingSphered value;
					if ( ci.getProperty( obj, it->first, value ) )
						item->setData( 1, Qt::DisplayRole, QString( "BSPHERED" ) );
					break;
				}
				case osgDB::BaseSerializer::RW_VECTOR:
				{
					break;
				}
				case osgDB::BaseSerializer::RW_MAP:
				{
					break;
				}
			}
		}
		tree->expandAll();
		/*tree->clear();
		CheckSelection_r( tree->invisibleRootItem(), obj );
		tree->expandAll();*/
	}
}

QString getNodeIcon( const osg::Node * node )
{
	QString nodeIcon = QStringLiteral( "Resources/svg/help19.svg" );

	if ( node->asCamera() )
	{
		nodeIcon = QStringLiteral( "Resources/svg/camera8.svg" );
	}
	else if ( node->asTransform() )
	{
		nodeIcon = QStringLiteral( "Resources/svg/3d76.svg" );
	}
	else if ( node->asGeode() )
	{
		nodeIcon = QStringLiteral( "Resources/svg/geometry2.svg" );
	}
	else if ( node->asSwitch() )
	{
		nodeIcon = QStringLiteral( "Resources/svg/circuit.svg" );
	}
	else if ( node->asTerrain() )
	{
		nodeIcon = QStringLiteral( "Resources/svg/mountain13.svg" );
	}
	else if ( node->asGroup() )
	{
		nodeIcon = QStringLiteral( "Resources/svg/family3.svg" );
	}
	return nodeIcon;
}

void AnalyticsViewer::UpdateHierarchy()
{
	class UpdateTree : public osg::NodeVisitor
	{
	public:
		UpdateTree( QTreeWidgetItem * item, QTreeWidget * sel )
			: osg::NodeVisitor( NodeVisitor::NODE_VISITOR )
			, mTreeItem( item )
			, mSelectionTree( sel )
		{
			setTraversalMode( NodeVisitor::TRAVERSE_ALL_CHILDREN );
		}

		QTreeWidgetItem * findChildItem( QTreeWidgetItem * searchUnder, qulonglong itemId )
		{
			QTreeWidgetItem * nodeItem = NULL;

			for ( int c = 0; c < searchUnder->childCount(); ++c )
			{
				QTreeWidgetItem * child = searchUnder->child( c );
				if ( child->data( 0, Qt::UserRole ).toULongLong() == itemId )
				{
					nodeItem = child;
					break;
				}
			}

			return nodeItem;
		}
		
		virtual void apply( osg::Node & node )
		{			
			QTreeWidgetItem * treeLevel = mTreeItem;
			for ( size_t i = 0; i < _nodePath.size(); ++i )
			{
				const qulonglong nodePathUID = (qulonglong)_nodePath[ i ];

				QTreeWidgetItem * nodeItem = findChildItem( treeLevel, nodePathUID );
				
				if ( nodeItem == NULL )
				{
					// create a readable name for it. it may not be unique
					QString nodeName =  _nodePath[ i ]->getName().c_str();
					QString nodeIcon = getNodeIcon( _nodePath[ i ] );
					QString nodeType = _nodePath[ i ]->className();
					
					if ( nodeName.isEmpty() )
						nodeName = "Unnamed";
					
					// doesn't exist yet, create it
					nodeItem = new QTreeWidgetItem( treeLevel );
					nodeItem->setIcon( 0, QIcon( nodeIcon ) );
					nodeItem->setData( 0, Qt::UserRole, nodePathUID );
					nodeItem->setData( 0, Qt::DisplayRole, nodeName );
					nodeItem->setData( 1, Qt::DisplayRole, nodeType );
					nodeItem->setExpanded( true );
					treeLevel->addChild( nodeItem );
				}

				CheckSelection( mSelectionTree, nodeItem, _nodePath[ i ] );

				// Some node types have different types of child items
				if ( _nodePath[ i ]->asGeode() )
				{
					osg::Geode * geode = _nodePath[ i ]->asGeode();

					for ( int d = 0; d < geode->getNumDrawables(); ++d )
					{
						const osg::Drawable * drawable = geode->getDrawable( d );

						const qulonglong drawableUID = (qulonglong)drawable;

						QTreeWidgetItem * drawableItem = findChildItem( nodeItem, drawableUID );

						if ( drawableItem == NULL )
						{
							drawableItem = new QTreeWidgetItem( nodeItem );
							drawableItem->setIcon( 0, QIcon( QStringLiteral( "Resources/svg/geometry2.svg" ) ) );
							drawableItem->setData( 0, Qt::UserRole, drawableUID );
							drawableItem->setData( 0, Qt::DisplayRole, "Shape" );
							drawableItem->setData( 1, Qt::DisplayRole, drawable->className() );
							drawableItem->setExpanded( true );
							nodeItem->addChild( drawableItem );
						}

						CheckSelection( mSelectionTree, drawableItem, drawable );

						const osg::Geometry * geometry = drawable->asGeometry();
						if ( geometry )
						{
							for ( int p = 0; p < geometry->getNumPrimitiveSets(); ++p )
							{
								const osg::PrimitiveSet * pset = geometry->getPrimitiveSet( p );

								const qulonglong psetUID = (qulonglong)pset;

								QTreeWidgetItem * primSetItem = findChildItem( drawableItem, psetUID );

								if ( primSetItem == NULL )
								{
									primSetItem = new QTreeWidgetItem( drawableItem );
									primSetItem->setIcon( 0, QIcon( getNodeIcon( _nodePath[ i ] ) ) );
									primSetItem->setData( 0, Qt::UserRole, psetUID );
									primSetItem->setData( 0, Qt::DisplayRole, "PrimitiveSet" );
									primSetItem->setData( 1, Qt::DisplayRole, pset->className() );
									primSetItem->setExpanded( true );
									drawableItem->addChild( primSetItem );
								}

								CheckSelection( mSelectionTree, primSetItem, pset );
							}
						}
						else
						{
							const osg::Shape * shape = drawable->getShape();

							const qulonglong shapeUID = (qulonglong)shape;
							QTreeWidgetItem * shapeItem = findChildItem( drawableItem, shapeUID );

							if ( shapeItem == NULL )
							{
								shapeItem = new QTreeWidgetItem( drawableItem );
								shapeItem->setIcon( 0, QIcon( getNodeIcon( _nodePath[ i ] ) ) );
								shapeItem->setData( 0, Qt::UserRole, shapeUID );
								shapeItem->setData( 0, Qt::DisplayRole, "Shape" );
								shapeItem->setData( 1, Qt::DisplayRole, shape->className() );
								shapeItem->setExpanded( true );
								drawableItem->addChild( shapeItem );
							}

							CheckSelection( mSelectionTree, shapeItem, shape );
						}
					}
				}


				treeLevel = nodeItem;
			}

			traverse( node );
		}
	protected:
		QTreeWidgetItem *	mTreeItem;
		QTreeWidget *		mSelectionTree;
	};

	UpdateTree upd( ui.treeHierarchy->invisibleRootItem(), ui.treeSelection );
	ui.renderBg->visitSceneNodes( upd );
}
void AnalyticsViewer::LogInfo( const QString & msg, const QString & details )
{
	AppendToLog( LOG_INFO, msg, details );
}
void AnalyticsViewer::LogWarn( const QString & msg, const QString & details )
{
	AppendToLog( LOG_WARNING, msg, details );
}
void AnalyticsViewer::LogError( const QString & msg, const QString & details )
{
	AppendToLog( LOG_ERROR, msg, details );
}
void AnalyticsViewer::LogDebug( const QString & msg, const QString & details )
{
	AppendToLog( LOG_DEBUG, msg, details );
}
void AnalyticsViewer::StatusInfo( const QString & msg )
{
	mNetworkLabel->setText( msg );
}
osg::Vec3d Convert( const modeldata::Vec3 & vec )
{
	return osg::Vec3d( vec.x(), vec.y(), vec.z() );
}

void AnalyticsViewer::processMessage( const Analytics::GameEntityInfo & msg )
{
	return;

	osg::MatrixTransform * xform = ui.renderBg->findOrCreateEntityNode( msg.entityid() );

	// if it's just been created, make a representation for it
	if ( xform->getNumChildren() == 0 )
	{
		// cylinder for the body
		osg::ref_ptr<osgDB::Options> options = new osgDB::Options( "generateFacetNormals noTriStripPolygons mergeMeshes noRotation" );
		options->setObjectCacheHint( osgDB::Options::CACHE_NODES );

		osg::ProxyNode * entityProxy = new osg::ProxyNode();
		entityProxy->setFileName( 0, "monito.obj" );
		entityProxy->setDatabaseOptions( options );

		xform->addChild( entityProxy );

		/*osg::ShapeDrawable* lightmarker = new osg::ShapeDrawable( new osg::Sphere( osg::Vec3( 0.f, 0.f, 0.f ), 10.f ) );
		lightmarker->setName( "Sphere" );

		osg::Geode* geode = new osg::Geode;
		geode->setName( "LightMarker" );
		geode->addDrawable( lightmarker );
		xform->addChild( geode );*/
	}

	osg::Matrix mat = xform->getMatrix();
	mat.makeScale( osg::Vec3d( 64.0, 64.0, 64.0 ) );
	mat.setTrans( osg::Vec3d( msg.positionx(), msg.positiony(), msg.positionz() ) );
	xform->setMatrix( mat );

	/*if ( msg.has_orient() )
	{
		osg::Quat q;
		q.makeRotate( msg->orient().heading(), msg->orient().pitch(), msg->orient().roll() );

		osg::Matrix mat( q );
		mat.setTrans( xform->getMatrix().getTrans() );

		xform->setMatrix( mat );
	}*/
}

void AnalyticsViewer::processMessage( MessageUnionPtr msg )
{
	if ( msg->has_gameentitylist() )
	{
		const Analytics::GameEntityList& elist = msg->gameentitylist();
		for ( int i = 0; i < elist.entities_size(); ++i )
		{
			processMessage( elist.entities( i ) );
		}
	}
}
