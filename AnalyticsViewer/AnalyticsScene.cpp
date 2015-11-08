#include <QtQml/QQmlComponent>
#include <Qt3DCore/QTransform>
#include <Qt3DCore/QMatrixTransform>
#include <Qt3DRenderer/QCylinderMesh>
#include <Qt3DRenderer/QPhongMaterial>

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

	Qt3D::QEntity* oldChild = findChild<Qt3D::QEntity*>( entity->objectName() );
	if ( oldChild != NULL )
	{
		oldChild->setParent( (Qt3D::QNode*)NULL );
		oldChild->deleteLater();

		// put the new components on the old entity
		//oldChild->removeAllComponents();

		//Qt3D::QComponentList components = entity->components();
		//foreach( Qt3D::QComponent* cmp, components )
		//{
		//	cmp->setParent( NULL );
		//	oldChild->addComponent( cmp );
		//}

		//// delete the ne
		//entity->deleteLater();
	}

	entity->setParent( this );
}

void AnalyticsScene::processMessage( const Analytics::GameEntityInfo & msg )
{
	Qt3D::QEntity* foundEntity = NULL;

	for ( int i = 0; i < children().size(); ++i )
	{
		Qt3D::QEntity* entity = qobject_cast<Qt3D::QEntity*>( children()[ i ] );
		if ( entity && entity->property( "entityId" ).toInt() == msg.entityid() )
		{
			foundEntity = entity;
			break;
		}
	}

	// todo: cache this
	QVariant entityProperty = property( "entityComponent" );
	QQmlComponent* qml = qvariant_cast<QQmlComponent*>( entityProperty );
	
	if ( foundEntity == NULL && qml )
	{
		bool loading = qml->isLoading();
		QQmlComponent::Status status = qml->status();

		QString errors;
		if ( status == QQmlComponent::Error )
			errors = qml->errorString();

		if ( !qml->isReady() )
			return;
		
		foundEntity = qobject_cast<Qt3D::QEntity*>( qml->create( qml->creationContext() ) );
		foundEntity->setProperty( "entityId", QVariant( msg.entityid() ) );
		foundEntity->setParent( this );
	}

	if ( foundEntity == NULL )
		return;

	// propagate the message data into our entity
	foundEntity->setObjectName( msg.name().c_str() );
	foundEntity->setProperty( "positionX", msg.positionx() );
	foundEntity->setProperty( "positionY", msg.positiony() );
	foundEntity->setProperty( "positionZ", msg.positionz() );
	foundEntity->setProperty( "heading", msg.heading() );
	foundEntity->setProperty( "pitch", msg.pitch() );
	foundEntity->setProperty( "roll", msg.roll() );	
	foundEntity->setProperty( "groupId", msg.groupid() );
	foundEntity->setProperty( "classId", msg.classid() );
	foundEntity->setProperty( "health", msg.health() );
	foundEntity->setProperty( "healthMax", msg.healthmax() );
	foundEntity->setProperty( "armor", msg.health() );
	foundEntity->setProperty( "healthMax", msg.healthmax() );

	QVariantList ammo;
	for ( int i = 0; i < msg.ammo_size(); ++i )
		ammo.push_back( QVariant::fromValue( QPoint( msg.ammo( i ).ammotype(), msg.ammo( i ).ammocount() ) ) );

	if ( !ammo.isEmpty() )
		foundEntity->setProperty( "ammo", ammo );	
}

void AnalyticsScene::processMessage( MessageUnionPtr msg )
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
