#pragma once

#include <Qt3DCore/QEntity>

#include "Messaging.h"

class AnalyticsScene : public Qt3D::QEntity
{
	Q_OBJECT
public:
	explicit AnalyticsScene( Qt3D::QNode *parent = 0 );
	~AnalyticsScene();
private Q_SLOTS:
	void AddToScene( Qt3D::QEntity* entity );
	
	// message handling
	void processMessage( MessageUnionPtr msg );
private:
	void processMessage( const Analytics::GameEntityInfo& msg );
};

