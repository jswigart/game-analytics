#ifndef GAMEANALYTICS_H
#define GAMEANALYTICS_H

#include <map>
#include <string>
#include <vector>
#include <list>
#include <functional>
#include <thread>
#include <mutex>
#include <queue>

//#include "json\json.h"

#include "analytics.pb.h"
#include "google/protobuf/text_format.h"
#include "google/protobuf/util/json_util.h"

namespace cpp_redis
{
	class client;
	class subscriber;
};

class GameAnalytics;
struct sqlite3;
class RedisDatabase;

//////////////////////////////////////////////////////////////////////////

class GameAnalyticsCallbacks
{
public:
	virtual void AnalyticsInfo(const char * str) = 0;
	virtual void AnalyticsWarn(const char * str) = 0;
	virtual void AnalyticsError(const char * str) = 0;
};

//////////////////////////////////////////////////////////////////////////

// The main class for logging and sending events.
class GameAnalytics
{
public:
	typedef std::function<void(const std::string&, const std::string&)> channel_message_recieved_t;

	struct ChannelData
	{
		std::string		mChannel;
		std::string		mData;
	};
	
	GameAnalytics(GameAnalyticsCallbacks* callbacks);
	~GameAnalytics();
	
	void Subscribe(const std::string& channelName);
	void ProcessPublishedMessages(channel_message_recieved_t exec);

	void AddEvent(const google::protobuf::Message & msg);

	void SendMesh(const std::string& modelName, const Analytics::Mesh& message);
	void SendGameEnum(const google::protobuf::EnumDescriptor* descriptor);

	void EndOfFrame();

	bool OpenDatabase(const char * filename);
	bool CreateDatabase(const char * filename);
	void CloseDatabase();

	bool OpenRedisConnection(const char *ipAddress = "127.0.0.1", int port = 6379);

	struct HeatmapDef
	{
		const char *	mAreaId;
		const char *	mEventId;
		float			mEventRadius;
		int				mImageSize;
		float			mWorldMins[2];
		float			mWorldMaxs[2];
	};
	void WriteHeatmapScript(const HeatmapDef & def, std::string & scriptContents);

	void GetUniqueEventNames(std::vector< std::string > & eventNames);

	static bool AnyFieldSet(const google::protobuf::Message & msg);
private:	
	GameAnalyticsCallbacks*		mCallbacks;
	
	sqlite3 *					mDatabase;
	
	// NEW REDIS
	cpp_redis::client *		mClient;
	cpp_redis::subscriber*	mSubscriber;

	std::mutex				mChannelDataMutex;
	std::queue<ChannelData> mChannelData;

	std::string				mKeySpacePrefix;

	std::string				mScriptSHA_SET;
	std::string				mScriptSHA_RPUSH;
	std::string				mScriptSHA_HMSET;

	GameAnalytics & operator=(const GameAnalytics & other);
	
	int CheckSqliteError(int errcode);
};

class vaAnalytics
{
public:
	const char * c_str() const { return buffer; }
	operator const char *() const { return buffer; }

	vaAnalytics(const char* msg, ...);
protected:
	enum { BufferSize = 1024 };
	char buffer[BufferSize];
};

#endif
