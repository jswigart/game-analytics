#include <QtGui/QWheelEvent>

#include "timelinegraphicsview.h"

TimelineGraphicsView::TimelineGraphicsView( QWidget *parent)
	: QGraphicsView(parent)
{ 
}

TimelineGraphicsView::~TimelineGraphicsView()
{

}

void TimelineGraphicsView::wheelEvent( QWheelEvent * event )
{
	//int numDegrees = event->delta() / 8;
	//int numSteps = numDegrees / 15; // see QWheelEvent documentation
	//_numScheduledScalings += numSteps;
	//if ( _numScheduledScalings * numSteps < 0 ) // if user moved the wheel in another direction, we reset previously scheduled scalings
	//	_numScheduledScalings = numSteps;

	//QTimeLine *anim = new QTimeLine( 350, this );
	//anim->setUpdateInterval( 20 );

	//connect( anim, SIGNAL( valueChanged( qreal ) ), SLOT( scalingTime( qreal ) ) );
	//connect( anim, SIGNAL( finished() ), SLOT( animFinished() ) );
	//anim->start();

	if ( event->delta() > 0 )
		scale( 0.95, 0.95 );
	else if ( event->delta() < 0 )
		scale( 1.15, 1.15 );
}
