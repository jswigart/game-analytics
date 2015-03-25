#include "analyticsviewer.h"
#include <QtWidgets/QApplication>

int main( int argc, char *argv [] )
{
	QApplication a( argc, argv );
	AnalyticsViewer w;
	w.show();
	return a.exec();
}
