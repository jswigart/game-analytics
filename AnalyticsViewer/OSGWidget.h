#ifndef OSGWidget_h__
#define OSGWidget_h__

#include <QPoint>
#include <QtOpenGL>

#include <osg/ref_ptr>

#include <osgViewer/GraphicsWindow>
#include <osgViewer/CompositeViewer>

#define WITH_SELECTION_PROCESSING 0
#define WITH_PICK_HANDLER 0

class OSGWidget : public QGLWidget
{
	Q_OBJECT
public:
	OSGWidget( QWidget* parent = 0, const QGLWidget* shareWidget = 0, Qt::WindowFlags f = 0 );
	virtual ~OSGWidget();

	bool exportSceneToFile( const QString & filePath );

	void visitSceneNodes( osg::NodeVisitor & visitor );

	osg::MatrixTransform * findOrCreateEntityNode( int entityId );	
public slots:
	void allocProxyNode( const QString& filename );
	bool importModelToScene( const QString & filePath, bool clearExisting );
protected:
	virtual void paintGL();
	virtual void resizeGL( int width, int height );

	virtual void keyPressEvent( QKeyEvent* event );
	virtual void keyReleaseEvent( QKeyEvent* event );

	virtual void mouseMoveEvent( QMouseEvent* event );
	virtual void mousePressEvent( QMouseEvent* event );
	virtual void mouseReleaseEvent( QMouseEvent* event );
	virtual void wheelEvent( QWheelEvent* event );

	virtual bool event( QEvent* event );
private:
	virtual void onHome();
	virtual void onResize( int width, int height );

	osgGA::EventQueue* getEventQueue() const;

	osg::ref_ptr<osg::Group>							mRoot;
	osg::ref_ptr<osgViewer::GraphicsWindowEmbedded>		mGraphicsWindow;
	osg::ref_ptr<osgViewer::CompositeViewer>			mViewer;

	osg::ref_ptr<osg::Group>							mEntityGroup;
	osg::ref_ptr<osg::Group>							mMapGroup;
	
	typedef QMap<int, osg::MatrixTransform*> EntityMap;
	EntityMap											mEntityMap;

#if(WITH_SELECTION_PROCESSING)
	QPoint selectionStart_;
	QPoint selectionEnd_;

	bool selectionActive_;
	bool selectionFinished_;
#endif

	void processSelection();
};

#endif
