#include "GameAnalytics.h"
#include <sstream>
#include <fstream>
#include <set>
#include <algorithm> // std::remove

#include "sqlite3.h"

#include "curl\curl.h" // libcurl, for sending http requests
#include "MD5.h"  // MD5 hashing algorithm

#include <zmq.hpp>

#include "google/protobuf/io/zero_copy_stream_impl.h"
#include "google/protobuf/io/coded_stream.h"

#undef GetMessage

#ifdef CODE_ANALYSIS
#include <CodeAnalysis\SourceAnnotations.h>
#define CHECK_PRINTF_ARGS			[SA_FormatString(Style="printf")]
#define CHECK_PARAM_VALID			[vc_attributes::Pre(Valid=vc_attributes::Yes)]
#define CHECK_VALID_BYTES(parm)		[vc_attributes::Pre(ValidBytes=#parm)]
#else
#define CHECK_PRINTF_ARGS
#define CHECK_PARAM_VALID
#define CHECK_VALID_BYTES(parm)
#endif

// todo: CURLOPT_PROGRESSFUNCTION

// Callback to take returned data and convert it to a string.
// The last parameter is actually user-defined - it can be whatever
//   type you want it to be.
static int DataToString(char *data, size_t size, size_t nmemb, std::string * result)
{
	int count = 0;
	if (result)
	{
		// Data is in a character array, so just append it to the string.
		result->append(data, size * nmemb);
		count = size * nmemb;
	}
	return count;
}

class vaAnalytics {
public:
	const char * c_str() const { return buffer; }
	operator const char *() const { return buffer; }

	vaAnalytics(CHECK_PRINTF_ARGS const char* msg, ...)
	{
		va_list list;
		va_start(list, msg);
#ifdef WIN32
		_vsnprintf(buffer, BufferSize, msg, list);
#else
		vsnprintf(buffer, BufferSize, msg, list);
#endif
		va_end(list);
	}
protected:
	enum { BufferSize = 1024 };
	char buffer[BufferSize];
};

#define va vaAnalytics

//////////////////////////////////////////////////////////////////////////

//void GameAnalyticsEvent::WriteToObject( Json::Value & obj ) const
//{
//	if ( mUseEventId )
//		obj[ "event_id" ] = mEventId;
//	if ( mUseArea )
//		obj[ "area" ] = mArea;
//	if ( mUseLocation )
//	{
//		obj[ "x" ] = mLocation.x;
//		obj[ "y" ] = mLocation.y;
//		obj[ "z" ] = mLocation.z;
//	}
//}

//void GameAnalyticsEvent::SetTime( size_t t )
//{
//	mEventTime = t;
//}

//////////////////////////////////////////////////////////////////////////

AnalyticsPublisher::AnalyticsPublisher()
{
}

AnalyticsPublisher::~AnalyticsPublisher()
{
}

//////////////////////////////////////////////////////////////////////////

AnalyticsSubscriber::AnalyticsSubscriber()
{
}

AnalyticsSubscriber::~AnalyticsSubscriber()
{
}

//////////////////////////////////////////////////////////////////////////

GameAnalytics::GameAnalytics( const Keys & keys )
	: mKeys( keys )
	, mDatabase( NULL )
	, mPublisher( NULL )
{
    // Initialize libcurl.
    curl_global_init(CURL_GLOBAL_ALL);

	SetUserID();
	SetSessionID();
}

//////////////////////////////////////////////////////////////////////////


GameAnalytics::~GameAnalytics()
{
	delete mPublisher;
}

//////////////////////////////////////////////////////////////////////////

void GameAnalytics::SetPublisher( AnalyticsPublisher * publisher )
{
	mPublisher = publisher;
}

//////////////////////////////////////////////////////////////////////////

AnalyticsPublisher * GameAnalytics::GetPublisher() const
{
	return mPublisher;
}

//////////////////////////////////////////////////////////////////////////

//void GameAnalyticsEventDesign::WriteToObject( Json::Value & obj ) const
//{
//	GameAnalyticsEvent::WriteToObject( obj );
//
//	if ( mUseValue )
//		obj[ "value" ] = mValue;
//}

//////////////////////////////////////////////////////////////////////////

//void GameAnalyticsEventQuality::WriteToObject( Json::Value & obj ) const
//{
//	GameAnalyticsEvent::WriteToObject( obj );
//
//	if ( mUseMessage )
//		obj[ "message" ] = mMessage;
//}

// User ID and session ID are necessary for all events to identify the source uniquely
// When possible this should probably be some sort of UID for a particular user
// For general use using a network MAC address is probably a decent way to create a UID
// More info here: http://support.gameanalytics.com/entries/23054568-Generating-unique-user-identifiers

// This hashes the user's MAC address to create an ID unique to that machine.
void GameAnalytics::SetUserID()
{
	mUserId = "DREVIL";

#if(0)
    // Prepare an array to get info for up to 16 network adapters.
    // It's a little hacky, but it's way complicated to do otherwise,
    //   and the chances of someone having more than 16 are pretty crazy.
    IP_ADAPTER_INFO info[16];

    // Get info for all the network adapters.
    DWORD dwBufLen = sizeof(info);
    DWORD dwStatus = GetAdaptersInfo(info, &dwBufLen);
    if (dwStatus != ERROR_SUCCESS)
    {
        /* deal with error */
    }
    else
    {
        PIP_ADAPTER_INFO pAdapterInfo = info;

        // Iterate through the adapter array until we find one.
        // (Will most likely be the first.)
        while (pAdapterInfo && pAdapterInfo == 0)
        {
            pAdapterInfo = pAdapterInfo->Next;
        }
        
        if (!pAdapterInfo)
        {
            // No network adapter found. In my code I just use a GUID at this point,
            //   (same code to get a session ID), but I'll leave that up to you.
        }
        else
        {
            // Get a hash of the MAC address using SHA1.
            unsigned char hash[10];
            char hexstring[41];
            sha1::calc(pAdapterInfo->Address, pAdapterInfo->AddressLength, hash);
            sha1::toHexString(hash, hexstring);
            mUserId = hexstring;

            // Remove any hyphens.
            mUserId.erase(std::remove(mUserId.begin(), mUserId.end(), '-'), mUserId.end());
        }
    }
#endif
}

// Sets the session ID with a GUID (Globally Unique Identifier).
void GameAnalytics::SetSessionID()
{
    // Get the GUID.
    //GUID guid;
    //CoCreateGuid(&guid);

    //// Turn the GUID into an ASCII string.
    //RPC_CSTR str;
    //UuidToStringA((UUID*)&guid, &str);

    //// Get a standard string out of that.
    //session_id_ = (LPSTR)str;

    //// Remove any hyphens.
    //session_id_.erase(std::remove(session_id_.begin(), session_id_.end(), '-'), session_id_.end());

	// just use the start time for the session id
	std::stringstream ss;

#ifdef WIN32
	LARGE_INTEGER timer;
	QueryPerformanceCounter(&timer);
	ss << "TS" << timer.QuadPart;
#else
	clock_t timer = clock();
	ss << "TS" << timer;
#endif	
	
	mSessionId = ss.str();
}

/*void GameAnalytics::BuildDesignEventsJsonTree( Json::Value & tree )
{
	tree = Json::Value(Json::arrayValue);
	tree.resize( mEventsDesign.size() );

	for ( size_t i = 0; i < mEventsDesign.size(); ++i )
	{
		const GameAnalyticsEvent & evt = mEventsDesign[ i ];

		Json::Value & evtValue = tree[ i ];

		evtValue = Json::Value( Json::objectValue );
		evtValue[ "user_id" ] = mUserId;
		evtValue[ "session_id" ] = mSessionId;
		evtValue[ "build" ] = mKeys.mVersionKey;
		evt.WriteToObject( evtValue );
	}
}*/

/*void GameAnalytics::BuildQualityEventsJsonTree( Json::Value & tree )
{
	tree = Json::Value(Json::arrayValue);
	tree.resize( mEventsQuality.size() );

	for ( size_t i = 0; i < mEventsQuality.size(); ++i )
	{
		const GameAnalyticsEvent & evt = mEventsQuality[ i ];

		Json::Value & evtValue = tree[ i ];

		evtValue = Json::Value( Json::objectValue );
		evtValue[ "user_id" ] = mUserId;
		evtValue[ "session_id" ] = mSessionId;
		evtValue[ "build" ] = mKeys.mVersionKey;
		evt.WriteToObject( evtValue );
	}
}*/

size_t GameAnalytics::SubmitDesignEvents()
{
	std::stringstream query;
	query << "SELECT * in events WHERE ()";

	//sqlite3_prepare_v2( mDatabase, "SELECT * in events WHERE ()" );
	
	// New CURL object.
	CURL * curl = curl_easy_init();
	if (curl)
	{
		// build the event string to send
		/*Json::Value root;
		BuildDesignEventsJsonTree( root );
		
		Json::StyledWriter writer;*/
		const std::string gaEvents;// = writer.write( root );
				
		// The URL to send events to
		const std::string url = BuildUrl( "design" );

		// Combine your events string and secret key for the header.
		std::string header = gaEvents + mKeys.mSecretKey;
		// Then hash that, and give it the label "Authorization:".
		std::string auth = "Authorization:" + md5(header);

		// Create a struct of headers.
		struct curl_slist *chunk = NULL;
		// Add the auth header to the struct.
		chunk = curl_slist_append(chunk, auth.c_str());

		// Set the URL of the request.
		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		// Set the POST data (the string of events).
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, gaEvents.c_str());
		// Set the headers (hashed secret key + data).
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
		// Set request mode to POST.
		curl_easy_setopt(curl, CURLOPT_POST, 1);

		std::string result;
		// Set the callback to deal with the returned data.
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, DataToString);
		// Pass in a string to accept the result.
		// (This will be taken care of in the callback function.)
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result);

		// Send the request!
		const CURLcode res = curl_easy_perform(curl);
		
		// Cleanup your CURL object.
		curl_easy_cleanup(curl);
		// Free the headers.
		curl_slist_free_all(chunk);

		// Check the error flag.
		if (res != CURLE_OK)
		{
			std::string err = "Error Submitting Design Events ";
			err += curl_easy_strerror(res);
			mErrors.push_back( err );
			return 0;
		}
	}

	const size_t eventsSubmitted = 0;
	/*const size_t eventsSubmitted = mEventsDesign.size();
	mEventsDesign.resize( 0 );*/
	return eventsSubmitted;
}

size_t GameAnalytics::SubmitQualityEvents()
{
	/*if ( mEventsQuality.size() == 0 )
		return 0;*/

	// New CURL object.
	CURL * curl = curl_easy_init();
	if (curl)
	{
		// build the event string to send
		/*Json::Value root;
		BuildQualityEventsJsonTree( root );
		
		Json::StyledWriter writer;*/
		const std::string gaEvents;// = writer.write( root );
		
		const std::string url = BuildUrl( "quality" );

		// Combine your events string and secret key for the header.
		std::string header = gaEvents + mKeys.mSecretKey;
		// Then hash that, and give it the label "Authorization:".
		std::string auth = "Authorization:" + md5(header);

		// Create a struct of headers.
		struct curl_slist *chunk = NULL;
		// Add the auth header to the struct.
		chunk = curl_slist_append(chunk, auth.c_str());

		// Set the URL of the request.
		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		// Set the POST data (the string of events).
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, gaEvents.c_str());
		// Set the headers (hashed secret key + data).
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
		// Set request mode to POST.
		curl_easy_setopt(curl, CURLOPT_POST, 1);

		std::string result;
		// Set the callback to deal with the returned data.
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, DataToString);
		// Pass in a string to accept the result.
		// (This will be taken care of in the callback function.)
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result);

		// Send the request!
		const CURLcode res = curl_easy_perform(curl);
		
		// Cleanup your CURL object.
		curl_easy_cleanup(curl);
		// Free the headers.
		curl_slist_free_all(chunk);

		// Check the error flag.
		if (res != CURLE_OK)
		{
			std::string err = "Error Submitting Quality Events ";
			err += curl_easy_strerror(res);
			mErrors.push_back( err );
			return 0;
		}
	}

	const size_t eventsSubmitted = 0;
	/*const size_t eventsSubmitted = mEventsQuality.size();
	mEventsQuality.resize( 0 );*/
	return eventsSubmitted;
}

std::string GameAnalytics::BuildUrl( const std::string & category )
{
	// Currently the API's version is 1. Leave it like this for now.
	const std::string apiVersion_ = "1";

	// category options are
	// design (gameplay)
	// quality (quality assurance)
	// business (transactions)
	// user (player profiles)
	return std::string("http://api.gameanalytics.com/") + apiVersion_ + "/" + mKeys.mGameKey + "/" + category;
}

const GameAnalytics::Heatmap * GameAnalytics::GetHeatmap( const std::string & area, const std::string & eventIds, bool loadFromServer )
{
	HeatMapsByName::iterator it = mHeatMaps.find( area );
	if ( it != mHeatMaps.end() )
		return it->second;

	if ( !loadFromServer )
		return NULL;

	// New CURL object.
	CURL * curl = curl_easy_init();
	if (curl)
	{
		// Request URL for getting heatmap data.
		std::string url = "http://data-api.gameanalytics.com/heatmap";
		// Specify what area the heatmap should be from, and what events it
		//   should be getting data for.
		// Note: you can ask for multiple events at the same time with |,
		//   like "Death:Enemy|Death:Fall".
		std::string requestInfo = "game_key=" + mKeys.mGameKey + "&event_ids=" + eventIds + "&area=" + area;
		// Concatenate the request with the URL.
		url += "/?";
		url += requestInfo;

		// Build the header with the request and API key.
		std::string header = requestInfo + mKeys.mDataApiKey;
		// Hash that, and give it the label "Authorization:".
		std::string auth = "Authorization:" + md5(header);

		// Create a struct of headers.
		struct curl_slist *chunk = NULL;
		// Add the auth header to the struct.
		chunk = curl_slist_append(chunk, auth.c_str());

		// Set the URL of the request.
		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		// Set the headers (hashed API key + request).
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
		
		std::string result;
		// Set the callback to deal with the returned data.
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, DataToString);
		// Pass in a string to accept the result.
		// (This will be taken care of in the callback function.)
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result);

		// Send the request!
		const CURLcode res = curl_easy_perform(curl);
		
		// Cleanup your CURL object.
		curl_easy_cleanup(curl);
		// Free the headers.
		curl_slist_free_all(chunk);

		// Check the error flag.
		if (res != CURLE_OK)
		{
			std::string err = "Error Getting Heat Map ";
			err += curl_easy_strerror(res);
			mErrors.push_back( err );
			return NULL;
		}

		//Json::Reader reader;
		//Json::Value jsonRoot;
		//if ( !reader.parse( result, jsonRoot ) )
		//{
		//	reader.getFormatedErrorMessages();
		//	return NULL;
		//}

		//// Data returned consists of 4 arrays: x, y, z, and value.
		//// x, y, and z being coordinates of course, and value being
		////   the number of instances at that point.
		//
		//Json::Value heatx = jsonRoot.get( "x", Json::Value::null );
		//Json::Value heaty = jsonRoot.get( "y", Json::Value::null );
		//Json::Value heatz = jsonRoot.get( "z", Json::Value::null );
		//Json::Value heatvalue = jsonRoot.get( "value", Json::Value::null );

		//if ( heatx.isArray() && heaty.isArray() && heatz.isArray() && heatvalue.isArray() )
		//{
		//	GameAnalyticsHeatmap * newHeatMap = new GameAnalyticsHeatmap();

		//	// Iterate through the arrays. (We're just iterating through x technically,
		//	//   but all arrays are the same length.)
		//	newHeatMap->mEvents.resize( heatx.size() );

		//	for (unsigned i = 0; i < heatx.size(); ++i)
		//	{
		//		newHeatMap->mEvents[ i ].mXYZ[0] = (float)heatx.get( i, Json::Value::null ).asDouble();
		//		newHeatMap->mEvents[ i ].mXYZ[1] = (float)heaty.get( i, Json::Value::null ).asDouble();
		//		newHeatMap->mEvents[ i ].mXYZ[2] = (float)heatz.get( i, Json::Value::null ).asDouble();
		//		newHeatMap->mEvents[ i ].mValue = (float)heatvalue.get( i, Json::Value::null ).asDouble();
		//	}

		//	mHeatMaps.insert( std::make_pair( area, newHeatMap ) );
		//	return newHeatMap;
		//}
	}
	return NULL;
}

bool GameAnalytics::GetError( std::string & errorOut )
{
	if ( !mErrors.empty() )
	{
		errorOut.swap( mErrors.front() );
		mErrors.pop_front();
		return true;
	}
	return false;
}

bool GameAnalytics::CreateDatabase( const char * filename )
{
	std::string filepath = "file:";
	filepath += filename;

	if ( CheckSqliteError( sqlite3_open_v2( filepath.c_str(), &mDatabase, SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE | SQLITE_OPEN_URI, NULL ) ) != SQLITE_OK )
		return false;
	
	CheckSqliteError( sqlite3_exec( mDatabase, "PRAGMA synchronous=OFF", NULL, NULL, NULL ) );
	CheckSqliteError( sqlite3_exec( mDatabase, "PRAGMA journal_mode=MEMORY", NULL, NULL, NULL ) );
	CheckSqliteError( sqlite3_exec( mDatabase, "PRAGMA temp_store=MEMORY", NULL, NULL, NULL ) );

	enum { NumDefaultTables = 2 };
	const char * defaultTableName[NumDefaultTables] =
	{
		"eventsGame",
		"eventsQuality",
	};
	const char * defaultTableDef[NumDefaultTables] =
	{
		"( id INTEGER PRIMARY KEY, time TIMESTAMP DEFAULT (strftime('%Y-%m-%d %H:%M:%S.%f', 'now')), areaId TEXT, eventId TEXT, x REAL, y REAL, z REAL, value REAL, eventData BLOB )",
		"( id INTEGER PRIMARY KEY, time TIMESTAMP DEFAULT (strftime('%Y-%m-%d %H:%M:%S.%f', 'now')), areaId TEXT, eventId TEXT, message TEXT, eventData BLOB )",
	};
	
	for ( int i = 0; i < NumDefaultTables; ++i )
	{
		sqlite3_stmt * statement = NULL;
		if ( SQLITE_OK == CheckSqliteError( sqlite3_prepare_v2( mDatabase, va( "DROP TABLE %s", defaultTableName[i] ), -1, &statement, NULL ) ) )
		{
			CheckSqliteError( sqlite3_step( statement ) );
			CheckSqliteError( sqlite3_finalize( statement ) );
		}

		if ( SQLITE_OK == CheckSqliteError( sqlite3_prepare_v2( mDatabase, va( "CREATE TABLE %s %s", defaultTableName[i], defaultTableDef[i] ), -1, &statement, NULL ) ) )
		{
			CheckSqliteError( sqlite3_step( statement ) );
			CheckSqliteError( sqlite3_finalize( statement ) );
		}
	}

	// make a unique table for each event type
	mMsgSubtypes = Analytics::MessageUnion::descriptor()->FindOneofByName( "msg" );

	for ( int i = 0; i < mMsgSubtypes->field_count(); ++i )
	{
		const google::protobuf::FieldDescriptor* fdesc = mMsgSubtypes->field( i );

		sqlite3_stmt * statement = NULL;
		if ( SQLITE_OK == CheckSqliteError( sqlite3_prepare_v2( mDatabase, va( "DROP TABLE %s", fdesc->camelcase_name().c_str() ), -1, &statement, NULL ) ) )
		{
			CheckSqliteError( sqlite3_step( statement ) );
			CheckSqliteError( sqlite3_finalize( statement ) );
		}

		if ( SQLITE_OK == CheckSqliteError( sqlite3_prepare_v2( mDatabase, va( "CREATE TABLE %s( id INTEGER PRIMARY KEY, key TEXT, data BLOB )", fdesc->camelcase_name().c_str() ), -1, &statement, NULL ) ) )
		{
			CheckSqliteError( sqlite3_step( statement ) );
			CheckSqliteError( sqlite3_finalize( statement ) );
		}
	}

	// todo: http://www.sqlite.org/rtree.html

	return true;
}

int GameAnalytics::CheckSqliteError( int errcode )
{
	switch( errcode )
	{
	case SQLITE_OK:
	case SQLITE_ROW:
	case SQLITE_DONE:
		break;
	default:
		{
			const int excode = sqlite3_extended_errcode( mDatabase );
			const char * exerr = sqlite3_errstr( excode );

			std::string str = va( "sqlite3 error: %s%s%s", 
				exerr ? exerr : "",
				exerr ? "\n" : "",
				sqlite3_errmsg( mDatabase ) );
			
			mErrors.push_back( str.c_str() );
		}
	}
	return errcode;
}

void GameAnalytics::CloseDatabase()
{
	sqlite3_close_v2( mDatabase );
}

void GameAnalytics::AddGameEvent( const char * areaId, const char * eventId )
{
	if ( mDatabase != NULL && areaId != NULL && eventId != NULL )
	{
		sqlite3_stmt * statement = NULL;
		if ( SQLITE_OK == CheckSqliteError( sqlite3_prepare_v2( mDatabase, "INSERT into eventsGame ( areaId, eventId ) VALUES( ?, ? )", -1, &statement, NULL ) ) )
		{
			sqlite3_bind_text( statement, 1, areaId, strlen( areaId ), NULL );
			sqlite3_bind_text( statement, 2, eventId, strlen( eventId ), NULL );

			if ( CheckSqliteError( sqlite3_step(statement) ) == SQLITE_DONE )
			{

			}
		}
		CheckSqliteError( sqlite3_finalize( statement ) );
	}
}

void GameAnalytics::AddGameEvent( const char * areaId, const char * eventId, float value )
{
	if ( mDatabase != NULL && areaId != NULL && eventId != NULL )
	{
		sqlite3_stmt * statement = NULL;
		if ( SQLITE_OK == CheckSqliteError( sqlite3_prepare_v2( mDatabase, "INSERT into eventsGame ( areaId, eventId, value ) VALUES( ?, ?, ? )", -1, &statement, NULL ) ) )
		{
			sqlite3_bind_text( statement, 1, areaId, strlen( areaId ), NULL );
			sqlite3_bind_text( statement, 2, eventId, strlen( eventId ), NULL );
			sqlite3_bind_double( statement, 3, value );

			if ( CheckSqliteError( sqlite3_step(statement) ) == SQLITE_DONE )
			{

			}
		}
		CheckSqliteError( sqlite3_finalize( statement ) );
	}
}

void GameAnalytics::AddGameEvent( const char * areaId, const char * eventId, const float * xyz )
{
	if ( mDatabase != NULL && areaId != NULL && eventId != NULL && xyz != NULL )
	{
		sqlite3_stmt * statement = NULL;
		if ( SQLITE_OK == CheckSqliteError( sqlite3_prepare_v2( mDatabase, "INSERT into eventsGame ( areaId, eventId, x, y, z ) VALUES( ?, ?, ?, ?, ? )", -1, &statement, NULL ) ) )
		{
			sqlite3_bind_text( statement, 1, areaId, strlen( areaId ), NULL );
			sqlite3_bind_text( statement, 2, eventId, strlen( eventId ), NULL );
			sqlite3_bind_double( statement, 3, xyz[ 0 ] );
			sqlite3_bind_double( statement, 4, xyz[ 1 ] );
			sqlite3_bind_double( statement, 5, xyz[ 2 ] );

			if ( CheckSqliteError( sqlite3_step(statement) ) == SQLITE_DONE )
			{

			}
		}
		CheckSqliteError( sqlite3_finalize( statement ) );
	}
}

void GameAnalytics::AddGameEvent( const char * areaId, const char * eventId, const float * xyz, float value )
{
	if ( mDatabase != NULL && areaId != NULL && eventId != NULL && xyz != NULL )
	{
		sqlite3_stmt * statement = NULL;
		if ( SQLITE_OK == CheckSqliteError( sqlite3_prepare_v2( mDatabase, "INSERT into eventsGame ( areaId, eventId, x, y, z, value ) VALUES( ?, ?, ?, ?, ?, ? )", -1, &statement, NULL ) ) )
		{
			sqlite3_bind_text( statement, 1, areaId, strlen( areaId ), NULL );
			sqlite3_bind_text( statement, 2, eventId, strlen( eventId ), NULL );
			sqlite3_bind_double( statement, 3, xyz[ 0 ] );
			sqlite3_bind_double( statement, 4, xyz[ 1 ] );
			sqlite3_bind_double( statement, 5, xyz[ 2 ] );
			sqlite3_bind_double( statement, 6, value );

			if ( CheckSqliteError( sqlite3_step(statement) ) == SQLITE_DONE )
			{

			}
		}
		CheckSqliteError( sqlite3_finalize( statement ) );
	}
}


void GameAnalytics::AddQualityEvent( const char * areaId, const char * eventId )
{
	if ( mDatabase != NULL && areaId != NULL && eventId != NULL )
	{
		sqlite3_stmt * statement = NULL;
		if ( SQLITE_OK == CheckSqliteError( sqlite3_prepare_v2( mDatabase, "INSERT into eventsQuality ( areaId, eventId ) VALUES( ?, ? )", -1, &statement, NULL ) ) )
		{
			sqlite3_bind_text( statement, 1, areaId, strlen( areaId ), NULL );
			sqlite3_bind_text( statement, 2, eventId, strlen( eventId ), NULL );

			if ( CheckSqliteError( sqlite3_step(statement) ) == SQLITE_DONE )
			{

			}
		}
		CheckSqliteError( sqlite3_finalize( statement ) );
	}
}

void GameAnalytics::AddQualityEvent( const char * areaId, const char * eventId, const char * messageId )
{
	if ( mDatabase != NULL && areaId != NULL && eventId != NULL && messageId != NULL )
	{
		sqlite3_stmt * statement = NULL;
		if ( SQLITE_OK == CheckSqliteError( sqlite3_prepare_v2( mDatabase, "INSERT into eventsQuality ( areaId, eventId, message ) VALUES( ?, ?, ? )", -1, &statement, NULL ) ) )
		{
			sqlite3_bind_text( statement, 1, areaId, strlen( areaId ), NULL );
			sqlite3_bind_text( statement, 2, eventId, strlen( eventId ), NULL );
			sqlite3_bind_text( statement, 3, messageId, strlen( messageId ), NULL );

			if ( CheckSqliteError( sqlite3_step(statement) ) == SQLITE_DONE )
			{

			}
		}
		CheckSqliteError( sqlite3_finalize( statement ) );
	}
}

void GameAnalytics::WriteHeatmapScript( const HeatmapDef & def, std::string & scriptContents )
{
	scriptContents.clear();

	if ( mDatabase != NULL )
	{
		const double worldSizeX = ceil( def.mWorldMaxs[ 0 ] - def.mWorldMins[ 0 ] );
		const double worldSizeY = ceil( def.mWorldMaxs[ 1 ] - def.mWorldMins[ 1 ] );
		const double aspect = (double)worldSizeX / (double)worldSizeY;

		const double worldCenterX = ( def.mWorldMaxs[ 0 ] + def.mWorldMins[ 0 ] ) * 0.5;
		const double worldCenterY = ( def.mWorldMaxs[ 1 ] + def.mWorldMins[ 1 ] ) * 0.5;

		int imageX = 0;
		int imageY = 0;
		if ( worldSizeX > worldSizeY )
		{
			imageX = def.mImageSize;
			imageY = (int)ceil( (double)def.mImageSize / aspect );
		}
		else
		{
			imageX = (int)(aspect * def.mImageSize);
			imageY = def.mImageSize;
		}

		const double scaleX = (double)imageX / worldSizeX;
		const double scaleY = (double)imageY / worldSizeY;

		std::stringstream str;
		str << "-gravity center" << std::endl;
		str << va( "-print 'Generating Image Buffer(%d x %d)\\n'", imageX, imageY ) << std::endl;
		str << va( "-size %dx%d", imageX, imageY ) << std::endl;
		str << "xc:black" << std::endl;
		str << "-print 'Rasterizing $(NUMEVENTS) Events\\n'" << std::endl;
		str << std::endl;
		
		int progressCounter = 0;
		const int progressMarker = 1000;

		sqlite3_stmt * statement = NULL;
		if ( SQLITE_OK == CheckSqliteError( sqlite3_prepare_v2( mDatabase, 
			"SELECT x, y, z, value FROM eventsGame WHERE ( areaId=? AND eventId=? AND x NOT NULL )", 
			-1, &statement, NULL ) ) )
		{
			sqlite3_bind_text( statement, 1, def.mAreaId, strlen( def.mAreaId ), NULL );
			sqlite3_bind_text( statement, 2, def.mEventId, strlen( def.mEventId ), NULL );

			while ( SQLITE_ROW == CheckSqliteError( sqlite3_step( statement ) ) )
			{
				if ( ( progressCounter % progressMarker ) == 0 )
					str << va( "-print 'Generating Event Gradients %d...\\n'", progressCounter ) << std::endl;
				++progressCounter;
				
				const double resultX = sqlite3_column_double( statement, 0 ) - worldCenterX;
				const double resultY = sqlite3_column_double( statement, 1 ) - worldCenterY;
				const double resultZ = sqlite3_column_double( statement, 2 );
				const double value = sqlite3_column_double( statement, 3 );

				const float multiplier = 1.0f;

				str << va( "( -size %dx%d radial-gradient: -evaluate Multiply %g ) -geometry +%d+%d -compose Plus -composite",
					(int)(def.mEventRadius * scaleX), 
					(int)(def.mEventRadius * scaleY),
					multiplier,
					(int)(resultX * scaleX),
					(int)(resultY * scaleY) ) << std::endl;
			}
		}
		if ( progressCounter == 0 )
			str << va( "-print 'No Events.\\n'", progressCounter ) << std::endl;

		CheckSqliteError( sqlite3_finalize( statement ) );

		str << std::endl;
		str << "-print 'Colorizing Heatmap\\n'" << std::endl;

		str << "-auto-level" << std::endl;
		str << "(" << std::endl;
		str << "	-size 1x250" << std::endl;
		str << "	gradient:#004-#804" << std::endl;
		str << "	gradient:#804-#f00" << std::endl;
		str << "	gradient:#f00-#f80" << std::endl;
		str << "	gradient:#f80-#ff0" << std::endl;
		str << "	gradient:#ff0-#ffa" << std::endl;
		str << "	-append" << std::endl;
		str << ") -clut" << std::endl;
		
		str << "-print 'Writing Heatmap\\n'" << std::endl;
		str << "-write " << va( "heatmap_%s_%s.png", def.mAreaId, def.mEventId ) << std::endl;
		str << "-exit" << std::endl;

		scriptContents = str.str();
		
		// replace variables
		const std::string varNumEvents = "$(NUMEVENTS)";
		const size_t p = scriptContents.find( varNumEvents );
		if ( p != std::string::npos )
		{
			scriptContents.replace( scriptContents.begin()+p,scriptContents.begin()+p+varNumEvents.length(), va( "%d", progressCounter ) );
		}
	}
}

void GameAnalytics::GetUniqueEventNames( std::vector< std::string > & eventNames )
{
	if ( mDatabase )
	{
		sqlite3_stmt * statement = NULL;
		if ( SQLITE_OK == CheckSqliteError( sqlite3_prepare_v2( mDatabase, 
			"SELECT eventId FROM eventsGame", -1, &statement, NULL ) ) )
		{
			while ( SQLITE_ROW == CheckSqliteError( sqlite3_step( statement ) ) )
			{
				const unsigned char * eventName = sqlite3_column_text( statement, 0 );
				if ( eventName != NULL )
				{
					if ( std::find( eventNames.begin(), eventNames.end(), (const char *)eventName ) == eventNames.end() )
						eventNames.push_back( (const char *)eventName );
				}			
			}
		}
	}
}

bool StringFromField( std::string & strOut, const google::protobuf::Message & msg, const google::protobuf::FieldDescriptor* fdesc )
{
	if ( fdesc->is_repeated() )
		return false;

	switch ( fdesc->type() )
	{
		case google::protobuf::FieldDescriptor::TYPE_DOUBLE:
		{
			const double val = msg.GetReflection()->GetBool( msg, fdesc );
			strOut = vaAnalytics( "%f", val );
			break;
		}
		case google::protobuf::FieldDescriptor::TYPE_FLOAT:
		{
			const float val = msg.GetReflection()->GetFloat( msg, fdesc );
			strOut = vaAnalytics( "%f", val );
			break;
		}
		case google::protobuf::FieldDescriptor::TYPE_INT64:
		case google::protobuf::FieldDescriptor::TYPE_FIXED64:
		case google::protobuf::FieldDescriptor::TYPE_SINT64:
		case google::protobuf::FieldDescriptor::TYPE_SFIXED64:
		{
			const google::protobuf::int64 val = msg.GetReflection()->GetInt64( msg, fdesc );
			strOut = vaAnalytics( "%ll", val );
			break;
		}
		case google::protobuf::FieldDescriptor::TYPE_UINT64:
		{
			const google::protobuf::uint64 val = msg.GetReflection()->GetUInt64( msg, fdesc );
			strOut = vaAnalytics( "%ull", val );
			break;
		}		
		case google::protobuf::FieldDescriptor::TYPE_SINT32:
		case google::protobuf::FieldDescriptor::TYPE_FIXED32:
		case google::protobuf::FieldDescriptor::TYPE_SFIXED32:
		{
			const google::protobuf::int32 val = msg.GetReflection()->GetInt32( msg, fdesc );
			strOut = vaAnalytics( "%d", val );
			break;
		}
		case google::protobuf::FieldDescriptor::TYPE_INT32:
		case google::protobuf::FieldDescriptor::TYPE_UINT32:
		{
			const google::protobuf::uint32 val = msg.GetReflection()->GetUInt32( msg, fdesc );
			strOut = vaAnalytics( "%d", val );
			break;
		}
		case google::protobuf::FieldDescriptor::TYPE_BOOL:
		{
			const bool val = msg.GetReflection()->GetBool( msg, fdesc );
			strOut = val ? "1" : "0";
			break;
		}
		case google::protobuf::FieldDescriptor::TYPE_STRING:
		{
			strOut = msg.GetReflection()->GetString( msg, fdesc );
			break;
		}
		case google::protobuf::FieldDescriptor::TYPE_ENUM:
		{
			const google::protobuf::EnumValueDescriptor* eval = msg.GetReflection()->GetEnum( msg, fdesc );
			if ( eval == NULL )
				return false;

			strOut = eval->name();
			break;
		}
		default:
		{
			return false;
		}
	}
	return true;
}

void GameAnalytics::AddEvent( const Analytics::MessageUnion & msg )
{
	const int messageSize = msg.ByteSize();
	
	// construct a network friendly buffer
	const google::protobuf::FieldDescriptor* oneofField = msg.GetReflection()->GetOneofFieldDescriptor( msg, mMsgSubtypes );

	if ( oneofField == NULL || oneofField->message_type() == NULL )
		return;

	const char * msgtype = oneofField->camelcase_name().c_str();

	bool cacheLast = false;
	if ( oneofField->message_type()->options().HasExtension( Analytics::cachelastvalue ) )
		cacheLast = oneofField->message_type()->options().GetExtension( Analytics::cachelastvalue );
		
	if ( cacheLast )
	{
		// figure out the key with which to cache this message, to allow us to cache more than one of the same type message
		std::string cacheKey = oneofField->camelcase_name();
		if ( oneofField->message_type()->options().HasExtension( Analytics::cachekeysuffix ) )
		{
			cacheKey = oneofField->message_type()->options().GetExtension( Analytics::cachekeysuffix );

			const google::protobuf::FieldDescriptor* fdesc = oneofField->message_type()->FindFieldByCamelcaseName( cacheKey );
			if ( fdesc != NULL && !fdesc->is_repeated() )
			{
				const google::protobuf::Message & oneofMsg = msg.GetReflection()->GetMessage( msg, oneofField );
				
				std::string fieldString;
				StringFromField( fieldString, oneofMsg, fdesc );				
				cacheKey += "_";
				cacheKey += fieldString;
			}
		}

		mMessageCache[ cacheKey ].CopyFrom( msg );
	}
	
	std::string messagePayload;
	messagePayload.reserve( messageSize );

	google::protobuf::io::StringOutputStream str( &messagePayload );
	google::protobuf::io::CodedOutputStream outputstr( &str );

	// todo: compression info?
	msg.SerializeToCodedStream( &outputstr );

	{
		// Save it to the output stream
		sqlite3_stmt * statement = NULL;
		if ( SQLITE_OK == CheckSqliteError( sqlite3_prepare_v2( mDatabase, va( "INSERT into %s ( key, data ) VALUES( ?, ? )", msgtype ), -1, &statement, NULL ) ) )
		{
			sqlite3_bind_text( statement, 1, msgtype, strlen( msgtype ), NULL );
			sqlite3_bind_blob( statement, 2, messagePayload.c_str(), messagePayload.size(), NULL );

			if ( CheckSqliteError( sqlite3_step( statement ) ) == SQLITE_DONE )
			{
			}
		}
		CheckSqliteError( sqlite3_finalize( statement ) );
	}
	
	if ( mPublisher != NULL )
	{
		mPublisher->Publish( oneofField->camelcase_name().c_str(), msg, messagePayload );
	}
}

//////////////////////////////////////////////////////////////////////////

bool GameAnalytics::Poll( Analytics::MessageUnion & msgOut )
{
	if ( mPublisher != NULL )
	{
		while ( mPublisher->Poll( msgOut ) )
		{
			if ( msgOut.has_topicsubscribe() )
			{
				for ( MsgCache::iterator it = mMessageCache.begin(); it != mMessageCache.end(); ++it )
				{
					if ( msgOut.topicsubscribe().topic().empty() || it->first.find( msgOut.topicsubscribe().topic() ) == 0 )
					{
						const int messageSize = it->second.ByteSize();

						const google::protobuf::FieldDescriptor* oneofField = it->second.GetReflection()->GetOneofFieldDescriptor( it->second, mMsgSubtypes );

						std::string messagePayload;
						messagePayload.reserve( messageSize );

						google::protobuf::io::StringOutputStream str( &messagePayload );
						google::protobuf::io::CodedOutputStream outputstr( &str );

						// todo: compression info?
						it->second.SerializeToCodedStream( &outputstr );

						mPublisher->Publish( oneofField->camelcase_name().c_str(), it->second, messagePayload );
					}
				}
				continue;
			}
			else if ( msgOut.has_topicunsubscribe() )
			{
				continue;
			}
			else
			{
				return true;
			}
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////

zmqPublisher::zmqPublisher( const char * ipAddr, unsigned short port )
	: mContext( new zmq::context_t( 1 ) )
	, mSocketPub( NULL )
	, mSocketSub( NULL )
{
	/*mSocketSub = new zmq::socket_t( *mCtx, ZMQ_SUB );
	mSocketSub->bind( vaAnalytics( "tcp://%s:%d", ipAddr, port ) );
	mSocketSub->setsockopt( ZMQ_SUBSCRIBE, "", 0 );*/

	mSocketPub = new zmq::socket_t( *mContext, ZMQ_XPUB );
	mSocketPub->bind( vaAnalytics( "tcp://%s:%d", ipAddr, port ) );
}

//////////////////////////////////////////////////////////////////////////

zmqPublisher::zmqPublisher( zmq::context_t & context, const char * ipAddr, unsigned short port )
	: mContext( NULL )
	, mSocketPub( NULL )
	, mSocketSub( NULL )
{
	/*mSocketSub = new zmq::socket_t( *mCtx, ZMQ_SUB );
	mSocketSub->bind( vaAnalytics( "tcp://%s:%d", ipAddr, port ) );
	mSocketSub->setsockopt( ZMQ_SUBSCRIBE, "", 0 );*/

	mSocketPub = new zmq::socket_t( context, ZMQ_XPUB );
	mSocketPub->bind( vaAnalytics( "tcp://%s:%d", ipAddr, port ) );
}

zmqPublisher::~zmqPublisher()
{
	zmq_close( mSocketPub );
}

void zmqPublisher::Publish( const char * topic, const Analytics::MessageUnion & msg, const std::string & payload )
{
	zmq::message_t ztopic( strlen( topic ) );
	strncpy( (char*)ztopic.data(), topic, ztopic.size() );

	zmq::message_t zmsg( payload.size() );
	memcpy( zmsg.data(), payload.c_str(), payload.size() );
	mSocketPub->send( ztopic, ZMQ_SNDMORE );
	mSocketPub->send( zmsg );
}

bool zmqPublisher::Poll( Analytics::MessageUnion & msgOut )
{
	using namespace google::protobuf;

	zmq::message_t zmsg;
	if ( mSocketPub->recv( &zmsg, ZMQ_DONTWAIT ) )
	{	
		enum Command
		{
			CMD_UNSUBSCRIBE = 0,
			CMD_SUBSCRIBE = 1,
		};
		
		const size_t sz = zmsg.size();
		// these messages are subscription messages
		const Command sub = (Command)( *(char*)zmsg.data() );

		if ( sub == CMD_SUBSCRIBE )
		{
			msgOut.mutable_topicsubscribe()->mutable_topic()->assign( (char*)zmsg.data() + 1, zmsg.size() - 1 );
		}
		else
		{
			msgOut.mutable_topicunsubscribe()->mutable_topic()->assign( (char*)zmsg.data() + 1, zmsg.size() - 1 );
		}
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////

zmqSubscriber::zmqSubscriber( zmq::context_t & context, const char * ipAddr, unsigned short port )
{
	mSocketSub = new zmq::socket_t( context, ZMQ_SUB );
	mSocketSub->connect( vaAnalytics( "tcp://%s:%d", ipAddr, port ) );
}

zmqSubscriber::~zmqSubscriber()
{
	zmq_close( mSocketSub );
}

void zmqSubscriber::Subscribe( const char * topic )
{
	mSocketSub->setsockopt( ZMQ_SUBSCRIBE, topic, strlen( topic ) );
}

bool zmqSubscriber::Poll( Analytics::MessageUnion & msgOut )
{
	using namespace google::protobuf;

	zmq::message_t zmsg;
	if ( mSocketSub->recv( &zmsg, ZMQ_DONTWAIT ) )
	{
		const std::string topic( (char*)zmsg.data(), zmsg.size() );
		
		if ( zmsg.more() )
		{
			if ( mSocketSub->recv( &zmsg, ZMQ_DONTWAIT ) )
			{
				io::ArrayInputStream arraystr( zmsg.data(), zmsg.size() );
				io::CodedInputStream inputstream( &arraystr );

				msgOut.Clear();
				if ( msgOut.ParseFromCodedStream( &inputstream ) )
				{

				}
			}
		}

		return true;
	}
	return false;
}
