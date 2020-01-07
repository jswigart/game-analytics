#include "GameAnalytics_zmq.h"

#include "google/protobuf/io/zero_copy_stream_impl.h"
#include "google/protobuf/io/coded_stream.h"

#include <zmq.hpp>

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
	delete mSocketPub;
	delete mSocketSub;
	delete mContext;
}

void zmqPublisher::Publish( const std::string& topic, const google::protobuf::Message& msg, const std::string & payload )
{
	/*zmq::message_t ztopic( strlen( topic ) );
	strncpy( (char*)ztopic.data(), topic, ztopic.size() );

	zmq::message_t zmsg( payload.size() );
	memcpy( zmsg.data(), payload.c_str(), payload.size() );
	mSocketPub->send( ztopic, ZMQ_SNDMORE );
	mSocketPub->send( zmsg );*/
}
/*
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
*/
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
/*
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
				io::ArrayInputStream arraystr( zmsg.data(), (int)zmsg.size() );
				io::CodedInputStream inputstream( &arraystr );

				msgOut.Clear();
				if ( msgOut.ParseFromCodedStream( &inputstream ) )
				{
					return true;
				}
			}
		}
	}
	return false;
}
*/