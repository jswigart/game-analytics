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

		//Qt3D::QComponentList cmps = foundEntity->components();
		//for ( int i = 0; i < cmps.size(); ++i )
		//{
		//	Qt3D::QTransform* xform = qobject_cast<Qt3D::QTransform*>( cmps[ i ] );
		//	if ( xform )
		//	{
		//		connect( xform, SIGNAL( matrixChanged() ), this, SLOT( EntityTransformChanged() ) );

		//		QList<Qt3D::QAbstractTransform *> xforms = xform->transforms();
		//		for ( int t = 0; t < xforms.size(); ++t )
		//		{
		//			//connect( xforms[ t ], SIGNAL( transformMatrixChanged() ), this, SLOT( EntityTransformChanged() ) );
		//		}
		//	}
		//}
	}

	if ( foundEntity == NULL )
		return;

	QVector3D add( rand() % 100, rand() % 100, rand() % 100 );
	
	// propagate the message data into our entity
	foundEntity->setProperty( "positionX", msg.positionx() + add.x() );
	foundEntity->setProperty( "positionY", msg.positiony() + add.y() );
	foundEntity->setProperty( "positionZ", msg.positionz() + add.z() );

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
		for ( int i = 0; i < 1; /*elist.entities_size();*/ ++i )
		{
			processMessage( elist.entities( i ) );
		}
	}
}
