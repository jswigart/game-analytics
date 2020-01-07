#ifndef ANALYTICSVIEWER_H
#define ANALYTICSVIEWER_H

#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMdiArea>
#include <Qt3DQuick/QQmlAspectEngine>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QLabel>
#include <Qt3dRender/QGeometry>
#include <Qt3DQuick/QQmlAspectEngine>
#include "timelinegraphicsscene.h"
#include "ui_analyticsviewer.h"

#include "Messaging.h"

class RenderWindow;
class HostThreadENET;

class QtVariantEditorFactory;
class QtVariantProperty;
class QtVariantPropertyManager;

class AnalyticsViewer : public QMainWindow
{
	Q_OBJECT

public:
	AnalyticsViewer(QWidget *parent = 0);
	~AnalyticsViewer();

	enum LogCategory
	{
		LOG_INFO,
		LOG_WARNING,
		LOG_ERROR,
		LOG_DEBUG,
	};

	struct LogEntry
	{
		LogCategory	mCategory;
		QString		mMessage;
		QString		mDetails;
	};

	QVector<LogEntry>	mLogEntries;

	void AppendToLog(const LogCategory category, const QString & message, const QString & details);

	//	Q_SIGNALS:
	//	void logEntryAppended( const LogEntry & entry );
	//	void logFiltersChanged( const LogEntry & entry );
	//

	protected Q_SLOTS:
	void FileOpen();
	void FileSave();
	void RebuildLogTable();

	void TreeSelectionChanged();
	void TimelineSelectionChanged();
	void PropertyChanged(QtProperty *property, const QVariant & propertyValue);
	void LogInfo(const QString & msg, const QString & details);
	void LogWarn(const QString & msg, const QString & details);
	void LogError(const QString & msg, const QString & details);
	void LogDebug(const QString & msg, const QString & details);
	void StatusInfo(const QString & msg);
	void UpdateHierarchy();

	void sceneCreated(QObject* rootObject);

	// message handling
	void processMessage(MessageUnionPtr msg);
private:
	Ui::AnalyticsViewerClass			ui;
	RenderWindow*						mView;
	HostThread0MQ*						mMessageThread;
	QLabel*								mNetworkLabel;

	QtVariantEditorFactory*				mVariantEditor;
	QtVariantPropertyManager*			mVariantPropMgr;

	TimelineGraphicsScene				mTimeline;

	struct EventInfo
	{
		QTableWidgetItem*		mItemMessage;
		QTableWidgetItem*		mItemCount;
		int						mRow;

		EventInfo()
			: mItemMessage(NULL)
			, mItemCount(NULL)
			, mRow(0)
		{
		}
	};
	typedef QMap<int, EventInfo> EventMap;
	EventMap							mEventMap;

	void FileLoad(const QString & filePath);
	void FileSave(const QString & filePath);

	void AddToTable(const LogEntry & log);

	void WalkHierarchy(Qt3DCore::QEntity* entity, QTreeWidgetItem * treeItem);

	void AddObjectProperties_r(QObject * obj, QtVariantProperty * parent);
	void AddObjectProperties_r(const QVariantMap& data, QtVariantProperty * parent);

	QTreeWidgetItem * FindChildItem(QTreeWidgetItem * searchUnder, QObject * obj);
	QTreeWidgetItem * FindOrAdd(QTreeWidgetItem * parent, const QString& name, QObject * obj);

	void SetButtonTitleCount(QPushButton * button, int value);
	void ModifyButtonTitleCount(QPushButton * button, int value);
};

#endif
