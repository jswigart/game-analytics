#include "zmq.hpp"

#include <iostream>
#include <windows.h>
#include <fstream>

#include "GameAnalytics.h"

static int RandInRange( int minValue, int maxValue )
{
	return minValue + rand() % (maxValue-minValue);
}

class Errors : public ErrorCallbacks
{
public:
	void Error( const char * str )
	{
		std::cout << str << std::endl;
	}
};

int main(int argc, char *argv[])
{
	GameAnalytics::Keys keys;
	keys.mGameKey		= "3d1b7bb3b2ad8c7d4d784f26505abf44";
	keys.mSecretKey		= "821f4c5a4690ea2460124f474a65ab582163040d";
	keys.mDataApiKey	= "a1af2abdb11ef2257f9e8941b072984731c349e0";
	keys.mVersionKey	= "GameAnalyticsTest";
	
	GameAnalytics logger( keys, new Errors );
	
	zmq::context_t zmqcontext( 2 );
	
	zmqPublisher pub( zmqcontext, "127.0.0.1", 40000 );
	zmqSubscriber sub( zmqcontext, "127.0.0.1", 40000 );
	sub.Subscribe( "game" );

	logger.CreateDatabase( "analytics.db" );
	logger.SetPublisher( &pub );

	int frameNum = 0;
	while ( true )
	{
		try
		{
			Analytics::MessageUnion msg;

			while ( sub.Poll( msg ) )
			{
				std::vector<const google::protobuf::FieldDescriptor*> fields;
				msg.GetReflection()->ListFields( msg, &fields );

				std::cout << "Subscriber recieved message: ";
				for ( size_t i = 0; i < fields.size(); ++i )
					std::cout << fields[ i ]->camelcase_name() << " ";
				std::cout << std::endl;
			}

			while ( pub.Poll( msg ) )
			{
				std::vector<const google::protobuf::FieldDescriptor*> fields;
				msg.GetReflection()->ListFields( msg, &fields );

				std::cout << "Publisher recieved message: ";
				for ( size_t i = 0; i < fields.size(); ++i )
					std::cout << fields[ i ]->camelcase_name() << " ";
				std::cout << std::endl;
			}

			if ( ( frameNum % 100 ) == 0 )
			{
				Analytics::MessageUnion msg;
				//msg.set_timestamp( logger.GetTimeStamp() );
				msg.mutable_gamedeath()->set_killedbyclass( 5 );
				msg.mutable_gamedeath()->set_killedbyhealth( 10 );
				msg.mutable_gamedeath()->set_killedbyweapon( 15 );

				logger.AddEvent( msg );
			}

			if ( ( frameNum % 100 ) == 0 )
			{
				Analytics::MessageUnion msg;
				//msg.set_timestamp( logger.GetTimeStamp() );
				msg.mutable_gameassert()->set_condition( "test string" );
				logger.AddEvent( msg );
			}
		}
		catch ( const std::exception& ex )
		{
			std::cout << "Network error: " << ex.what() << std::endl;
			break;
		}

		Sleep( 100 );

		++frameNum;
	}

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
	
	Analytics::MessageUnion msg;
	for ( size_t i = 0; i < numDesignEvents; ++i )
	{
		msg.Clear();

		msg.set_timestamp( i * 100 ); // artificial timestamps

		Analytics::GameWeaponFired* event = msg.mutable_gameweaponfired();
		event->set_weaponid( rand() % 10 );
		event->set_team( rand() % 4 );
		event->set_positionx( (float)RandInRange( -worldSizeX, worldSizeX ) );
		event->set_positiony( (float)RandInRange( -worldSizeY, worldSizeY ) );
		event->set_positionz( 0.0f );
		
		logger.AddEvent( msg );
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
	GameAnalytics::HeatmapDef def;
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
	
	logger.CloseDatabase();

	return 0;
}