#include <iostream>
#include <windows.h>
#include <fstream>

#include "GameAnalytics.h"

static int RandInRange( int minValue, int maxValue )
{
	return minValue + rand() % (maxValue-minValue);
}

int main(int argc, char *argv[])
{
	GameAnalyticsKeys keys;
	keys.mGameKey		= "3d1b7bb3b2ad8c7d4d784f26505abf44";
	keys.mSecretKey		= "821f4c5a4690ea2460124f474a65ab582163040d";
	keys.mDataApiKey	= "a1af2abdb11ef2257f9e8941b072984731c349e0";
	keys.mVersionKey	= "GameAnalyticsTest";
	
	GameAnalyticsLogger logger( keys );

	logger.CreateDatabase( "analytics.db" );

	logger.AddModel( "testmodel", "Some string data with model information", 0 );
	
	/*for ( size_t i = 0; i < 10; ++i )
	{
		Json::Value ent( Json::objectValue );
		ent[ "name" ] = "EntityName";
		ent[ "class" ] = "EntityClass";
		ent[ "team" ] = "EntityTeam";
		logger.AddEntity( i, ent );
	}*/

	const int worldSizeX = 5000;
	const int worldSizeY = 3500;

	const char * areaName = "TestAppArea";
	const char * eventName = "TestAppEvent";
	const size_t numDesignEvents = 10000;
	const size_t numQualityEvents = 100;

	for ( size_t i = 0; i < numDesignEvents; ++i )
	{
		const float pos[3] = { (float)RandInRange(-worldSizeX,worldSizeX), (float)RandInRange(-worldSizeY,worldSizeY), 0.f };
		logger.AddGameEvent( areaName, eventName, pos, (float)RandInRange( 0, 5 ) );
	}

	for ( size_t i = 0; i < numQualityEvents; ++i )
	{
		logger.AddQualityEvent( areaName, eventName, "Test Quality Event" );
	}
	
	/*const size_t submittedQualityEvents = logger.SubmitQualityEvents();
	const size_t submittedDesignEvents = logger.SubmitDesignEvents();	

	std::cout << "Submitted " << submittedDesignEvents << " Design Events" << std::endl;
	std::cout << "Submitted " << submittedQualityEvents << " Quality Events" << std::endl;

	if ( const GameAnalyticsHeatmap * heatmap = logger.GetHeatmap( areaName, eventName, true ) )
	{
		std::cout << "Got heatmap with " << heatmap->mEvents.size() << " events." << std::endl;

	}
	else
	{
		std::cout << "No heatmap 'Test Area'" << std::endl;
	}*/
	GameAnalyticsLogger::HeatmapDef def;
	def.mAreaId = areaName;
	def.mEventId = eventName;
	def.mEventRadius = 256.0f;
	def.mImageSize = 10000;
	def.mWorldMins[ 0 ] = -(float)worldSizeX;	
	def.mWorldMaxs[ 0 ] =  (float)worldSizeX;
	def.mWorldMins[ 1 ] = -(float)worldSizeY;
	def.mWorldMaxs[ 1 ] =  (float)worldSizeY;

	std::string script;
	logger.WriteHeatmapScript( def, script );

	std::fstream scriptOut;
	scriptOut.open( std::string(areaName) + ".mgk", std::ios_base::out | std::ios_base::trunc );
	scriptOut << script << std::endl;
	scriptOut.close();

	std::string error;
	while ( logger.GetError( error ) )
	{
		std::cout << error << std::endl;
	}

	logger.CloseDatabase();

	return 0;
}