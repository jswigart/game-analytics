#include <QtQml/QQmlContext>
#include <Qt3DInput/QInputAspect>
#include <Qt3DRenderer/QRenderAspect>
#include <QtQml>

#include <QMetaProperty>
#include <QCheckBox>
#include <QTextEdit>
#include <QFileDialog>
#include <QTimer>
#include <strstream>
#include <QVector3D>

#include "analyticsviewer.h"
#include "window.h"

#include "Messaging.h"
#include "ModelProcessor.h"

#include "AnalyticsScene.h"

#include "qtvariantproperty.h"
#include "analytics.pb.h"

AnalyticsViewer::AnalyticsViewer( QWidget *parent )
	: QMainWindow( parent )
{
	qmlRegisterType<QAnalyticModel>( "Analytics", 1, 0, "Mesh" );
	qmlRegisterType<AnalyticsScene>( "Analytics", 1, 0, "Scene" );
	
	ui.setupUi( this );
	
	// Initialize Qt3d QML
	Window* view = new Window();

	QWidget *container = QWidget::createWindowContainer( view );

	container->setMinimumSize( QSize( 100, 100 ) );

	mEngine.aspectEngine()->registerAspect( new Qt3D::QRenderAspect() );
	mEngine.aspectEngine()->registerAspect( new Qt3D::QInputAspect() );

	QVariantMap data;
	data.insert( QStringLiteral( "surface" ), QVariant::fromValue( static_cast<QSurface *>( view ) ) );
	data.insert( QStringLiteral( "eventSource" ), QVariant::fromValue( view ) );
	mEngine.aspectEngine()->setData( data );
	mEngine.aspectEngine()->initialize();
	mEngine.qmlEngine()->rootContext()->setContextProperty( "_window", view );
	mEngine.setSource( QUrl( "main.qml" ) );
	
	ui.renderLayout->addWidget( container );
	view->show();
	
	mVariantPropMgr = new QtVariantPropertyManager( this );
	mVariantEditor = new QtVariantEditorFactory( this );

	ui.props->setFactoryForManager( mVariantPropMgr, mVariantEditor );

	/*QtVariantProperty * prop;
	prop = mVariantPropMgr->addProperty( QVariant::Int, "Some Int" );
	prop->setValue( QVariant( 10 ) );
	ui.props->addProperty( prop );

	prop = mVariantPropMgr->addProperty( QVariant::Double, "Some Dbl" );
	prop->setValue( QVariant( 0.5 ) );
	ui.props->addProperty( prop );

	prop = mVariantPropMgr->addProperty( QVariant::Color, "Some Color" );
	prop->setValue( QVariant( QColor( "red" ) ) );
	ui.props->addProperty( prop );

	prop = mVariantPropMgr->addProperty( QVariant::SizeF, "Some Size" );
	prop->setValue( QVariant( QSize( 10, 20 ) ) );
	ui.props->addProperty( prop );

	prop = mVariantPropMgr->addProperty( QVariant::Int, "Some Int" );
	prop->setValue( QVariant( 10 ) );
	ui.props->addProperty( prop );

	prop = mVariantPropMgr->addProperty( QVariant::Int, "Some Int" );
	prop->setValue( QVariant( 10 ) );
	ui.props->addProperty( prop );

	prop = mVariantPropMgr->addProperty( QVariant::Int, "Some Int" );
	prop->setValue( QVariant( 10 ) );
	ui.props->addProperty( prop );

	prop = mVariantPropMgr->addProperty( QVariant::Int, "Some Int" );
	prop->setValue( QVariant( 10 ) );
	ui.props->addProperty( prop );

	prop = mVariantPropMgr->addProperty( QVariant::Int, "Some Int" );
	prop->setValue( QVariant( 10 ) );
	ui.props->addProperty( prop );*/

	ui.treeHierarchy->invisibleRootItem()->setExpanded( true );

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

	//{
	//	QStringList labels;
	//	labels.push_back( "Property" );
	//	labels.push_back( "Value" );

	//	// Configure the selection tree
	//	ui.treeSelection->setColumnCount( 2 );
	//	ui.treeSelection->setHeaderLabels( labels );
	//	ui.treeSelection->setColumnWidth( 0, 300 );
	//	ui.treeSelection->setColumnWidth( 1, 100 );
	//}

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
		connect( mModelProcessorThread, SIGNAL( allocModel( Qt3D::QEntity* ) ), mEngine.aspectEngine()->rootEntity().data(), SLOT( AddToScene( Qt3D::QEntity* ) ) );
		
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
	/*if ( ui.renderBg->importModelToScene( filePath, false ) )
		AppendToLog( LOG_INFO, tr( "Loading %1" ).arg( filePath ), QString() );
	else
		AppendToLog( LOG_INFO, tr( "Problem Loading %1" ).arg( filePath ), QString() );*/
}

void AnalyticsViewer::FileSave( const QString & filePath )
{
	/*if ( ui.renderBg->exportSceneToFile( filePath ) )
		AppendToLog( LOG_INFO, tr( "Saved %1" ).arg( filePath ), QString() );
	else
		AppendToLog( LOG_INFO, tr( "Problem Saving %1" ).arg( filePath ), QString() );*/
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
	ui.props->clear();
}

//QString getNodeIcon( const osg::Node * node )
//{
//	QString nodeIcon = QStringLiteral( "Resources/svg/help19.svg" );
//
//	if ( node->asCamera() )
//	{
//		nodeIcon = QStringLiteral( "Resources/svg/camera8.svg" );
//	}
//	else if ( node->asTransform() )
//	{
//		nodeIcon = QStringLiteral( "Resources/svg/3d76.svg" );
//	}
//	else if ( node->asGeode() )
//	{
//		nodeIcon = QStringLiteral( "Resources/svg/geometry2.svg" );
//	}
//	else if ( node->asSwitch() )
//	{
//		nodeIcon = QStringLiteral( "Resources/svg/circuit.svg" );
//	}
//	else if ( node->asTerrain() )
//	{
//		nodeIcon = QStringLiteral( "Resources/svg/mountain13.svg" );
//	}
//	else if ( node->asGroup() )
//	{
//		nodeIcon = QStringLiteral( "Resources/svg/family3.svg" );
//	}
//	return nodeIcon;
//}

QTreeWidgetItem * AnalyticsViewer::FindChildItem( QTreeWidgetItem * searchUnder, qulonglong itemId )
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

QTreeWidgetItem * AnalyticsViewer::FindOrAdd( QTreeWidgetItem * parent, const QString& str, qulonglong id, const QVariant & info )
{
	QTreeWidgetItem * item = FindChildItem( parent, id );
	if ( item == NULL )
	{
		item = new QTreeWidgetItem( parent );
		item->setIcon( 0, QIcon( QStringLiteral( "Resources/svg/family3.svg" ) ) );
		item->setData( 0, Qt::UserRole, id );
		item->setData( 0, Qt::DisplayRole, str );
		item->setData( 1, Qt::DisplayRole, info );
		item->setExpanded( false );
		parent->addChild( item );
	}
	return item;
}

void AnalyticsViewer::ShowProperties( QTreeWidgetItem * treeItem, const QObject * obj )
{
	QHash<QString, QVariant> list;
	for ( int i = 0; i < obj->metaObject()->propertyCount(); i++ )
	{
		QMetaProperty property = obj->metaObject()->property( i );
		const char* name = property.name();
		QVariant value = obj->property( name );
		
		FindOrAdd( treeItem, name, (qulonglong)i, value );
	}
}

void AnalyticsViewer::WalkHierarchy( Qt3D::QEntity* entity, QTreeWidgetItem * treeItem )
{
	QTreeWidgetItem * entityItem = FindOrAdd( treeItem, entity->metaObject()->className(), (qulonglong)entity );
	
	ShowProperties( entityItem, entity );

	Qt3D::QComponentList components = entity->components();
	for ( int i = 0; i < components.size(); ++i )
	{
		Qt3D::QComponent* comp = components[ i ];
		
		QTreeWidgetItem * compItem = FindOrAdd( entityItem, comp->metaObject()->className(), (qulonglong)comp );
		
		ShowProperties( compItem, comp );
	}

	const QObjectList & ch = entity->children();
	for ( int i = 0; i < ch.size(); ++i )
	{
		Qt3D::QEntity* ent = qobject_cast<Qt3D::QEntity*>( ch[ i ] );
		if ( ent != NULL )
		{
			WalkHierarchy( ent, entityItem );
		}
	}
}

void AnalyticsViewer::UpdateHierarchy()
{
	WalkHierarchy( mEngine.aspectEngine()->rootEntity().data(), ui.treeHierarchy->invisibleRootItem() );
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

QVector3D Convert( const modeldata::Vec3 & vec )
{
	return QVector3D( vec.x(), vec.y(), vec.z() );
}

void AnalyticsViewer::processMessage( const Analytics::GameEntityInfo & msg )
{
	return;
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
