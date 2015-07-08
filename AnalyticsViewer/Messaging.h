#pragma once

#include <QThread>
#include <QSharedPointer>

#include "analytics.pb.h"

typedef QSharedPointer<Analytics::MessageUnion> MessageUnionPtr;

Q_DECLARE_METATYPE( MessageUnionPtr );

class HostThreadENET : public QThread
{
	Q_OBJECT
public:
	HostThreadENET( QObject *parent = 0 );

	enum NetChannels
	{
		CHANNEL_EVENT_STREAM,
		CHANNEL_DATA_STREAM,
		CHANNEL_COUNT,
	};

	void run();

	bool	mRunning;
signals:
	void info( const QString &info, const QString &details );
	void warn( const QString &info, const QString &details );
	void error( const QString &info, const QString &details );
	void debug( const QString &info, const QString &details );
	// message handling functions
	void onmsg( MessageUnionPtr msg );
	void status( const QString & msg );
};

class HostThread0MQ : public QThread
{
	Q_OBJECT
public:
	HostThread0MQ( QObject *parent = 0 );

	void run();

	bool	mRunning;
signals:
	void info( const QString &info, const QString &details );
	void warn( const QString &info, const QString &details );
	void error( const QString &info, const QString &details );
	void debug( const QString &info, const QString &details );
	// message handling functions
	void onmsg( MessageUnionPtr msg );
	void status( const QString & msg );
};

