#ifndef GAMEANALYTICS_ZMQ_H
#define GAMEANALYTICS_ZMQ_H

#include "GameAnalytics.h"

namespace zmq
{
	class context_t;
	class socket_t;
};

//////////////////////////////////////////////////////////////////////////

class zmqPublisher //: public AnalyticsPublisher
{
public:
	zmqPublisher( const char * ipAddr, unsigned short port );
	zmqPublisher( zmq::context_t & context, const char * ipAddr, unsigned short port );
	virtual ~zmqPublisher();

	virtual void Publish( const std::string& topic, const google::protobuf::Message& msg, const std::string & payload );

	//virtual bool Poll( Analytics::MessageUnion & msgOut );
private:
	zmq::context_t *	mContext;
	zmq::socket_t *		mSocketPub;
	zmq::socket_t *		mSocketSub;
};

class zmqSubscriber //: public AnalyticsSubscriber
{
public:
	zmqSubscriber( zmq::context_t & context, const char * ipAddr, unsigned short port );
	virtual ~zmqSubscriber();

	virtual void Subscribe( const char * topic = "" );
	//virtual bool Poll( Analytics::MessageUnion & msgOut );
private:
	zmq::socket_t *		mSocketSub;
};

#endif
