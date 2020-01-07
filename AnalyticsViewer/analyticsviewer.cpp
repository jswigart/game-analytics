#include <QtQml/QQmlContext>
#include <Qt3DInput/QInputAspect>
#include <Qt3DCore/QComponent>
#include <Qt3DRender/QRenderAspect>
#include <Qt3DLogic/QLogicAspect>
#include <QtCore/QMetaType>
#include <Qt3DCore/QNode>
#include <QtQml/QtQml>
#include <QtQml/QQmlListProperty>
#include <QtWidgets/QGraphicsTextItem>
#include <Qt3DExtras/QForwardRenderer>

#include <QtCore/QMetaProperty>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QFileDialog>
#include <QtCore/QTimer>
#include <strstream>
#include <QtGui/QVector2D>
#include <QtGui/QVector3D>
#include <QtGui/QVector4D>
#include <QtGui/QMatrix4x4>

#include "analyticsviewer.h"
#include "RenderWindow.h"

#include "Messaging.h"

#include "AnalyticsScene.h"

#include "qtvariantproperty.h"
#include "analytics.pb.h"

// f u windows
#undef GetMessage

QVariantMap VariantMapFromMessage(const google::protobuf::Message & msg);
QVariant VariantFromField(const google::protobuf::Message & msg, const google::protobuf::FieldDescriptor* fdesc);

AnalyticsViewer::AnalyticsViewer(QWidget *parent)
	: QMainWindow(parent)
{
	qRegisterMetaType<MessageUnionPtr>("MessageUnionPtr");

	qmlRegisterType<AnalyticsScene>("Analytics", 1, 0, "Scene");

	ui.setupUi(this);

	ui.toggleMessages->setProperty("Title", QString("Info (%1)"));
	ui.toggleWarnings->setProperty("Title", QString("Warning (%1)"));
	ui.toggleErrors->setProperty("Title", QString("Errors (%1)"));
	ui.toggleDebug->setProperty("Title", QString("Debug (%1)"));

	// Initialize Qt3d QML
	mView = new RenderWindow();
	
	QWidget *container = QWidget::createWindowContainer(mView);
	
	container->setMinimumSize(QSize(100, 100));
	container->updateGeometry();
	
	mView->setSource(QUrl("main.qml"));
	
	connect(mView->engine(), SIGNAL(sceneCreated(QObject*)), this, SLOT(sceneCreated(QObject*)));

	//mEngine.qmlEngine()->rootContext()->setContextProperty("_window", mView);
	//mEngine.qmlEngine()->rootContext()->setContextProperty("_engine", mView);
		
	connect(mView->engine()->aspectEngine()->rootEntity().data(), SIGNAL(info(QString, QString)), this, SLOT(LogInfo(QString, QString)));
	connect(mView->engine()->aspectEngine()->rootEntity().data(), SIGNAL(warn(QString, QString)), this, SLOT(LogWarn(QString, QString)));
	connect(mView->engine()->aspectEngine()->rootEntity().data(), SIGNAL(error(QString, QString)), this, SLOT(LogError(QString, QString)));
	connect(mView->engine()->aspectEngine()->rootEntity().data(), SIGNAL(debug(QString, QString)), this, SLOT(LogDebug(QString, QString)));

	ui.renderLayout->addWidget(container);
	
	mVariantPropMgr = new QtVariantPropertyManager(this);
	mVariantEditor = new QtVariantEditorFactory(this);

	ui.props->setFactoryForManager(mVariantPropMgr, mVariantEditor);

	ui.treeHierarchy->invisibleRootItem()->setExpanded(true);

	// Hook up signals
	connect(ui.actionOpen, SIGNAL(triggered(bool)), this, SLOT(FileOpen()));
	connect(ui.actionSaveScene, SIGNAL(triggered(bool)), this, SLOT(FileSave()));
	connect(ui.toggleMessages, SIGNAL(toggled(bool)), this, SLOT(RebuildLogTable()));
	connect(ui.toggleWarnings, SIGNAL(toggled(bool)), this, SLOT(RebuildLogTable()));
	connect(ui.toggleErrors, SIGNAL(toggled(bool)), this, SLOT(RebuildLogTable()));
	connect(ui.toggleDebug, SIGNAL(toggled(bool)), this, SLOT(RebuildLogTable()));
	connect(ui.editOutputFilter, SIGNAL(textChanged(const QString &)), this, SLOT(RebuildLogTable()));
	connect(ui.treeHierarchy, SIGNAL(itemSelectionChanged()), this, SLOT(TreeSelectionChanged()));
	connect(mVariantPropMgr, SIGNAL(valueChanged(QtProperty *, const QVariant &)), this, SLOT(PropertyChanged(QtProperty *, const QVariant &)), Qt::DirectConnection);

	ui.viewTimeline->setScene(&mTimeline);
	
	connect(&mTimeline, SIGNAL(selectionChanged()), this, SLOT(TimelineSelectionChanged()));
	
	// schedule some processor time intervals
	{
		QTimer *timer = new QTimer(this);
		connect(timer, SIGNAL(timeout()), this, SLOT(UpdateHierarchy()));
		timer->start(1000);
	}

	{
		QTableWidgetItem * typeItem = new QTableWidgetItem("Type");
		typeItem->setData(Qt::TextAlignmentRole, Qt::AlignLeft);
		QTableWidgetItem * descItem = new QTableWidgetItem("Description");
		descItem->setData(Qt::TextAlignmentRole, Qt::AlignLeft);
		QTableWidgetItem * detailsItem = new QTableWidgetItem();
		detailsItem->setIcon(QIcon(QStringLiteral("Resources/svg/document98.svg")));

		// Configure the output table
		ui.tableOutput->setColumnCount(3);
		ui.tableOutput->setHorizontalHeaderItem(0, typeItem);
		ui.tableOutput->setHorizontalHeaderItem(1, descItem);
		ui.tableOutput->setHorizontalHeaderItem(2, detailsItem);
		ui.tableOutput->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
		ui.tableOutput->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
		ui.tableOutput->horizontalHeader()->resizeSection(1, 400);
		ui.tableOutput->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Interactive);
		ui.tableOutput->horizontalHeader()->resizeSection(2, 30);
	}

	{
		const google::protobuf::Descriptor* desc = Analytics::MessageUnion::descriptor();
		const google::protobuf::OneofDescriptor* oneOf = desc->FindOneofByName("msg");
		if (oneOf != NULL)
		{
			const int numFields = oneOf->field_count();
			ui.tableEvents->setRowCount(numFields);

			for (int i = 0; i < numFields; ++i)
			{
				const google::protobuf::FieldDescriptor* fdesc = oneOf->field(i);

				EventInfo info;
				info.mItemMessage = new QTableWidgetItem(QString(fdesc->name().c_str()));
				info.mItemCount = new QTableWidgetItem(QString("%1").arg(0));
				info.mItemCount->setData(Qt::UserRole, QVariant(0));

				ui.tableEvents->setItem(i, 0, info.mItemMessage);
				ui.tableEvents->setItem(i, 1, info.mItemCount);

				info.mRow = mTimeline.AddRow(QString(fdesc->name().c_str()));

				mEventMap[fdesc->number()] = info;
			}
		}
		ui.tableEvents->resizeColumnsToContents();
	}

	{
		QLineEdit * networkIp = new QLineEdit(this);
		networkIp->setFixedWidth(100);
		networkIp->setText("127.0.0.1");

		QLineEdit * networkPort = new QLineEdit(this);
		networkPort->setFixedWidth(50);
		networkPort->setText("5050");

		mNetworkLabel = new QLabel(this);

		statusBar()->addWidget(networkIp);
		statusBar()->addWidget(networkPort);
		statusBar()->addWidget(mNetworkLabel);
	}
}

AnalyticsViewer::~AnalyticsViewer()
{
	mMessageThread->mRunning = false;
	
	mMessageThread->wait(5000);

	/*delete mView;
	mView = NULL;*/
}

void AnalyticsViewer::sceneCreated(QObject* rootObject)
{
	mMessageThread = new HostThread0MQ(this);
	connect(mMessageThread, SIGNAL(info(QString, QString)), this, SLOT(LogInfo(QString, QString)));
	connect(mMessageThread, SIGNAL(warn(QString, QString)), this, SLOT(LogWarn(QString, QString)));
	connect(mMessageThread, SIGNAL(error(QString, QString)), this, SLOT(LogError(QString, QString)));
	connect(mMessageThread, SIGNAL(debug(QString, QString)), this, SLOT(LogDebug(QString, QString)));
	connect(mMessageThread, SIGNAL(status(QString)), this, SLOT(StatusInfo(QString)));

	// message processing functions
	connect(mMessageThread, SIGNAL(onmsg(MessageUnionPtr)), this, SLOT(processMessage(MessageUnionPtr)));
	connect(mMessageThread, SIGNAL(onmsg(MessageUnionPtr)), rootObject, SLOT(processMessage(MessageUnionPtr)));

	mMessageThread->start();
}

void AnalyticsViewer::AppendToLog(const LogCategory category, const QString & message, const QString & details)
{
	LogEntry log;
	log.mCategory = category;
	log.mMessage = message;
	log.mDetails = details;
	mLogEntries.push_back(log);

	AddToTable(log);
}

void AnalyticsViewer::SetButtonTitleCount(QPushButton * button, int value)
{
	button->setProperty("numEntries", value);

	// update the label with the new count
	QString title = button->property("Title").toString();
	button->setText(title.arg(value));
}

void AnalyticsViewer::ModifyButtonTitleCount(QPushButton * button, int value)
{
	const int newCount = button->property("numEntries").toInt() + value;
	button->setProperty("numEntries", newCount);

	QString title = button->property("Title").toString();
	button->setText(title.arg(newCount));
}

void AnalyticsViewer::AddToTable(const LogEntry & log)
{
	QRegExp expression(ui.editOutputFilter->text(), Qt::CaseInsensitive, QRegExp::Wildcard);
	if (!expression.isEmpty())
	{
		if (expression.indexIn(log.mMessage) == -1 &&
			expression.indexIn(log.mDetails) == -1)
			return;
	}

	QTableWidgetItem * categoryItem = NULL;
	switch (log.mCategory)
	{
	case LOG_INFO:
		if (!ui.toggleMessages->isChecked())
			return;
		ModifyButtonTitleCount(ui.toggleMessages, 1);
		categoryItem = new QTableWidgetItem();
		categoryItem->setText("INFO");
		categoryItem->setForeground(QColor::fromRgb(127, 127, 127));
		break;
	case LOG_WARNING:
		if (!ui.toggleWarnings->isChecked())
			return;

		ModifyButtonTitleCount(ui.toggleWarnings, 1);
		categoryItem = new QTableWidgetItem();
		categoryItem->setText("WARN");
		categoryItem->setForeground(QColor::fromRgb(255, 127, 0));
		break;
	case LOG_ERROR:
		if (!ui.toggleErrors->isChecked())
			return;

		ModifyButtonTitleCount(ui.toggleErrors, 1);
		categoryItem = new QTableWidgetItem();
		categoryItem->setText("ERROR");
		categoryItem->setForeground(QColor::fromRgb(255, 0, 0));
		break;
	case LOG_DEBUG:
		if (!ui.toggleDebug->isChecked())
			return;

		ModifyButtonTitleCount(ui.toggleDebug, 1);
		categoryItem = new QTableWidgetItem();
		categoryItem->setText("DEBUG");
		categoryItem->setForeground(QColor::fromRgb(0, 0, 255));
		break;
	default:
		qDebug() << "Unknown Log Type";
		return;
	}

	QTableWidgetItem * messageItem = new QTableWidgetItem(log.mMessage);

	const int numRows = ui.tableOutput->rowCount();
	ui.tableOutput->insertRow(ui.tableOutput->rowCount());
	ui.tableOutput->setItem(numRows, 0, categoryItem);
	ui.tableOutput->setItem(numRows, 1, messageItem);

	// todo: make this conditional
	ui.tableOutput->scrollToBottom();

	if (!log.mDetails.isEmpty())
	{
		QTableWidgetItem * detailsItem = new QTableWidgetItem();
		detailsItem->setIcon(QIcon(QStringLiteral("Resources/svg/document98.svg")));
		detailsItem->setData(Qt::ToolTipRole, log.mDetails);
		ui.tableOutput->setItem(numRows, 2, detailsItem);
	}
}

void AnalyticsViewer::RebuildLogTable()
{
	ui.tableOutput->setUpdatesEnabled(false);
	while (ui.tableOutput->rowCount() > 0)
		ui.tableOutput->removeRow(0);

	SetButtonTitleCount(ui.toggleMessages, 0);
	SetButtonTitleCount(ui.toggleWarnings, 0);
	SetButtonTitleCount(ui.toggleErrors, 0);
	SetButtonTitleCount(ui.toggleDebug, 0);

	for (int i = 0; i < mLogEntries.size(); ++i)
		AddToTable(mLogEntries[i]);
	ui.tableOutput->setUpdatesEnabled(true);
}

void AnalyticsViewer::FileLoad(const QString & filePath)
{
	/*if ( ui.renderBg->importModelToScene( filePath, false ) )
		AppendToLog( LOG_INFO, tr( "Loading %1" ).arg( filePath ), QString() );
		else
		AppendToLog( LOG_INFO, tr( "Problem Loading %1" ).arg( filePath ), QString() );*/
}

void AnalyticsViewer::FileSave(const QString & filePath)
{
	/*if ( ui.renderBg->exportSceneToFile( filePath ) )
		AppendToLog( LOG_INFO, tr( "Saved %1" ).arg( filePath ), QString() );
		else
		AppendToLog( LOG_INFO, tr( "Problem Saving %1" ).arg( filePath ), QString() );*/
}

void AnalyticsViewer::FileOpen()
{
	QString openFileName = QFileDialog::getOpenFileName(this, tr("Open Model"), "", tr("Model Files (*.obj, *.osgb)"));

	if (!openFileName.isEmpty())
	{
		FileLoad(openFileName);
	}
}

void AnalyticsViewer::FileSave()
{
	QString saveFileName = QFileDialog::getSaveFileName(this, tr("Save Scene Graph"), "", tr("Model Files (*.*)"));

	if (!saveFileName.isEmpty())
	{
		FileSave(saveFileName);
	}
}

void AnalyticsViewer::AddObjectProperties_r(const QVariantMap& data, QtVariantProperty * parent)
{
	for (auto it = data.begin(); it != data.end(); ++it)
	{
		if (it.value().type() == QVariant::Type::Map)
		{
			QtVariantProperty * childProp = mVariantPropMgr->addProperty(QtVariantPropertyManager::groupTypeId(), it.key());
			ui.props->addProperty(childProp, parent);

			QVariantMap childMap = it.value().toMap();
			AddObjectProperties_r(childMap, childProp);
		}
		else
		{
			QtVariantProperty * childProp = mVariantPropMgr->addProperty(it.value().type(), it.key());
			if (childProp != nullptr)
			{
				childProp->setValue(it.value());
				childProp->setEnabled(false);
				ui.props->addProperty(childProp, parent);
			}
			else
			{
				qDebug() << QString("Property Type: %1 not handled by variant manager").arg(it.value().typeName());
			}
		}
		
	}
}

void AnalyticsViewer::AddObjectProperties_r(QObject * obj, QtVariantProperty * parent)
{
	for (int i = 0; i < obj->metaObject()->propertyCount(); i++)
	{
		QMetaProperty metaProperty = obj->metaObject()->property(i);

		if (!metaProperty.isReadable())
			continue;

		const char* objClassName = obj->metaObject()->className();
		const char* metaPropName = metaProperty.name();
		const char* metaTypeName = metaProperty.typeName();

		// prevent a crash due to uninitialized vars
		if (QString(metaPropName) == "activeInput")
			continue;

		QVariant value = obj->property(metaPropName);

		// Special handling for certain types
		if (value.canConvert(QMetaType::QObjectStar))
		{
			QtVariantProperty * childProp = mVariantPropMgr->addMetaProperty(QtVariantPropertyManager::groupTypeId(), obj, metaProperty);
			ui.props->addProperty(childProp, parent);

			if (QString("parent") == metaPropName)
				continue;

			QObject* childObj = qvariant_cast<QObject*>(value);
			if (childObj != NULL)
				AddObjectProperties_r(childObj, childProp);
			continue;
		}
		else if (obj->metaObject()->indexOfEnumerator(metaTypeName) >= 0)
		{
			QMetaEnum metaEnum = obj->metaObject()->enumerator(obj->metaObject()->indexOfEnumerator(metaTypeName));

			QStringList enumNames;
			for (int i = 0; i < metaEnum.keyCount(); ++i)
				enumNames.push_back(metaEnum.key(i));

			QtVariantProperty * childProp = mVariantPropMgr->addMetaProperty(QtVariantPropertyManager::enumTypeId(), obj, metaProperty);
			ui.props->addProperty(childProp, parent);

			mVariantPropMgr->setAttribute(childProp, "enumNames", QVariant::fromValue(enumNames));
			childProp->setValue(value.toInt());
			continue;
		}
		else if (value.canConvert(QMetaType::QVariantList))
		{
			QtVariantProperty * childProp = mVariantPropMgr->addProperty(QVariant::String, metaProperty.name());
			if (childProp != NULL)
			{
				QtBrowserItem * browse = ui.props->addProperty(childProp, parent);
				ui.props->setExpanded(browse, false);

				const QString objClassName = obj->metaObject()->className();
				const QString metaPropName = metaProperty.name();
				const QString metaTypeName = metaProperty.typeName();

				QVariantList varList = value.toList();

				// store user data to remember this metaproperty mapping
				childProp->setEnabled(false);
				childProp->setValue(QString("[%1]").arg(varList.size()));
				childProp->setToolTip(value.typeName());

				foreach(const QVariant& item, varList)
				{
					//item.canConvert<QString>()
				}
			}
			continue;
		}
		else if (QString(metaTypeName).startsWith("QQmlListProperty"))
		{
			//QtVariantProperty * childProp = mVariantPropMgr->addProperty( QVariant::String, metaPropName );
			//ui.props->addProperty( childProp, parent );
			//childProp->setEnabled( metaProperty.isWritable() );
			//childProp->setValue( value );
			//childProp->setToolTip( metaProperty.typeName() );

			//ui.props->addProperty( childProp, parent );
			//
			//// this works but recurses infinitely
			//QQmlListProperty<QObject> qmlList = qvariant_cast<QQmlListProperty<QObject>>( value );
			//
			//if ( qmlList.count != NULL && qmlList.at != NULL )
			//{
			//	for ( int i = 0; i < qmlList.count( &qmlList ); ++i )
			//	{
			//		QObject* childObj = qmlList.at( &qmlList, i );
			//		if ( childObj != NULL )
			//			AddObjectProperties_r( childObj, childProp, false );
			//	}
			//}
			continue;
		}

		// convert to usable types
		if (metaProperty.type() == QMetaType::Float)
			value = value.toDouble();
		else if (metaProperty.type() == QMetaType::QUrl)
			value = value.toString();

		QtVariantProperty * prop = mVariantPropMgr->addMetaProperty(value.type(), obj, metaProperty);
		if (prop != NULL)
		{
			QtBrowserItem * browse = ui.props->addProperty(prop, parent);
			ui.props->setExpanded(browse, false);
		}
		else
		{
			qDebug() << QString("Property Type: %1 not handled by property manager").arg(value.typeName());
		}
	}
}

void AnalyticsViewer::TreeSelectionChanged()
{
	ui.props->clear();

	auto selected = ui.treeHierarchy->selectedItems();
	for (int i = 0; i < selected.size(); ++i)
	{
		QTreeWidgetItem* sel = selected[i];
		QObject * obj = (QObject *)sel->data(0, Qt::UserRole).toULongLong();

		AddObjectProperties_r(obj, NULL);
		return;
	}
}

void AnalyticsViewer::PropertyChanged(QtProperty *property, const QVariant & propertyValue)
{
	const QString propertyName = property->propertyName();

	QVariant obj = property->getUserData("Object");
	QVariant prop = property->getUserData("MetaProperty");

	if (obj.isValid() && prop.isValid())
	{
		QObject * o = qvariant_cast<QObject*>(obj);
		QMetaProperty metaProperty = o->metaObject()->property(prop.toInt());

		bool writable = metaProperty.isWritable();
		if (writable)
		{
			const QVariant uneditableProperties = o->property("uneditableProperties");
			QVariantList propList = uneditableProperties.toList();
			foreach(const QVariant& item, propList)
			{
				if (propertyName == item.toString())
				{
					writable = false;
					break;
				}
			}
		}

		if (writable)
		{
			const QString metaClassName = o->metaObject()->className();
			const char* metaPropName = metaProperty.name();

			// the property UI changed, so let's push the data to the object			
			o->setProperty(metaPropName, propertyValue);

			//LogDebug( QString( "MetaProperty: %1 set to %2" ).arg( propertyName ).arg( propertyValue.toString() ), QString() );
		}
	}
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

QTreeWidgetItem * AnalyticsViewer::FindChildItem(QTreeWidgetItem * searchUnder, QObject * obj)
{
	QTreeWidgetItem * nodeItem = NULL;

	for (int c = 0; c < searchUnder->childCount(); ++c)
	{
		QTreeWidgetItem * child = searchUnder->child(c);
		if (child->data(0, Qt::UserRole).toULongLong() == (qulonglong)obj)
		{
			nodeItem = child;
			break;
		}
	}

	return nodeItem;
}

QTreeWidgetItem * AnalyticsViewer::FindOrAdd(QTreeWidgetItem * parent, const QString& name, QObject * obj)
{
	QTreeWidgetItem * item = FindChildItem(parent, obj);
	if (item == NULL)
	{
		item = new QTreeWidgetItem(parent);
		item->setIcon(0, QIcon(QStringLiteral("Resources/svg/family3.svg")));
		item->setData(0, Qt::UserRole, (qulonglong)obj);
		item->setData(0, Qt::DisplayRole, name);
		item->setData(1, Qt::DisplayRole, obj->metaObject()->className());
		item->setExpanded(false);
		parent->addChild(item);
	}
	return item;
}

void AnalyticsViewer::WalkHierarchy(Qt3DCore::QEntity* entity, QTreeWidgetItem * treeItem)
{
	if (entity == NULL)
		return;

	QString itemName = entity->objectName();
	if (itemName.isEmpty())
		itemName = "Entity";

	QTreeWidgetItem * entityItem = FindOrAdd(treeItem, itemName, entity);

	Qt3DCore::QComponentVector components = entity->components();
	for (int i = 0; i < components.size(); ++i)
	{
		Qt3DCore::QComponent* comp = components[i];

		QString cmpName = comp->objectName();
		if (cmpName.isEmpty())
			cmpName = "Component";

		QTreeWidgetItem * compItem = FindOrAdd(entityItem, cmpName, comp);
	}

	const QObjectList & ch = entity->children();
	for (int i = 0; i < ch.size(); ++i)
	{
		Qt3DCore::QEntity* ent = qobject_cast<Qt3DCore::QEntity*>(ch[i]);
		if (ent != NULL)
		{
			WalkHierarchy(ent, entityItem);
		}
	}
}

void AnalyticsViewer::UpdateHierarchy()
{	
	WalkHierarchy(mView->engine()->aspectEngine()->rootEntity().data(), ui.treeHierarchy->invisibleRootItem());
}

void AnalyticsViewer::LogInfo(const QString & msg, const QString & details)
{
	AppendToLog(LOG_INFO, msg, details);
}
void AnalyticsViewer::LogWarn(const QString & msg, const QString & details)
{
	AppendToLog(LOG_WARNING, msg, details);
}
void AnalyticsViewer::LogError(const QString & msg, const QString & details)
{
	AppendToLog(LOG_ERROR, msg, details);
}
void AnalyticsViewer::LogDebug(const QString & msg, const QString & details)
{
	AppendToLog(LOG_DEBUG, msg, details);
}
void AnalyticsViewer::StatusInfo(const QString & msg)
{
	mNetworkLabel->setText(msg);
}

QVector3D Convert(const modeldata::Vec3 & vec)
{
	return QVector3D(vec.x(), vec.y(), vec.z());
}

void AnalyticsViewer::processMessage(MessageUnionPtr msg)
{
	switch (msg->msg_case())
	{
	case Analytics::MessageUnion::kGameInfo:
		LogWarn(QString("Game Info %1").arg(msg->gameinfo().mapname().c_str()), QString());
		break;
	case Analytics::MessageUnion::kGameNavNotFound:
		LogWarn(QString("Navigation Not found for %1").arg(msg->gamenavnotfound().mapname().c_str()), QString());
		break;
	case Analytics::MessageUnion::kSystemNavDownloaded:
		LogInfo(QString("Navigation Downloaded for %1").arg(msg->systemnavdownloaded().mapname().c_str()), QString());
		break;
	case Analytics::MessageUnion::kGameAssert:
		LogWarn(QString("Assert %1: %2(%3)")
			.arg(msg->gameassert().condition().c_str())
			.arg(msg->gameassert().file().c_str())
			.arg(msg->gameassert().line()), QString());
		break;
	case Analytics::MessageUnion::kGameCrash:
		LogError(QString("Crash %1")
			.arg(msg->gamecrash().info().c_str()), QString());
		break;
	case Analytics::MessageUnion::kGameModelData:
		// handled elsewhere
		break;
	case Analytics::MessageUnion::kGameEntityList:
		// handled elsewhere
		break;
	case Analytics::MessageUnion::kGameWeaponFired:
		// todo:
		break;
	case Analytics::MessageUnion::kGameDeath:
		// todo:
		break;
	case Analytics::MessageUnion::kGameTookDamage:
		// todo:
		break;
	case Analytics::MessageUnion::kGameNavigationStuck:
		// todo:
		break;
	case Analytics::MessageUnion::kTopicSubscribe:
		LogInfo(QString("Topic Subscribed %1").arg(msg->topicsubscribe().topic().c_str()), QString());
		break;
	case Analytics::MessageUnion::kTopicUnsubscribe:
		LogInfo(QString("Topic Unsubscribed %1").arg(msg->topicunsubscribe().topic().c_str()), QString());
		break;
	case Analytics::MessageUnion::MSG_NOT_SET:
		LogError(QString("Message case not set"), QString());
		break;
	}

	EventInfo& info = mEventMap[msg->msg_case()];
	if (info.mItemCount)
	{
		const int newCount = info.mItemCount->data(Qt::UserRole).toInt() + 1;
		info.mItemCount->setData(Qt::UserRole, QVariant(newCount));
		info.mItemCount->setText(QString("%1").arg(newCount));

		auto item = mTimeline.AddTick(info.mRow, msg->timestamp());
		if (item != nullptr)
		{
			QVariantMap eventData = VariantMapFromMessage(*msg);
			item->setData(0, eventData);
		}
	}
}

void AnalyticsViewer::TimelineSelectionChanged()
{
	ui.props->clear();

	auto selected = mTimeline.selectedItems();
	for (int i = 0; i < selected.size(); ++i)
	{
		QGraphicsItem* sel = selected[i];
		auto val = sel->data(0);
		if (!val.isNull() && val.type() == QVariant::Type::Map)
		{
			AddObjectProperties_r(val.toMap(), NULL);
		}
		return;
	}
}

QVariantMap VariantMapFromMessage(const google::protobuf::Message & msg)
{
	QVariantMap data;

	std::vector<const google::protobuf::FieldDescriptor*> descriptors;
	msg.GetReflection()->ListFields(msg, &descriptors);
	for (int f = 0; f < descriptors.size(); ++f)
	{
		QVariant val = VariantFromField(msg, descriptors[f]);
		if (!val.isNull())
		{
			data.insert(QString::fromStdString(descriptors[f]->name()), val);
		}
	}

	return data;
}

QVariant VariantFromField(const google::protobuf::Message & msg, const google::protobuf::FieldDescriptor* fdesc)
{
	if (fdesc->is_repeated())
		return false;

	switch (fdesc->type())
	{
	case google::protobuf::FieldDescriptor::TYPE_DOUBLE:
	{
		const double val = msg.GetReflection()->GetBool(msg, fdesc);
		return QVariant(val);
	}
	case google::protobuf::FieldDescriptor::TYPE_FLOAT:
	{
		const float val = msg.GetReflection()->GetFloat(msg, fdesc);
		return QVariant(val);
	}
	case google::protobuf::FieldDescriptor::TYPE_INT64:
	case google::protobuf::FieldDescriptor::TYPE_FIXED64:
	case google::protobuf::FieldDescriptor::TYPE_SINT64:
	case google::protobuf::FieldDescriptor::TYPE_SFIXED64:
	{
		const google::protobuf::int64 val = msg.GetReflection()->GetInt64(msg, fdesc);
		return QVariant(val);
	}
	case google::protobuf::FieldDescriptor::TYPE_UINT64:
	{
		const google::protobuf::uint64 val = msg.GetReflection()->GetUInt64(msg, fdesc);
		return QVariant(val);
	}
	case google::protobuf::FieldDescriptor::TYPE_SINT32:
	case google::protobuf::FieldDescriptor::TYPE_FIXED32:
	case google::protobuf::FieldDescriptor::TYPE_SFIXED32:
	case google::protobuf::FieldDescriptor::TYPE_INT32:
	{
		const google::protobuf::int32 val = msg.GetReflection()->GetInt32(msg, fdesc);
		return QVariant(val);
	}	
	case google::protobuf::FieldDescriptor::TYPE_UINT32:
	{
		const google::protobuf::uint32 val = msg.GetReflection()->GetUInt32(msg, fdesc);
		return QVariant(val);
	}
	case google::protobuf::FieldDescriptor::TYPE_BOOL:
	{
		const bool val = msg.GetReflection()->GetBool(msg, fdesc);
		return QVariant(val);
	}
	case google::protobuf::FieldDescriptor::TYPE_STRING:
	{
		auto val = msg.GetReflection()->GetString(msg, fdesc);
		return QVariant(QString::fromStdString(val));
	}
	case google::protobuf::FieldDescriptor::TYPE_ENUM:
	{
		const google::protobuf::EnumValueDescriptor* eval = msg.GetReflection()->GetEnum(msg, fdesc);
		if (eval == NULL)
			return false;

		auto val = eval->name();
		return QVariant(QString::fromStdString(val));
	}
	case google::protobuf::FieldDescriptor::TYPE_MESSAGE:
	{
		const google::protobuf::Message& childmsg = msg.GetReflection()->GetMessage(msg, fdesc);

		return QVariant(VariantMapFromMessage(childmsg));
	}
	default:
	{
		return false;
	}
	}
	return true;
}

