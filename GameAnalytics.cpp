#include "GameAnalytics.h"
#include <sstream>
#include <fstream>
#include <set>
#include <cctype>
#include <algorithm> // std::remove

#include "sqlite3.h"

#define CURL_ENABLE 1

#if(CURL_ENABLE)
#include "curl\curl.h" // libcurl, for sending http requests
#endif

#include "MD5.h"  // MD5 hashing algorithm

#include "google/protobuf/io/zero_copy_stream_impl.h"
#include "google/protobuf/io/coded_stream.h"
#include "google/protobuf/util/json_util.h"

#include <cpp_redis/cpp_redis>
#include <cpp_redis/core/client.hpp>

#undef GetMessage

// todo: CURLOPT_PROGRESSFUNCTION

// Callback to take returned data and convert it to a string.
// The last parameter is actually user-defined - it can be whatever
//   type you want it to be.
static size_t DataToString(char *data, size_t size, size_t nmemb, std::string * result)
{
	size_t count = 0;
	if (result)
	{
		// Data is in a character array, so just append it to the string.
		result->append(data, size * nmemb);
		count = size * nmemb;
	}
	return count;
}

vaAnalytics::vaAnalytics(const char* msg, ...)
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

GameAnalytics::GameAnalytics(GameAnalyticsCallbacks* callbacks)
	: mDatabase(NULL)
	, mCallbacks(callbacks)
	, mClient(nullptr)
	, mSubscriber(nullptr)
{
}

//////////////////////////////////////////////////////////////////////////

GameAnalytics::~GameAnalytics()
{
	CloseDatabase();

	if (mClient != nullptr)
	{
		mClient->disconnect(true);
		delete mClient;
		mClient = nullptr;
	}

	tacopie::set_default_io_service(nullptr);
}

bool GameAnalytics::OpenDatabase(const char * filename)
{
	std::string filepath = "file:";
	filepath += filename;

	if (CheckSqliteError(sqlite3_open_v2(filepath.c_str(), &mDatabase, SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE | SQLITE_OPEN_URI, NULL)) != SQLITE_OK)
		return false;

	CheckSqliteError(sqlite3_exec(mDatabase, "VACUUM", 0, 0, 0));

	return true;
}

bool GameAnalytics::CreateDatabase(const char * filename)
{
	std::string filepath = "file:";
	filepath += filename;

	if (CheckSqliteError(sqlite3_open_v2(filepath.c_str(), &mDatabase, SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE | SQLITE_OPEN_URI, NULL)) != SQLITE_OK)
		return false;

	CheckSqliteError(sqlite3_exec(mDatabase, "PRAGMA synchronous=OFF", NULL, NULL, NULL));
	CheckSqliteError(sqlite3_exec(mDatabase, "PRAGMA journal_mode=MEMORY", NULL, NULL, NULL));
	CheckSqliteError(sqlite3_exec(mDatabase, "PRAGMA temp_store=MEMORY", NULL, NULL, NULL));

	/*enum { NumDefaultTables = 2 };
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
	}*/

	CheckSqliteError(sqlite3_exec(mDatabase, "VACUUM", 0, 0, 0));

	// todo: replace with a pre-register for message types
	/*for (int i = 0; i < mMsgSubtypes->field_count(); ++i)
	{
		const google::protobuf::FieldDescriptor* fdesc = mMsgSubtypes->field(i);

		sqlite3_stmt * statement = NULL;
		if (SQLITE_OK == CheckSqliteError(sqlite3_prepare_v2(mDatabase, vaAnalytics("DROP TABLE %s", fdesc->camelcase_name().c_str()), -1, &statement, NULL)))
		{
			CheckSqliteError(sqlite3_step(statement));
			CheckSqliteError(sqlite3_finalize(statement));
		}

		if (SQLITE_OK == CheckSqliteError(sqlite3_prepare_v2(mDatabase, vaAnalytics("CREATE TABLE %s( id INTEGER PRIMARY KEY, timestamp INTEGER KEY, key TEXT, data BLOB )", fdesc->camelcase_name().c_str()), -1, &statement, NULL)))
		{
			CheckSqliteError(sqlite3_step(statement));
			CheckSqliteError(sqlite3_finalize(statement));
		}
	}*/

	// todo: http://www.sqlite.org/rtree.html

	return true;
}

int GameAnalytics::CheckSqliteError(int errcode)
{
	switch (errcode)
	{
	case SQLITE_OK:
	case SQLITE_ROW:
	case SQLITE_DONE:
		break;
	default:
	{
		const int excode = sqlite3_extended_errcode(mDatabase);
		const char * exerr = sqlite3_errstr(excode);

		std::string str = vaAnalytics("sqlite3 error: %s%s%s",
			exerr ? exerr : "",
			exerr ? "\n" : "",
			sqlite3_errmsg(mDatabase));

		if (mCallbacks)
			mCallbacks->AnalyticsError(str.c_str());
	}
	}
	return errcode;
}

void GameAnalytics::CloseDatabase()
{
	sqlite3_close_v2(mDatabase);
}

void GameAnalytics::WriteHeatmapScript(const HeatmapDef & def, std::string & scriptContents)
{
	scriptContents.clear();
#if(0)
	if (mDatabase != NULL)
	{
		const double worldSizeX = ceil(def.mWorldMaxs[0] - def.mWorldMins[0]);
		const double worldSizeY = ceil(def.mWorldMaxs[1] - def.mWorldMins[1]);
		const double aspect = (double)worldSizeX / (double)worldSizeY;

		const double worldCenterX = (def.mWorldMaxs[0] + def.mWorldMins[0]) * 0.5;
		const double worldCenterY = (def.mWorldMaxs[1] + def.mWorldMins[1]) * 0.5;

		int imageX = 0;
		int imageY = 0;
		if (worldSizeX > worldSizeY)
		{
			imageX = def.mImageSize;
			imageY = (int)ceil((double)def.mImageSize / aspect);
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
		str << va("-print 'Generating Image Buffer(%d x %d)\\n'", imageX, imageY) << std::endl;
		str << va("-size %dx%d", imageX, imageY) << std::endl;
		str << "xc:black" << std::endl;
		str << "-print 'Rasterizing $(NUMEVENTS) Events\\n'" << std::endl;
		str << std::endl;

		int progressCounter = 0;
		const int progressMarker = 1000;

		sqlite3_stmt * statement = NULL;
		if (SQLITE_OK == CheckSqliteError(sqlite3_prepare_v2(mDatabase,
			"SELECT x, y, z, value FROM eventsGame WHERE ( areaId=? AND eventId=? AND x NOT NULL )",
			-1, &statement, NULL)))
		{
			sqlite3_bind_text(statement, 1, def.mAreaId, strlen(def.mAreaId), NULL);
			sqlite3_bind_text(statement, 2, def.mEventId, strlen(def.mEventId), NULL);

			while (SQLITE_ROW == CheckSqliteError(sqlite3_step(statement)))
			{
				if ((progressCounter % progressMarker) == 0)
					str << va("-print 'Generating Event Gradients %d...\\n'", progressCounter) << std::endl;
				++progressCounter;

				const double resultX = sqlite3_column_double(statement, 0) - worldCenterX;
				const double resultY = sqlite3_column_double(statement, 1) - worldCenterY;
				const double resultZ = sqlite3_column_double(statement, 2);
				const double value = sqlite3_column_double(statement, 3);

				const float multiplier = 1.0f;

				str << va("( -size %dx%d radial-gradient: -evaluate Multiply %g ) -geometry +%d+%d -compose Plus -composite",
					(int)(def.mEventRadius * scaleX),
					(int)(def.mEventRadius * scaleY),
					multiplier,
					(int)(resultX * scaleX),
					(int)(resultY * scaleY)) << std::endl;
			}
		}
		if (progressCounter == 0)
			str << va("-print 'No Events.\\n'", progressCounter) << std::endl;

		CheckSqliteError(sqlite3_finalize(statement));

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
		str << "-write " << va("heatmap_%s_%s.png", def.mAreaId, def.mEventId) << std::endl;
		str << "-exit" << std::endl;

		scriptContents = str.str();

		// replace variables
		const std::string varNumEvents = "$(NUMEVENTS)";
		const size_t p = scriptContents.find(varNumEvents);
		if (p != std::string::npos)
		{
			scriptContents.replace(scriptContents.begin() + p, scriptContents.begin() + p + varNumEvents.length(), va("%d", progressCounter));
		}
	}
#endif
}

void GameAnalytics::GetUniqueEventNames(std::vector< std::string > & eventNames)
{
	if (mDatabase)
	{
		sqlite3_stmt * statement = NULL;
		if (SQLITE_OK == CheckSqliteError(sqlite3_prepare_v2(mDatabase,
			"SELECT eventId FROM eventsGame", -1, &statement, NULL)))
		{
			while (SQLITE_ROW == CheckSqliteError(sqlite3_step(statement)))
			{
				const unsigned char * eventName = sqlite3_column_text(statement, 0);
				if (eventName != NULL)
				{
					if (std::find(eventNames.begin(), eventNames.end(), (const char *)eventName) == eventNames.end())
						eventNames.push_back((const char *)eventName);
				}
			}
		}
	}
}

bool StringFromField(std::string & strOut, const google::protobuf::Message & msg, const google::protobuf::FieldDescriptor* fdesc)
{
	if (fdesc->is_repeated())
		return false;

	switch (fdesc->type())
	{
	case google::protobuf::FieldDescriptor::TYPE_DOUBLE:
	{
		const double val = msg.GetReflection()->GetBool(msg, fdesc);
		strOut = vaAnalytics("%f", val);
		break;
	}
	case google::protobuf::FieldDescriptor::TYPE_FLOAT:
	{
		const float val = msg.GetReflection()->GetFloat(msg, fdesc);
		strOut = vaAnalytics("%f", val);
		break;
	}
	case google::protobuf::FieldDescriptor::TYPE_INT64:
	case google::protobuf::FieldDescriptor::TYPE_FIXED64:
	case google::protobuf::FieldDescriptor::TYPE_SINT64:
	case google::protobuf::FieldDescriptor::TYPE_SFIXED64:
	{
		const google::protobuf::int64 val = msg.GetReflection()->GetInt64(msg, fdesc);
		strOut = vaAnalytics("%ll", val);
		break;
	}
	case google::protobuf::FieldDescriptor::TYPE_UINT64:
	{
		const google::protobuf::uint64 val = msg.GetReflection()->GetUInt64(msg, fdesc);
		strOut = vaAnalytics("%ull", val);
		break;
	}
	case google::protobuf::FieldDescriptor::TYPE_SINT32:
	case google::protobuf::FieldDescriptor::TYPE_FIXED32:
	case google::protobuf::FieldDescriptor::TYPE_SFIXED32:
	case google::protobuf::FieldDescriptor::TYPE_INT32:
	{
		const google::protobuf::uint32 val = msg.GetReflection()->GetInt32(msg, fdesc);
		strOut = vaAnalytics("%d", val);
		break;
	}
	case google::protobuf::FieldDescriptor::TYPE_UINT32:
	{
		const google::protobuf::uint32 val = msg.GetReflection()->GetUInt32(msg, fdesc);
		strOut = vaAnalytics("%d", val);
		break;
	}
	case google::protobuf::FieldDescriptor::TYPE_BOOL:
	{
		const bool val = msg.GetReflection()->GetBool(msg, fdesc);
		strOut = val ? "1" : "0";
		break;
	}
	case google::protobuf::FieldDescriptor::TYPE_STRING:
	{
		strOut = msg.GetReflection()->GetString(msg, fdesc);
		break;
	}
	case google::protobuf::FieldDescriptor::TYPE_ENUM:
	{
		const google::protobuf::EnumValueDescriptor* eval = msg.GetReflection()->GetEnum(msg, fdesc);
		if (eval == NULL)
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

//void GameAnalytics::AddEvent(const google::protobuf::Message& msg)
//{
//	/*
//	std::string cacheKey;
//
//	if (cacheLast)
//	{
//		MsgKeyCache& keyCache = mMessageCache[msg.GetTypeName()];
//
//		// figure out the key with which to cache this message, to allow us to cache more than one of the same type message
//		cacheKey = msg.GetTypeName();
//
//		if (msg.GetDescriptor()->options().HasExtension(Analytics::cachekeysuffix))
//		{
//			cacheKey = msg.GetDescriptor()->options().GetExtension(Analytics::cachekeysuffix);
//
//			const google::protobuf::FieldDescriptor* fdesc = msg.GetDescriptor()->FindFieldByCamelcaseName(cacheKey);
//			if (fdesc != NULL && !fdesc->is_repeated())
//			{
//				std::string fieldString;
//				StringFromField(fieldString, msg, fdesc);
//				cacheKey += "_";
//				cacheKey += fieldString;
//			}
//		}
//		
//		std::shared_ptr<google::protobuf::Message> msgCopy;
//		msgCopy.reset(msg.New());
//		msgCopy->CopyFrom(msg);
//
//		keyCache[cacheKey] = msgCopy;
//	}
//
//	std::string messagePayload;
//	messagePayload.reserve(messageSize);
//	
//	google::protobuf::io::StringOutputStream str(&messagePayload);
//	google::protobuf::io::CodedOutputStream outputstr(&str);
//
//	// todo: compression info?
//	msg.SerializeToCodedStream(&outputstr);
//	
//	messagePayload.resize(outputstr.ByteCount());
//
//	if (mDatabase != nullptr)
//	{
//		// Save it to the output stream
//		sqlite3_stmt * statement = NULL;
//		if (SQLITE_OK == CheckSqliteError(sqlite3_prepare_v2(mDatabase, vaAnalytics("INSERT into %s ( key, data ) VALUES( ?, ?, ? )", msgName), -1, &statement, NULL)))
//		{
//			sqlite3_bind_text(statement, 1, msgName.c_str(), (int)msgName.length(), NULL);
//			sqlite3_bind_blob(statement, 2, messagePayload.c_str(), (int)messagePayload.size(), NULL);
//
//			if (CheckSqliteError(sqlite3_step(statement)) == SQLITE_DONE)
//			{
//			}
//		}
//		CheckSqliteError(sqlite3_finalize(statement));
//	}*/
//
//	std::string serializedMsg = msg.SerializeAsString();
//
//	if (mRedisDb != nullptr)
//	{
//		mRedisDb->AddEvent(msg, serializedMsg);
//	}
//
//	/*if (mPublisher != NULL)
//	{
//		mPublisher->Publish(msgName, msg, messagePayload);
//	}*/
//}

//////////////////////////////////////////////////////////////////////////

void GameAnalytics::SendMesh(const std::string& modelName, const Analytics::Mesh& message)
{
	if (mClient == nullptr)
		return;

	std::string cachedFileData;
	if (message.SerializeToString(&cachedFileData))
	{
		Analytics::GameMeshData msg;
		msg.set_modelname(modelName);

		switch (msg.compressiontype())
		{
		case Analytics::Compression_FastLZ:
		{
			// compress the tile data
			/*const size_t bufferSize = cachedFileData.size() + (size_t)(cachedFileData.size() * 0.1);
			boost::shared_array<char> compressBuffer(new char[bufferSize]);
			const int sizeCompressed = fastlz_compress_level(2, cachedFileData.c_str(), cachedFileData.size(), compressBuffer.get());
			msg.mutable_modelbytes()->assign(compressBuffer.get(), sizeCompressed);
			msg.set_modelbytesuncompressed((uint32_t)cachedFileData.size());*/
			break;
		}
		default:
			// no special processing
			msg.set_modelbytes(cachedFileData);
			break;
		}
		
		AddEvent(msg);
	}
}

//////////////////////////////////////////////////////////////////////////

void GameAnalytics::SendGameEnum(const google::protobuf::EnumDescriptor* descriptor)
{
	if (mClient == nullptr)
		return;

	std::string enumName = descriptor->name();
	std::transform(enumName.begin(), enumName.end(), enumName.begin(), [](unsigned char c) { return std::toupper(c); });

	Analytics::GameEnum msg;
	msg.set_enumname(enumName);

	for (int i = 0; i < descriptor->value_count(); ++i)
	{
		auto v = descriptor->value(i);

		auto val = msg.add_values();
		val->set_name(v->name());
		val->set_value(v->index());
	}

	AddEvent(msg);
}

//////////////////////////////////////////////////////////////////////////

void GameAnalytics::AddEvent(const google::protobuf::Message& msg)
{
	if (mClient == nullptr)
		return;

	std::string payload;

	if (msg.GetDescriptor()->options().HasExtension(Analytics::useJsonEncoding))
	{
		google::protobuf::util::MessageToJsonString(msg, &payload);
	}
	else
	{
		payload = msg.SerializeAsString();
	}

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

//////////////////////////////////////////////////////////////////////////

void GameAnalytics::EndOfFrame()
{
	if (mClient != nullptr)
	{
		mClient->commit();
	}
}

bool GameAnalytics::AnyFieldSet(const google::protobuf::Message & msg)
{
	using namespace google;
	const protobuf::Reflection * refl = msg.GetReflection();

	std::vector<const protobuf::FieldDescriptor*> fields;
	refl->ListFields(msg, &fields);
	for (size_t i = 0; i < fields.size(); ++i)
	{
		const protobuf::FieldDescriptor * fieldDesc = fields[i];

		if (fieldDesc->is_extension())
			continue;

		return true;
	}
	return false;
}

void GameAnalytics::Subscribe(const std::string& channelName)
{
	if (mSubscriber != nullptr)
	{
		mSubscriber->subscribe(vaAnalytics("%s:%s", mKeySpacePrefix.c_str(), channelName.c_str()).c_str(), [=](const std::string& chan, const std::string& msg)
		{
			std::lock_guard<std::mutex> lock(mChannelDataMutex);

			ChannelData data;
			data.mChannel = channelName; // not using the keyspace encoded name
			data.mData = msg;
			mChannelData.push(data);
		});
		mSubscriber->commit();
	}
	
}

void GameAnalytics::ProcessPublishedMessages(channel_message_recieved_t exec)
{
	std::lock_guard<std::mutex> lock(mChannelDataMutex);
	while (!mChannelData.empty())
	{
		const ChannelData& data = mChannelData.front();
		exec(data.mChannel, data.mData);
		mChannelData.pop();
	}
}
