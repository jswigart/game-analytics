#pragma once

#include <QThread>
#include <QMutex>
#include <QSharedPointer>
#include "Messaging.h"

#include "analytics.pb.h"
#include "modeldata.pb.h"

#include <osg/Node>
#include <osg/MatrixTransform>
#include <osgDB/WriteFile>

class ModelProcessor : public QThread
{
	Q_OBJECT
public:
	ModelProcessor( QObject *parent = 0 );

	typedef QList<MessageUnionPtr> MessageQueue;

	typedef osg::ref_ptr<osg::Geometry> GeometryPtr;
	typedef std::map<std::string,GeometryPtr> GeomMap;

	void run();

	bool mRunning;
signals:
	void info( const QString &info, const QString &details );
	void warn( const QString &info, const QString &details );
	void error( const QString &info, const QString &details );
	void debug( const QString &info, const QString &details );
	// message handling functions
	void allocModel( const QString& filename );
public slots:
	void processMessage( MessageUnionPtr msg );
private:
	void processMessage( const Analytics::SystemModelData& msg );
	void processNode( const modeldata::Scene& scene, const modeldata::Node& node, osg::MatrixTransform* grp );

	MessageQueue	mMessageQueue;
	QMutex			mMutex;

	GeomMap			mGeomCache;
};

