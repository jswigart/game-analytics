#include <QtCore/QTimer>
#include <QtWidgets/QGraphicsTextItem>
#include <QtWidgets/QGraphicsSceneMouseEvent>

#include "TimelineGraphicsScene.h"

TimelineGraphicsScene::TimelineGraphicsScene( QObject *parent )
	: QGraphicsScene( parent )
	, mTimeBase( -1 )
	, mRowHeight( 20.0 )
{
	mDividerLineH = addLine( 0.0, 0.0, 1000.0, 0.0 );
	mDividerLineV = addLine( 0.0, 0.0, 0.0, 200.0 );

	QGraphicsTextItem* events = addText( "Events" );
	events->setPos( -events->boundingRect().width(), -events->boundingRect().height() );

	connect( this, SIGNAL( sceneRectChanged( QRectF ) ), this, SLOT( SceneSizeChanged( QRectF ) ) );
}

TimelineGraphicsScene::~TimelineGraphicsScene()
{
}

void TimelineGraphicsScene::SceneSizeChanged( const QRectF &rect )
{
	mDividerLineH->setLine( 0.0, 0.0, rect.right()-10.0f, 0.0 );
}

int TimelineGraphicsScene::AddRow( const QString& text )
{
	Row row;
	row.mRowText = new QGraphicsTextItem();
	row.mRowText->setPlainText( text );
	row.mRowText->setPos( -row.mRowText->boundingRect().width(), mRows.size() * mRowHeight );
	row.mRowText->setData( DataRow, QVariant( mRows.size() ) );

	addItem( row.mRowText );
	mRows.push_back( row );

	// update the dividor line to the new size of the rows
	mDividerLineV->setLine( 0.0, 0.0, 0.0, mRows.size() * mRowHeight );

	return mRows.size() - 1;
}

QGraphicsItem* TimelineGraphicsScene::AddTick( int row, qint64 milliseconds )
{
	if ( row >= 0 && row < mRows.size() )
	{
		if ( mTimeBase < 0 )
			mTimeBase = milliseconds;

		Row& rowRef = mRows[ row ];

		const qint64 msDelta = milliseconds - mTimeBase;
		const qreal x = (qreal)msDelta / 10.0;
		const qreal y = (qreal)row * mRowHeight;
		
		QGraphicsLineItem * tick = addLine( x, y, x, y + 20.0 );
		tick->setData( DataTimestamp, QVariant( milliseconds ) );
		tick->setFlag( QGraphicsItem::ItemIsSelectable, true );
		tick->setFlag( QGraphicsItem::ItemIsFocusable, true );

		// put it into the ticks list timestamp ordered 
		if ( rowRef.mTicks.empty() )
			rowRef.mTicks.push_back( tick );
		else
		{
			qulonglong ts0, ts1;
			ts0 = rowRef.mTicks.front()->data( DataTimestamp ).toULongLong();
			ts1 = rowRef.mTicks.back()->data( DataTimestamp ).toULongLong();

			if ( milliseconds < ts0 )
				rowRef.mTicks.insert( rowRef.mTicks.begin(), tick );
			else if ( milliseconds > ts1 )
				rowRef.mTicks.push_back( tick );
			else
			{
				for ( std::vector<QGraphicsItem*>::iterator it = rowRef.mTicks.begin();
					it != rowRef.mTicks.end(); ++it )
				{
					ts0 = ( *it )->data( DataTimestamp ).toULongLong();
					if ( milliseconds < ts0 )
					{
						rowRef.mTicks.insert( it, tick );
						break;
					}
				}
			}
		}

		return tick;
	}
	return nullptr;
}

bool TimelineGraphicsScene::focusNextPrevChild( bool next )
{
	QGraphicsItem* item = focusItem();
	if ( item != NULL )
	{
		QVariant var = item->data( DataRow );

		bool valid = false;

		const int row = var.toInt( &valid );
		if ( valid && row >= 0 && row < mRows.size() )
		{
			Row& rowRef = mRows[ row ];
			for ( size_t i = 0; i < rowRef.mTicks.size(); ++i )
			{
				if ( rowRef.mTicks[ i ] == item )
				{
					i += ( next ? 1 : -1 );
					setFocusItem( rowRef.mTicks[ i % rowRef.mTicks.size() ] );
					return true;
				}
			}
		}
	}
	return false;
}
