#include "Messaging.h"
#include "GameAnalytics.h"

#include "enet/enet.h"

#include "google/protobuf/io/zero_copy_stream_impl.h"
#include "google/protobuf/io/coded_stream.h"
#include "google/protobuf/text_format.h"

struct ScopedPacketDestroy
{
	ScopedPacketDestroy( ENetPacket * packet )
		: mPacket( packet )
	{		
	}
	~ScopedPacketDestroy()
	{
		enet_packet_destroy( mPacket );
	}
private:
	ENetPacket * mPacket;
};

HostThreadENET::HostThreadENET( QObject *parent )
	: QThread( parent )
	, mRunning( false )
{
	// connect message handling to the parent
}

void HostThreadENET::run()
{
	enet_initialize();
	
	// todo: read these in
	const char * hostIp = "127.0.0.1";
	unsigned short hostPort = 5050;

	ENetAddress hostAddr;
	hostAddr.port = hostPort;
	if ( enet_address_set_host( &hostAddr, hostIp ) < 0 )
	{
		emit error( tr( "Problem Setting Host %1:%2" ).arg( hostIp ).arg( hostAddr.port ), QString() );
		return;
	}

	char hostipstr[ 32 ] = {};
	enet_address_get_host_ip( &hostAddr, hostipstr, 32 );

	ENetHost * enetHost = enet_host_create( NULL, 1, CHANNEL_COUNT, 0, 0 );
	if ( enetHost == NULL )
	{
		emit error( tr( "Problem Creating Host %1:%2" ).arg( hostipstr ).arg( hostAddr.port ), QString() );
		return;
	}
	
	//enet_host_compress( enetHost );

	{
		enetHost->peers[ 0 ].address.port = hostPort;
		if ( enet_address_set_host( &enetHost->peers[ 0 ].address, hostIp ) < 0 )
		{
			emit error( tr( "Problem Setting Host %1:%2" ).arg( hostipstr ).arg( hostAddr.port ), QString() );
			return;
		}
	}
	
	using namespace google::protobuf;

	mRunning = true; 

	while ( mRunning )
	{
		for ( int p = 0; p < enetHost->peerCount; ++p )
		{
			if ( enetHost->peers[ p ].state == ENET_PEER_STATE_DISCONNECTED )
			{
				emit info( tr( "Attempting Connection %1:%2" ).arg( hostipstr ).arg( enetHost->peers[ p ].address.port ), QString() );
				if ( enet_host_connect( enetHost, &enetHost->peers[ p ].address, CHANNEL_COUNT, 0 ) == NULL )
				{
					emit warn( tr( "Failed Connecting %1:%2" ).arg( hostipstr ).arg( enetHost->peers[ p ].address.port ), QString() );
				}
			}
		}

		ENetEvent event;
		while ( enet_host_service( enetHost, &event, 2000 ) > 0 )
		{
			QString srcAddr = "<unknown>";

			if ( event.peer != NULL )
			{
				char buffer[ 32 ] = {};
				enet_address_get_host_ip( &event.peer->address, buffer, 32 );
				srcAddr = QString( "%1:%2" ).arg( buffer ).arg( event.peer->address.port );
			}
			
			if ( event.peer != NULL )
			{
				emit status( QString( "Sent: %1, Recvd: %2" )
					.arg( event.peer->outgoingDataTotal )
					.arg( event.peer->incomingDataTotal ) );
			}

			switch ( event.type )
			{
				case ENET_EVENT_TYPE_CONNECT:
				{
					/* Store any relevant client information here. */
					//event.peer->data = "Client information";
					emit debug( tr( "Client Connected %1" ).arg( srcAddr ), QString() );
					break;
				}
				case ENET_EVENT_TYPE_RECEIVE:
				{
					try
					{
						ScopedPacketDestroy spd( event.packet );

						io::ArrayInputStream arraystr( event.packet->data, event.packet->dataLength );
						io::CodedInputStream inputstream( &arraystr );
						
						// hold a base ref in case ownership isnt passed off to a slot it
						// will get deleted at the end of this scope
						MessageUnionPtr msgInstance( new Analytics::MessageUnion() );

						if ( !msgInstance->ParseFromCodedStream( &inputstream ) )
							break;

#if(0)
						std::string strmsg;
						//TextFormat::PrintToString( *msgInstance, &strmsg );
						QString detailInfo( QByteArray( strmsg.c_str(), strmsg.size() ) );

						emit debug( tr( "Got Message %1 size %2" ).arg( typeid( *msgInstance ).name() ).arg( event.packet->dataLength ), detailInfo );
#endif
						// determine the type, is there a better way to do this?
						emit onmsg( msgInstance );
					}
					catch ( const std::exception & e )
					{
						e;
					}
					break;
				}
				case ENET_EVENT_TYPE_DISCONNECT:
				{
					/* Reset the peer's client information. */
					//event.peer->data = NULL;
					emit debug( tr( "Client Disconnected %1" ).arg( srcAddr ), QString() );
					break;
				}
			}
		}
	}
	
	enet_deinitialize();
}

#include <zmq.hpp>

HostThread0MQ::HostThread0MQ( QObject *parent )
	: QThread( parent )
	, mRunning( false )
{

}

void HostThread0MQ::run()
{
	using namespace google::protobuf;
	
	zmq::context_t zmqcontext( 1 );

	zmqSubscriber sub( zmqcontext, "127.0.0.1", 40000 );
	sub.Subscribe( "" );
	
	while ( true )
	{
		Analytics::MessageUnion msg;
		if ( sub.Poll( msg ) )
		{
			MessageUnionPtr msgInstance( new Analytics::MessageUnion() );
			msgInstance->CopyFrom( msg );
			emit onmsg( msgInstance );
		}
		else
		{
			Sleep( 10 );
		}
	}
}
