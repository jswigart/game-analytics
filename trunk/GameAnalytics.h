#ifndef GAMEANALYTICS_H
#define GAMEANALYTICS_H

#include <map>
#include <string>
#include <vector>
#include <queue>

//#include "json\json.h"

#include "analytics.pb.h"

class GameAnalyticsLogger;
struct sqlite3;
struct _ENetHost;

//////////////////////////////////////////////////////////////////////////

struct GameAnalyticsKeys
{
	std::string		mGameKey;
	std::string		mSecretKey;
	std::string		mDataApiKey;
	std::string		mVersionKey;
};

//////////////////////////////////////////////////////////////////////////

class GameAnalyticsHeatmap
{
public:
	struct HeatEvent
	{
		float				mXYZ[3];
		float				mValue;
	};

	typedef std::vector<HeatEvent> HeatEvents;

	HeatEvents	mEvents;
};

//////////////////////////////////////////////////////////////////////////

// The main class for logging and sending events.
class GameAnalyticsLogger
{
public:
    GameAnalyticsLogger( const GameAnalyticsKeys & keys );

	enum NetChannel
	{
		CHANNEL_EVENT_STREAM,
		CHANNEL_DATA_STREAM,
		CHANNEL_COUNT,
	};

	void AddEvent( const Analytics::MessageUnion & msg );

	void AddGameEvent( const char * areaId, const char * eventId );
	void AddGameEvent( const char * areaId, const char * eventId, float value );
	void AddGameEvent( const char * areaId, const char * eventId, const float * xyz );
	void AddGameEvent( const char * areaId, const char * eventId, const float * xyz, float value );
	
	void AddQualityEvent( const char * areaId, const char * eventId );
	void AddQualityEvent( const char * areaId, const char * eventId, const char * messageId );

	size_t SubmitDesignEvents();
	size_t SubmitQualityEvents();

	const GameAnalyticsHeatmap * GetHeatmap(const std::string & area, const std::string & eventIds, bool loadFromServer);

	bool CreateDatabase( const char * filename );
	void CloseDatabase();

	void AddModel( const char * modelName, const std::string & data, int lod = 0 );
	//void AddEntity( const int id, const Json::Value & properties );

	struct HeatmapDef
	{
		const char *	mAreaId;
		const char *	mEventId;
		float			mEventRadius;
		int				mImageSize;
		float			mWorldMins[2];
		float			mWorldMaxs[2];
	};
	void WriteHeatmapScript( const HeatmapDef & def, std::string & scriptContents );

	void GetUniqueEventNames( std::vector< std::string > & eventNames );

	bool GetError( std::string & errorOut );

	// Network
	void InitNetwork();
	void ShutdownNetwork();

	bool IsNetworkActive() const;
	size_t NumClientsConnected() const;
	bool StartHost( const char * ipAddress, unsigned short port );
	bool Connect( const char * ipAddress, unsigned short port );
	void ServiceNetwork( std::vector<Analytics::MessageUnion*> & recievedMessages );
private:
    void SetUserID();
    void SetSessionID();
	
    const GameAnalyticsKeys		mKeys;
    std::string					mUserId;
    std::string					mSessionId;

	typedef std::map<std::string,GameAnalyticsHeatmap*> HeatMapsByName;

	typedef std::queue<std::string> ErrorQueue;

	ErrorQueue					mErrors;

	HeatMapsByName				mHeatMaps;

	sqlite3 *					mDatabase;

	// remote connections
	_ENetHost *					mConnection;

	GameAnalyticsLogger & operator=( const GameAnalyticsLogger & other );

	std::string BuildUrl( const std::string & category );

	int CheckSqliteError( int errcode );
};

#endif
