#include <QtWidgets/QApplication>
#include <QtQml/QQmlComponent>
#include <Qt3DCore/QTransform>
#include <Qt3DCore/QTransform>
#include <Qt3DRender/QAttribute>
#include <Qt3DRender/QBuffer>
#include <Qt3DExtras/QPhongMaterial>
#include <Qt3DExtras/QPerVertexColorMaterial>
#include <Qt3DRender/QGeometryRenderer>
#include <Qt3DRender/QObjectPicker>
#include <Qt3DRender/QPickingSettings>
#include <Qt3DRender/QRenderSettings>

#include "fastlz.h"
#include "AnalyticsScene.h"

AnalyticsScene::AnalyticsScene(Qt3DCore::QNode *parent)
{
	setObjectName("Scene");
}

AnalyticsScene::~AnalyticsScene()
{
}

void AnalyticsScene::processMessage(const Analytics::GameEntityInfo & msg)
{
	Qt3DCore::QEntity* foundEntity = NULL;

	for (int i = 0; i < children().size(); ++i)
	{
		Qt3DCore::QEntity* entity = qobject_cast<Qt3DCore::QEntity*>(children()[i]);
		if (entity && entity->property("entityId").toInt() == msg.entityid())
		{
			foundEntity = entity;
			break;
		}
	}

	// todo: cache this
	QVariant entityProperty = property("entityComponent");
	QQmlComponent* qml = qvariant_cast<QQmlComponent*>(entityProperty);

	if (foundEntity == NULL && qml)
	{
		bool loading = qml->isLoading();
		QQmlComponent::Status status = qml->status();

		QString errors;
		if (status == QQmlComponent::Error)
			errors = qml->errorString();

		if (!qml->isReady())
			return;

		foundEntity = qobject_cast<Qt3DCore::QEntity*>(qml->create(qml->creationContext()));
		foundEntity->setProperty("entityId", QVariant(msg.entityid()));
		foundEntity->setParent(this);
	}

	if (foundEntity == NULL)
		return;

	// propagate the message data into our entity
	foundEntity->setObjectName(msg.name().c_str());
	foundEntity->setProperty("positionX", msg.positionx());
	foundEntity->setProperty("positionY", msg.positiony());
	foundEntity->setProperty("positionZ", msg.positionz());
	foundEntity->setProperty("heading", msg.heading());
	foundEntity->setProperty("pitch", msg.pitch());
	foundEntity->setProperty("roll", msg.roll());
	foundEntity->setProperty("groupId", msg.groupid());
	foundEntity->setProperty("classId", msg.classid());
	foundEntity->setProperty("health", msg.health());
	foundEntity->setProperty("healthMax", msg.healthmax());
	foundEntity->setProperty("armor", msg.health());
	foundEntity->setProperty("healthMax", msg.healthmax());

	QVariantList ammo;
	for (int i = 0; i < msg.ammo_size(); ++i)
		ammo.push_back(QVariant::fromValue(QPoint(msg.ammo(i).ammotype(), msg.ammo(i).ammocount())));

	if (!ammo.isEmpty())
		foundEntity->setProperty("ammo", ammo);
}

void AnalyticsScene::processMessage(MessageUnionPtr msg)
{
	if (msg->has_gameentitylist())
	{
		/*const Analytics::GameEntityList& elist = msg->gameentitylist();
		for ( int i = 0; i < elist.entities_size(); ++i )
		{
		processMessage( elist.entities( i ) );
		}*/
	}
	else if (msg->has_gamemodeldata())
	{
		processMessage(msg->gamemodeldata());
	}
}

void AnalyticsScene::processMessage(const Analytics::GameModelData& msg)
{
	try
	{
		const std::string* dataStr = &msg.modelbytes();

		QString details;

		std::string uncompressedData;
		switch (msg.compressiontype())
		{
		case Analytics::Compression_FastLZ:
		{
			uncompressedData.resize(msg.modelbytesuncompressed() * 2);
			uncompressedData.resize(fastlz_decompress(msg.modelbytes().c_str(), msg.modelbytes().size(), &uncompressedData[0], uncompressedData.size()));
			dataStr = &uncompressedData;

			details = QString("Decompressed FastLZ: %1 bytes to %2 bytes (%3% compression)")
				.arg(msg.modelbytes().size())
				.arg(uncompressedData.size())
				.arg((float)msg.modelbytes().size() / (float)uncompressedData.size() * 100.0);
			break;
		}
		case Analytics::Compression_None:
		{
			// decode the string as-is
			break;
		}
		}

		modeldata::Scene scene;
		if (scene.ParseFromString(*dataStr) && scene.IsInitialized())
		{
			for (int m = 0; m < scene.meshes_size(); ++m)
			{
				const modeldata::Mesh& mesh = scene.meshes(m);

				/*if ( !mesh.has_vertexcolors() )
				continue;*/

				Qt3DRender::QGeometry*& cachedGeom = mGeomCache[mesh.name()];
				if (cachedGeom == NULL)
					cachedGeom = new Qt3DRender::QGeometry();

				const QVector3D * vertices = reinterpret_cast<const QVector3D*>(&mesh.vertices()[0]);
				const size_t numVertices = mesh.vertices().size() / sizeof(QVector3D);

				QByteArray posBytes;
				posBytes.resize(numVertices * sizeof(QVector3D));
				memcpy(posBytes.data(), vertices, mesh.vertices().size());

				// Normal Buffer
				QByteArray normalBytes;
				normalBytes.resize(numVertices * sizeof(QVector3D));
				QVector3D * normalPtr = (QVector3D *)normalBytes.data();
				for (size_t v = 0; v < numVertices; ++v)
				{
					const QVector3D * baseVert = &vertices[v / 3 * 3];
					normalPtr[v] = QVector3D::normal(baseVert[0], baseVert[1], baseVert[2]);
				}

				// Color Buffer				
				const QColor dColor("#606060");

				QByteArray clrBytes;
				clrBytes.resize(numVertices * sizeof(QVector4D));
				//memcpy( clrBytes.data(), mesh.vertices().c_str(), mesh.vertices().size() );
				QVector4D * clrPtr = (QVector4D *)clrBytes.data();

				// initialize to default color
				for (int i = 0; i < numVertices; ++i)
					clrPtr[i] = QVector4D(dColor.redF(), dColor.greenF(), dColor.blueF(), dColor.alphaF());

				// optionally use the input color data
				if (mesh.has_vertexcolors())
				{
					// incoming colors are packed smaller than floats, so we need to expand them
					const QRgb * vertexColors = reinterpret_cast<const QRgb*>(&mesh.vertexcolors()[0]);
					const size_t numColors = mesh.vertexcolors().size() / sizeof(QRgb);
					if (numColors == numVertices)
					{
						for (int i = 0; i < numColors; ++i)
						{
							QColor color = QColor::fromRgba(vertexColors[i]);
							clrPtr[i] = QVector4D(color.redF(), color.greenF(), color.blueF(), color.alphaF());
						}
					}
				}

				// Qt 5.6
				Qt3DRender::QBuffer *vertexDataBuffer = new Qt3DRender::QBuffer(Qt3DRender::QBuffer::VertexBuffer, cachedGeom);
				Qt3DRender::QBuffer *normalDataBuffer = new Qt3DRender::QBuffer(Qt3DRender::QBuffer::VertexBuffer, cachedGeom);
				Qt3DRender::QBuffer *colorDataBuffer = new Qt3DRender::QBuffer(Qt3DRender::QBuffer::VertexBuffer, cachedGeom);
				//Qt3DCore::QBuffer *indexDataBuffer = new Qt3DCore::QBuffer( Qt3DCore::QBuffer::IndexBuffer, cachedGeom );

				vertexDataBuffer->setData(posBytes);
				normalDataBuffer->setData(normalBytes);
				colorDataBuffer->setData(clrBytes);

				// Attributes
				Qt3DRender::QAttribute *positionAttribute = new Qt3DRender::QAttribute();
				positionAttribute->setAttributeType(Qt3DRender::QAttribute::VertexAttribute);
				positionAttribute->setBuffer(vertexDataBuffer);
				positionAttribute->setDataType(Qt3DRender::QAttribute::Float);
				positionAttribute->setDataSize(3);
				positionAttribute->setByteOffset(0);
				positionAttribute->setByteStride(3 * sizeof(float));
				positionAttribute->setCount(numVertices);
				positionAttribute->setName(Qt3DRender::QAttribute::defaultPositionAttributeName());

				Qt3DRender::QAttribute *normalAttribute = new Qt3DRender::QAttribute();
				normalAttribute->setAttributeType(Qt3DRender::QAttribute::VertexAttribute);
				normalAttribute->setBuffer(normalDataBuffer);
				normalAttribute->setDataType(Qt3DRender::QAttribute::Float);
				normalAttribute->setDataSize(3);
				normalAttribute->setByteOffset(0);
				normalAttribute->setByteStride(3 * sizeof(float));
				normalAttribute->setCount(numVertices);
				normalAttribute->setName(Qt3DRender::QAttribute::defaultNormalAttributeName());

				Qt3DRender::QAttribute *colorAttribute = new Qt3DRender::QAttribute();
				colorAttribute->setAttributeType(Qt3DRender::QAttribute::VertexAttribute);
				colorAttribute->setBuffer(colorDataBuffer);
				colorAttribute->setDataType(Qt3DRender::QAttribute::Float);
				colorAttribute->setDataSize(4);
				colorAttribute->setByteOffset(0);
				colorAttribute->setByteStride(4 * sizeof(float));
				colorAttribute->setCount(numVertices);
				colorAttribute->setName(Qt3DRender::QAttribute::defaultColorAttributeName());

				/*Qt3DRender::QAttribute *indexAttribute = new Qt3DRender::QAttribute();
				indexAttribute->setAttributeType( Qt3DRender::QAttribute::IndexAttribute );
				indexAttribute->setBuffer( indexDataBuffer );
				indexAttribute->setDataType( Qt3DRender::QAttribute::UnsignedShort );
				indexAttribute->setDataSize( 1 );
				indexAttribute->setByteOffset( 0 );
				indexAttribute->setByteStride( 0 );
				indexAttribute->setCount( 12 );*/

				cachedGeom->setBoundingVolumePositionAttribute(positionAttribute);

				cachedGeom->addAttribute(positionAttribute);
				cachedGeom->addAttribute(normalAttribute);
				cachedGeom->addAttribute(colorAttribute);
				//cachedGeom->addAttribute( indexAttribute );

				//cachedGeom->setProperty( "numPrimitives", numVertices );
			}

			QStringList entityHierarchy = QString(scene.name().c_str()).split('/');

			Qt3DCore::QEntity * entity = NULL;
			Qt3DCore::QEntity * parent = this;

			for (int i = 0; i < entityHierarchy.size(); ++i)
			{
				Qt3DCore::QEntity* existing = findChild<Qt3DCore::QEntity*>(entityHierarchy[i]);
				if (existing == NULL)
				{
					existing = new Qt3DCore::QEntity();
					existing->setObjectName(entityHierarchy[i]);
					existing->setParent(parent);
				}

				entity = existing;
				parent = existing;
			}

			processNode(scene, scene.rootnode(), entity);

			emit info(QString("Created Group %1 with %2 meshes").arg(entity->objectName()).arg(scene.meshes_size()), details);
			//AddToScene( entity );
		}
		else
		{
			emit info(QString("Unable to parse message %1").arg(scene.descriptor()->name().c_str()), QString());
		}
	}
	catch (const std::exception & ex)
	{
	}
}

void AnalyticsScene::AddToScene(Qt3DCore::QEntity* entity)
{
	Qt3DCore::QEntity* oldChild = findChild<Qt3DCore::QEntity*>(entity->objectName());
	if (oldChild != NULL)
	{
		oldChild->setParent((Qt3DCore::QNode*)NULL);
		oldChild->deleteLater();

		// put the new components on the old entity
		//oldChild->removeAllComponents();

		//Qt3DCore::QComponentList components = entity->components();
		//foreach( Qt3DCore::QComponent* cmp, components )
		//{
		//	cmp->setParent( NULL );
		//	oldChild->addComponent( cmp );
		//}

		//// delete the ne
		//entity->deleteLater();
	}

	entity->setParent(this);
}

void AnalyticsScene::processNode(const modeldata::Scene& scene, const modeldata::Node& node, Qt3DCore::QEntity* entity)
{
	QMatrix4x4 xform;
	xform.setToIdentity();

	if (node.has_eulerrotation())
	{
		const modeldata::Vec3& euler = node.eulerrotation();

		xform.rotate(euler.x(), QVector3D(1.0f, 0.0f, 0.0f));
		xform.rotate(euler.y(), QVector3D(0.0f, 1.0f, 0.0f));
		xform.rotate(euler.z(), QVector3D(0.0f, 0.0f, 1.0f));
	}

	if (node.has_translation())
	{
		const modeldata::Vec3& vec = node.translation();
		xform.translate(vec.x(), vec.y(), vec.z());
	}

	/*Qt3DCore::QPhongMaterial * mtrl = new Qt3DCore::QPhongMaterial();
	mtrl->setDiffuse( QColor( "grey" ) );*/

	Qt3DExtras::QPerVertexColorMaterial * mtrl = new Qt3DExtras::QPerVertexColorMaterial();

	Qt3DCore::QTransform * cmpXform = new Qt3DCore::QTransform();

	Qt3DRender::QObjectPicker* picker = new Qt3DRender::QObjectPicker();
	picker->setDragEnabled(false);
	picker->setHoverEnabled(false);

	Qt3DRender::QPickingSettings* pickerSettings = new Qt3DRender::QPickingSettings();
	pickerSettings->setPickMethod(Qt3DRender::QPickingSettings::PrimitivePicking);
	pickerSettings->setPickResultMode(Qt3DRender::QPickingSettings::NearestPick);

	entity->addComponent(mtrl);
	entity->addComponent(cmpXform);
	entity->addComponent(picker);

	if (node.has_meshname())
	{
		GeomMap::iterator it = mGeomCache.find(node.meshname());
		if (it != mGeomCache.end())
		{
			Qt3DRender::QGeometryRenderer *renderer = new Qt3DRender::QGeometryRenderer();

			renderer->setInstanceCount(1);
			renderer->setFirstVertex(0);
			renderer->setFirstInstance(0);
			renderer->setPrimitiveType(Qt3DRender::QGeometryRenderer::Triangles);
			renderer->setGeometry(it->second);
			renderer->setVertexCount(it->second->boundingVolumePositionAttribute()->count());

			/*cmp->setInstanceCount( 1 );
			cmp->setBaseVertex( 0 );
			cmp->setBaseInstance( 0 );
			cmp->setPrimitiveType( Qt3DRender::QGeometryRenderer::Triangles );
			cmp->setGeometry( it->second );
			cmp->setPrimitiveCount( it->second->verticesPerPatch() );*/
			entity->addComponent(renderer);
		}
	}

	for (int c = 0; c < node.children_size(); ++c)
	{
		const modeldata::Node& child = node.children(c);

		Qt3DCore::QEntity * childEntity = new Qt3DCore::QEntity();
		childEntity->setObjectName(child.meshname().c_str());
		processNode(scene, child, childEntity);
		childEntity->setParent(entity);
	}
}
