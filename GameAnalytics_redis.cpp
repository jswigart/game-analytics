#include "GameAnalytics_redis.h"

#include "google/protobuf/io/zero_copy_stream_impl.h"
#include "google/protobuf/io/coded_stream.h"

#include "json/json.h"

#include <cpp_redis/cpp_redis>
#include <cpp_redis/core/client.hpp>

#undef GetMessage

//////////////////////////////////////////////////////////////////////////

Json::Value ConstructJsonFromMessage(const google::protobuf::Message & msg);

Json::Value ConstructJsonFromMessage(const google::protobuf::Message & msg, const google::protobuf::FieldDescriptor* fdesc)
{
	if (fdesc->is_repeated())
		return false;

	switch (fdesc->type())
	{
	case google::protobuf::FieldDescriptor::TYPE_DOUBLE:
	{
		const double val = msg.GetReflection()->GetBool(msg, fdesc);
		return Json::Value(val);
	}
	case google::protobuf::FieldDescriptor::TYPE_FLOAT:
	{
		const float val = msg.GetReflection()->GetFloat(msg, fdesc);
		return Json::Value(val);
	}
	case google::protobuf::FieldDescriptor::TYPE_INT64:
	case google::protobuf::FieldDescriptor::TYPE_FIXED64:
	case google::protobuf::FieldDescriptor::TYPE_SINT64:
	case google::protobuf::FieldDescriptor::TYPE_SFIXED64:
	{
		const google::protobuf::int64 val = msg.GetReflection()->GetInt64(msg, fdesc);
		return Json::Value((int)val);
	}
	case google::protobuf::FieldDescriptor::TYPE_UINT64:
	{
		const google::protobuf::uint64 val = msg.GetReflection()->GetUInt64(msg, fdesc);
		return Json::Value((unsigned int)val);
	}
	case google::protobuf::FieldDescriptor::TYPE_SINT32:
	case google::protobuf::FieldDescriptor::TYPE_FIXED32:
	case google::protobuf::FieldDescriptor::TYPE_SFIXED32:
	case google::protobuf::FieldDescriptor::TYPE_INT32:
	{
		const google::protobuf::int32 val = msg.GetReflection()->GetInt32(msg, fdesc);
		return Json::Value(val);
	}
	case google::protobuf::FieldDescriptor::TYPE_UINT32:
	{
		const google::protobuf::uint32 val = msg.GetReflection()->GetUInt32(msg, fdesc);
		return Json::Value(val);
	}
	case google::protobuf::FieldDescriptor::TYPE_BOOL:
	{
		const bool val = msg.GetReflection()->GetBool(msg, fdesc);
		return Json::Value(val);
	}
	case google::protobuf::FieldDescriptor::TYPE_STRING:
	{
		auto val = msg.GetReflection()->GetString(msg, fdesc);
		return Json::Value(val);
	}
	case google::protobuf::FieldDescriptor::TYPE_ENUM:
	{
		const google::protobuf::EnumValueDescriptor* eval = msg.GetReflection()->GetEnum(msg, fdesc);
		if (eval == NULL)
			return false;

		auto val = eval->name();
		return Json::Value(val);
	}
	case google::protobuf::FieldDescriptor::TYPE_MESSAGE:
	{
		const google::protobuf::Message& childmsg = msg.GetReflection()->GetMessage(msg, fdesc);
		return ConstructJsonFromMessage(childmsg);
	}
	case google::protobuf::FieldDescriptor::TYPE_BYTES:
	{
		auto val = msg.GetReflection()->GetString(msg, fdesc);
		return Json::Value(val);
	}
	default:
	{
		return false;
	}
	}
	return true;
}

Json::Value ConstructJsonFromMessage(const google::protobuf::Message & msg)
{
	Json::Value json;

	std::vector<const google::protobuf::FieldDescriptor*> descriptors;
	msg.GetReflection()->ListFields(msg, &descriptors);
	for (size_t f = 0; f < descriptors.size(); ++f)
	{
		Json::Value val = ConstructJsonFromMessage(msg, descriptors[f]);
		if (!val.isNull())
		{
			json[descriptors[f]->name()] = val;
		}
	}

	return json;
}

//////////////////////////////////////////////////////////////////////////

bool StringFromField(std::string & strOut, const google::protobuf::Message & msg, const google::protobuf::FieldDescriptor* fdesc);

//////////////////////////////////////////////////////////////////////////

bool GameAnalytics::OpenRedisConnection(const char *ipAddress, int port)
{
	mClient = new cpp_redis::client();
	try
	{
		mClient->connect(ipAddress, port, [](const std::string& host, std::size_t port, cpp_redis::client::connect_state status)
		{
			if (status == cpp_redis::client::connect_state::dropped)
			{
				// todo: expose logging to api
				std::cout << "client disconnected from " << host << ":" << port << std::endl;
			}
		}, 1000, 30, 2000);

		time_t rawtime;
		time(&rawtime);
		tm * utc = gmtime(&rawtime);

		mKeySpacePrefix = vaAnalytics("eventStream_%d.%d.%d-%d.%d.%d",
			utc->tm_mday, utc->tm_mon, utc->tm_year, utc->tm_hour, utc->tm_min, utc->tm_sec);

		// start the new stream
		std::vector<std::string> values;
		values.push_back(mKeySpacePrefix);
		mClient->rpush("event_streams", values);

		const char* cachedSET = "local l = redis.call('SET', KEYS[1], ARGV[1]) \
		local pubresult = redis.call('PUBLISH', KEYS[1], ARGV[1]) \
		return l";

		const char* cachedRPUSH = "local l = redis.call('RPUSH', KEYS[1], ARGV[1]) \
		local pubresult = redis.call('PUBLISH', KEYS[1], ARGV[1]) \
		return l";

		const char* cachedHMSET = "local l = redis.call('HMSET', KEYS[1], ARGV[1], ARGV[2]) \
		local pubresult = redis.call('PUBLISH', KEYS[1] .. '~' .. ARGV[1], ARGV[2]) \
		return l";

		mClient->script_load(cachedSET, [&](cpp_redis::reply& reply) {
			std::cout << reply << std::endl;
			mScriptSHA_SET = reply.as_string();
		});

		mClient->script_load(cachedRPUSH, [&](cpp_redis::reply& reply) {
			std::cout << reply << std::endl;
			mScriptSHA_RPUSH = reply.as_string();
		});

		mClient->script_load(cachedHMSET, [&](cpp_redis::reply& reply) {
			std::cout << reply << std::endl;
			mScriptSHA_HMSET = reply.as_string();
		});

		// we want to wait until we have hashes for all the script snippets
		// because that's what we are going to use for sending data to redis
		// so that it will both cache that data and also publish it out
		mClient->sync_commit();

		mSubscriber = new cpp_redis::subscriber();
		mSubscriber->connect(ipAddress, port);
		return true;
	}
	catch (const std::exception& e)
	{
		delete mClient;
		mClient = nullptr;

		delete mSubscriber;
		mSubscriber = nullptr;

		return false;
	}
}

RedisDatabase::RedisDatabase(GameAnalytics* analytics, const char *ipAddress, int port = 6379)
	: Analytics(analytics)
{
	mClient = new cpp_redis::client();
	mClient->connect(ipAddress, port, [](const std::string& host, std::size_t port, cpp_redis::client::connect_state status)
	{
		if (status == cpp_redis::client::connect_state::dropped)
		{
			// todo: expose logging to api
			std::cout << "client disconnected from " << host << ":" << port << std::endl;
		}
	});
	
	time_t rawtime;
	time(&rawtime);
	tm * utc = gmtime(&rawtime);

	mKeySpacePrefix = vaAnalytics("eventStream_%d.%d.%d-%d.%d.%d", 
		utc->tm_mday, utc->tm_mon, utc->tm_year, utc->tm_hour, utc->tm_min, utc->tm_sec);
	
	// start the new stream
	std::vector<std::string> values;
	values.push_back(mKeySpacePrefix);
	mClient->rpush("event_streams", values);

	const char* cachedSET = "local l = redis.call('SET', KEYS[1], ARGV[1]) \
		local pubresult = redis.call('PUBLISH', KEYS[1], ARGV[1]) \
		return l";

	const char* cachedRPUSH = "local l = redis.call('RPUSH', KEYS[1], ARGV[1]) \
		local pubresult = redis.call('PUBLISH', KEYS[1], ARGV[1]) \
		return l";

	const char* cachedHMSET = "local l = redis.call('HMSET', KEYS[1], ARGV[1], ARGV[2]) \
		local pubresult = redis.call('PUBLISH', KEYS[1] .. '~' .. ARGV[1], ARGV[2]) \
		return l";
	
	mClient->script_load(cachedSET, [&](cpp_redis::reply& reply) {
		std::cout << reply << std::endl;
		mScriptSHA_SET = reply.as_string();
	});

	mClient->script_load(cachedRPUSH, [&](cpp_redis::reply& reply) {
		std::cout << reply << std::endl;
		mScriptSHA_RPUSH = reply.as_string();
	});

	mClient->script_load(cachedHMSET, [&](cpp_redis::reply& reply) {
		std::cout << reply << std::endl;
		mScriptSHA_HMSET = reply.as_string();
	});
	
	// we want to wait until we have hashes for all the script snippets
	// because that's what we are going to use for sending data to redis
	// so that it will both cache that data and also publish it out
	mClient->sync_commit();

	mSubscriber = new cpp_redis::subscriber();
	mSubscriber->connect(ipAddress, port);
}

RedisDatabase::~RedisDatabase()
{
	mClient->disconnect(true);
	delete mClient;
	mClient = nullptr;

	tacopie::set_default_io_service(nullptr);
}

void RedisDatabase::AddEvent(const google::protobuf::Message& msg, const std::string & payload)
{
	Analytics::RedisKeyType redisKeyType = Analytics::UNKNOWN;
	if (msg.GetDescriptor()->options().HasExtension(Analytics::rediskeytype))
		redisKeyType = msg.GetDescriptor()->options().GetExtension(Analytics::rediskeytype);

	std::string keyName = msg.GetDescriptor()->name();
	if (msg.GetDescriptor()->options().HasExtension(Analytics::rediskeysuffix))
	{
		std::string suffix = msg.GetDescriptor()->options().GetExtension(Analytics::rediskeysuffix);

		const google::protobuf::FieldDescriptor* fdesc = msg.GetDescriptor()->FindFieldByCamelcaseName(suffix);
		if (fdesc != NULL && !fdesc->is_repeated())
		{
			std::string fieldString;
			if (StringFromField(fieldString, msg, fdesc))
			{
				suffix = fieldString;
			}
		}

		keyName += ":" + suffix;
	}

	const std::string eventKey = vaAnalytics("%s:%s", mKeySpacePrefix.c_str(), keyName.c_str()).c_str();
	
	switch (redisKeyType)
	{
	case Analytics::SET:
	case Analytics::UNKNOWN:
	{
		std::vector<std::string> keys, args;
		keys.push_back(eventKey);
		args.push_back(payload);
		mClient->evalsha(mScriptSHA_SET, 1, keys, args);
		break;
	}
	case Analytics::RPUSH:
	{
		std::vector<std::string> keys, args;
		keys.push_back(eventKey);
		args.push_back(payload);
		mClient->evalsha(mScriptSHA_RPUSH, 1, keys, args);
		break;
	}
	case Analytics::HMSET:
	{
		std::string setKey = msg.GetTypeName();

		if (msg.GetDescriptor()->options().HasExtension(Analytics::redishmsetkey))
		{
			setKey = msg.GetDescriptor()->options().GetExtension(Analytics::redishmsetkey);

			const google::protobuf::FieldDescriptor* fdesc = msg.GetDescriptor()->FindFieldByCamelcaseName(setKey);
			if (fdesc != NULL && !fdesc->is_repeated())
			{
				std::string fieldString;
				if (StringFromField(fieldString, msg, fdesc))
				{
					setKey = fieldString;
				}
			}
		}
		
		std::vector<std::string> keys, args;
		keys.push_back(eventKey);
		args.push_back(setKey);
		args.push_back(payload);
		mClient->evalsha(mScriptSHA_HMSET, 1, keys, args);
		break;
	}
	}
}

void RedisDatabase::EndOfFrame()
{
	mClient->commit();
}

void RedisDatabase::Subscribe(const std::string& channelName)
{
	mSubscriber->subscribe(vaAnalytics("%s:%s", mKeySpacePrefix.c_str(), channelName.c_str()).c_str(), [&](const std::string& chan, const std::string& msg)
	{
		std::lock_guard<std::mutex> lock(mChannelDataMutex);

		ChannelData data;
		data.mChannel = chan;
		data.mData = msg;
		mChannelData.push(data);
	});
	mSubscriber->commit();
}

void RedisDatabase::ProcessPublishedMessages(channel_message_recieved_t exec)
{
	std::lock_guard<std::mutex> lock(mChannelDataMutex);
	while (!mChannelData.empty())
	{
		const ChannelData& data = mChannelData.front();
		exec(data.mChannel, data.mData);
		mChannelData.pop();
	}
}
