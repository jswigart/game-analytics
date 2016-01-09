#ifndef TIMELINEGRAPHICSSCENE_H
#define TIMELINEGRAPHICSSCENE_H

#include <QGraphicsScene>

class TimelineGraphicsScene : public QGraphicsScene
{
	Q_OBJECT
public:
	TimelineGraphicsScene( QObject *parent = Q_NULLPTR );
	~TimelineGraphicsScene();
	
	bool focusNextPrevChild( bool next );

	int AddRow( const QString& text );
	void AddTick( int row, qint64 milliseconds );
private Q_SLOTS:
	void SceneSizeChanged( const QRectF &rect );
private:
	qreal					mRowHeight;
	enum ItemData
	{
		DataNone,
		DataTimestamp,
		DataRow,
	};
	struct Row
	{
		QGraphicsTextItem*				mRowText;
		std::vector<QGraphicsItem*>		mTicks;
	};

	qint64				mTimeBase;
	QGraphicsLineItem*	mDividerLineH;
	QGraphicsLineItem*	mDividerLineV;
	QVector<Row>		mRows;
	
};

#endif // TIMELINEGRAPHICSSCENE_H
