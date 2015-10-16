#pragma once

#include <QThread>
#include <QMutex>
#include <QMutexLocker>
#include <QSharedPointer>
#include <Qt3DCore/QEntity>
#include <Qt3DRenderer/QMeshDataPtr>
#include <Qt3DRenderer/QAbstractMesh>
#include <Qt3DRenderer/QAbstractMeshFunctor>
#include "Messaging.h"

#include "analytics.pb.h"
#include "modeldata.pb.h"

class QAnalyticModel : public Qt3D::QAbstractMesh
{
Q_OBJECT
public:
	explicit QAnalyticModel( QNode *parent = 0, Qt3D::QMeshDataPtr data = Qt3D::QMeshDataPtr() );
	
	Qt3D::QAbstractMeshFunctorPtr meshFunctor() const Q_DECL_OVERRIDE;

private:
	Qt3D::QMeshDataPtr		m_data;
private:
	QT3D_CLONEABLE( QAnalyticModel )
};

class ModelProcessor : public QThread
{
	Q_OBJECT
public:
	ModelProcessor( QObject *parent = 0 );

	typedef QList<MessageUnionPtr> MessageQueue;

	typedef std::map<std::string, Qt3D::QMeshDataPtr> GeomMap;

	void run();

	bool mRunning;
signals:
	void info( const QString &info, const QString &details );
	void warn( const QString &info, const QString &details );
	void error( const QString &info, const QString &details );
	void debug( const QString &info, const QString &details );
	// message handling functions
	void allocModel( Qt3D::QEntity* entity );
public slots:
	void processMessage( MessageUnionPtr msg );
private:
	void processMessage( const Analytics::SystemModelData& msg );
	void processNode( const modeldata::Scene& scene, const modeldata::Node& node, Qt3D::QEntity* entity );
	
	MessageQueue	mMessageQueue;
	QMutex			mMutex;

	GeomMap			mGeomCache;
};

