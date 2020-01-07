#ifndef TIMELINEGRAPHICSVIEW_H
#define TIMELINEGRAPHICSVIEW_H

#include <QtWidgets/QGraphicsView>

class TimelineGraphicsView : public QGraphicsView
{
	Q_OBJECT

public:
	TimelineGraphicsView( QWidget *parent = Q_NULLPTR );
	~TimelineGraphicsView();

	void wheelEvent( QWheelEvent * event );
private:
};

#endif // TIMELINEGRAPHICSVIEW_H
