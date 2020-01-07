#ifndef GAMEANALYTICS_REDIS_H
#define GAMEANALYTICS_REDIS_H



#include "GameAnalytics.h"

namespace cpp_redis
{
	class client;
	class subscriber;
};

struct ChannelData
{
	std::string		mChannel;
	std::string		mData;
};

//////////////////////////////////////////////////////////////////////////

class RedisDatabase
{
public:
	typedef std::function<void(const std::string&, const std::string&)> channel_message_recieved_t;

	RedisDatabase(GameAnalytics* analytics, const char *ipAddress, int port);
	virtual ~RedisDatabase();

	void AddEvent(const google::protobuf::Message& msg, const std::string & payload);
	
	void Subscribe(const std::string& channelName);
	void ProcessPublishedMessages(channel_message_recieved_t exec);

	virtual void EndOfFrame();
private:
	GameAnalytics *			Analytics;
	cpp_redis::client *		mClient;
	cpp_redis::subscriber*	mSubscriber;

	std::mutex				mChannelDataMutex;
	std::queue<ChannelData> mChannelData;

	std::string				mKeySpacePrefix;

	std::string				mScriptSHA_SET;
	std::string				mScriptSHA_RPUSH;
	std::string				mScriptSHA_HMSET;
};

#endif
