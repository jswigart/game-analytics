#pragma once

#include <Qt3DCore/QEntity>
#include <Qt3DRender/QGeometry>

#include "Messaging.h"

#include "analytics.pb.h"
#include "modeldata.pb.h"

class AnalyticsScene : public Qt3DCore::QEntity
{
	Q_OBJECT
public:
	typedef std::map<std::string, Qt3DRender::QGeometry*> GeomMap;
	
	explicit AnalyticsScene( Qt3DCore::QNode *parent = 0 );
	~AnalyticsScene();
 Q_SIGNALS:
	void info( const QString &info, const QString &details );
	void warn( const QString &info, const QString &details );
	void error( const QString &info, const QString &details );
	void debug( const QString &info, const QString &details );
private Q_SLOTS:
	void AddToScene( Qt3DCore::QEntity* entity );
	
	// message handling
	void processMessage( MessageUnionPtr msg );
private:
	void processMessage( const Analytics::GameEntityInfo& msg );
	void processMessage( const Analytics::GameModelData& msg );
	void processNode( const modeldata::Scene& scene, const modeldata::Node& node, Qt3DCore::QEntity* entity );
	
	GeomMap			mGeomCache;
};

