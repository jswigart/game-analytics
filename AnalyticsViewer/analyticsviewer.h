#ifndef ANALYTICSVIEWER_H
#define ANALYTICSVIEWER_H

#include <QtWidgets/QMainWindow>
#include "ui_analyticsviewer.h"

#include "Messaging.h"

class ModelProcessor;
class HostThreadENET;

class AnalyticsViewer : public QMainWindow
{
	Q_OBJECT

public:
	AnalyticsViewer( QWidget *parent = 0 );
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
	
	void AppendToLog( const LogCategory category, const QString & message, const QString & details );

	//	Q_SIGNALS:
	//	void logEntryAppended( const LogEntry & entry );
	//	void logFiltersChanged( const LogEntry & entry );
	//
	protected Q_SLOTS:
	void FileOpen();
	void FileSave();
	void RebuildLogTable();
	void UpdateHierarchy();
	void SelectionChanged();
	void LogInfo( const QString & msg, const QString & details );
	void LogWarn( const QString & msg, const QString & details );
	void LogError( const QString & msg, const QString & details );
	void LogDebug( const QString & msg, const QString & details );
	void StatusInfo( const QString & msg );

	// message handling
	void processMessage( MessageUnionPtr msg );

private:
	Ui::AnalyticsViewerClass ui;

	ModelProcessor*		mModelProcessorThread;
	HostThread0MQ*		mMessageThread;
	QLabel*				mNetworkLabel;

	void FileLoad( const QString & filePath );
	void FileSave( const QString & filePath );

	void AddToTable( const LogEntry & log );

	void processMessage( const Analytics::GameEntityInfo& msg );
};

#endif
