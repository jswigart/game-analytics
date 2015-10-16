#include "AnalyticsScene.h"


AnalyticsScene::AnalyticsScene( Qt3D::QNode *parent )
{
	setObjectName( "Scene" );
}

AnalyticsScene::~AnalyticsScene()
{

}

void AnalyticsScene::AddToScene( Qt3D::QEntity* entity )
{
	entity->moveToThread( thread() );
	entity->setParent( this );
}
