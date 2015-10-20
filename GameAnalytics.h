#ifndef GAMEANALYTICS_H
#define GAMEANALYTICS_H

#include <map>
#include <string>
#include <vector>
#include <list>

//#include "json\json.h"

#include "analytics.pb.h"

class GameAnalytics;
struct sqlite3;
struct _ENetHost;

namespace zmq
{
	class context_t;
	class socket_t;
};

//////////////////////////////////////////////////////////////////////////

class ErrorCallbacks
{
public:
	virtual ~ErrorCallbacks()
	{
	}

	virtual void Error( const char * str ) = 0;
};

//////////////////////////////////////////////////////////////////////////

class AnalyticsPublisher
{
public:
	AnalyticsPublisher();
	virtual ~AnalyticsPublisher();

	virtual void Publish( const char * topic, const Analytics::MessageUnion & msg, const std::string & payload ) = 0;

	virtual bool Poll( Analytics::MessageUnion & msgOut ) = 0;
};

class AnalyticsSubscriber
{
public:
	AnalyticsSubscriber();
	virtual ~AnalyticsSubscriber();

	virtual void Subscribe( const char * topic ) = 0;
	virtual bool Poll( Analytics::MessageUnion & msgOut ) = 0;
};

//////////////////////////////////////////////////////////////////////////

class zmqPublisher : public AnalyticsPublisher
{
public:
	zmqPublisher( const char * ipAddr, unsigned short port );
	zmqPublisher( zmq::context_t & context, const char * ipAddr, unsigned short port );
	~zmqPublisher();

	virtual void Publish( const char * topic, const Analytics::MessageUnion & msg, const std::string & payload );

	virtual bool Poll( Analytics::MessageUnion & msgOut );
private:
	zmq::context_t *	mContext;
	zmq::socket_t *		mSocketPub;
	zmq::socket_t *		mSocketSub;
};

class zmqSubscriber : public AnalyticsSubscriber
{
public:
	zmqSubscriber( zmq::context_t & context, const char * ipAddr, unsigned short port );
	~zmqSubscriber();

	virtual void Subscribe( const char * topic = "" );
	virtual bool Poll( Analytics::MessageUnion & msgOut );
private:
	zmq::socket_t *		mSocketSub;
};

//////////////////////////////////////////////////////////////////////////

#define SMART_FIELD_SET(m,fieldname,val) \
	if ( m->fieldname() != val ) \
		m->set_##fieldname(val);

//////////////////////////////////////////////////////////////////////////

// The main class for logging and sending events.
class GameAnalytics
{
public:
	struct Keys
	{
		std::string		mGameKey;
		std::string		mSecretKey;
		std::string		mDataApiKey;
		std::string		mVersionKey;
	};

	class Heatmap
	{
	public:
		struct HeatEvent
		{
			float				mXYZ[ 3 ];
			float				mValue;
		};

		typedef std::vector<HeatEvent> HeatEvents;

		HeatEvents	mEvents;
	};

	GameAnalytics( const Keys & keys, ErrorCallbacks* errorCbs );
	~GameAnalytics();

	void SetPublisher( AnalyticsPublisher * publisher );
	AnalyticsPublisher * GetPublisher() const;

	google::protobuf::int64 GetTimeStamp() const;

	void AddEvent( const Analytics::MessageUnion & msg );

	void AddGameEvent( const char * areaId, const char * eventId );
	void AddGameEvent( const char * areaId, const char * eventId, float value );
	void AddGameEvent( const char * areaId, const char * eventId, const float * xyz );
	void AddGameEvent( const char * areaId, const char * eventId, const float * xyz, float value );

	void AddQualityEvent( const char * areaId, const char * eventId );
	void AddQualityEvent( const char * areaId, const char * eventId, const char * messageId );

	bool Poll( Analytics::MessageUnion & msgOut );

	size_t SubmitDesignEvents();
	size_t SubmitQualityEvents();

	const Heatmap * GetHeatmap( const std::string & area, const std::string & eventIds, bool loadFromServer );

	bool OpenDatabase( const char * filename );
	bool CreateDatabase( const char * filename );
	void CloseDatabase();

	struct HeatmapDef
	{
		const char *	mAreaId;
		const char *	mEventId;
		float			mEventRadius;
		int				mImageSize;
		float			mWorldMins[ 2 ];
		float			mWorldMaxs[ 2 ];
	};
	void WriteHeatmapScript( const HeatmapDef & def, std::string & scriptContents );

	void GetUniqueEventNames( std::vector< std::string > & eventNames );
private:
	void SetUserID();
	void SetSessionID();

	const Keys					mKeys;
	std::string					mUserId;
	std::string					mSessionId;

	typedef std::map<std::string, Heatmap*> HeatMapsByName;

	typedef std::map<std::string, Analytics::MessageUnion> MsgCache;

	ErrorCallbacks*				mErrorCallbacks;

	HeatMapsByName				mHeatMaps;

	sqlite3 *					mDatabase;

	AnalyticsPublisher *		mPublisher;

	MsgCache					mMessageCache;

	const google::protobuf::OneofDescriptor* mMsgSubtypes;

	GameAnalytics & operator=( const GameAnalytics & other );

	std::string BuildUrl( const std::string & category );

	int CheckSqliteError( int errcode );
};

#endif
