#include "restcomet.h"
#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <map>
#include <boost/regex.hpp>


//DEBUGGING
#include <iostream>

namespace rc
{


void restcomet::SubmitEvent( const string& guid, const string& eventData )
{
	boost::unique_lock<boost::shared_mutex> eventsLock;

	//increment sequence
	m_currentSequence++;
	//insert event
	Event& currentEvent = m_EventBuffer[m_currentSequence % RESTCOMET_EVENT_BUFFER_SIZE];

	currentEvent.sequence = m_currentSequence;
	currentEvent.guid = guid;
	currentEvent.timestamp = time( NULL );
	currentEvent.eventData = eventData;


	//notify all
	m_conditionNewEvent.notify_all();
}


map<string, string> restcomet::DecodePostData( const string& rawPostData )
{
	map<string, string> pairs;
	string spacedPostData = rawPostData;
	boost::regex termRegex( "([^\\&]+?)=([^\\&]+)" );

	for ( int i=0; i < spacedPostData.length(); ++i )
		if ( spacedPostData[i] == '+')
			spacedPostData[i] = ' ';

	boost::match_results<std::string::const_iterator> terms;
	string::const_iterator start, end;
	start = spacedPostData.begin();
	end = spacedPostData.end();

	while ( boost::regex_search( start, end, terms, termRegex, boost::match_default ) )
	{
		if ( terms[2].matched )
			pairs[ string( terms[1].first, terms[1].second ) ] = string( terms[2].first, terms[2].second );
		else
			pairs[ string( terms[1].first, terms[1].second ) ] = "";

		start = terms[0].second;
	}

	for ( map<string, string>::iterator aPair = pairs.begin();
	      aPair != pairs.end();
	      ++aPair )
	{
		ReplacePercentEncoded( aPair->second );
	}

	return pairs;
}

void restcomet::ReplacePercentEncoded( string& workString )
{
	int pos = workString.find( "%" );
	while( pos != string::npos )
	{
		int numPos = pos + 1;
		while ( numPos < workString.length() && isdigit( workString[numPos] ) )
		{
				numPos++;
		}
		
		if ( numPos > pos ) //Yay, a valid code was found
		{
			int value;
			sscanf( workString.substr( pos + 1, numPos - pos ).c_str(), "%x", &value); 
			char translated = (char) value;
			workString.replace( pos, numPos - pos + 1, string( &translated, 1 ).c_str() );
		}
		
		pos = workString.find( "%" );
	}
}

void restcomet::SocketDispatchThreadFunc()
{
	while ( !m_terminated )
	{

		struct sockaddr_in clientAddress;
		unsigned int clientLen = sizeof( clientAddress );
		int clientSock = accept( m_listenSocket, ( struct sockaddr* ) &clientAddress, &clientLen );

		if ( clientSock >= 0 )
		{
			m_connectionHandlerThreadGroup.create_thread( boost::bind( &restcomet::ConnectionHandlerThreadFunc, this, clientSock ) );
		}
	}
}

void restcomet::ConnectionHandlerThreadFunc( int clientSock )
{
	char buf[4096];
	static char success[] =	"HTTP/1.1 200 OK\r\n"
	                        "Content-Type: text/html\r\n\r\n"
	                        "<html><body><h1>restcomet</h1></body></html>";
	static char test[] =	"HTTP/1.1 200 OK\r\n"
	                     "Content-Type: text/html\r\n\r\n"
	                     "<html><body><form method=\"POST\" action=\"\">"
	                     "<input type=\"hidden\" name=\"post1\" value=\"blah\">"
	                     "<input type=\"hidden\" name=\"post2\" value=\"blah&\">"
	                     "CLICK THIS<input type=\"submit\" name=\"submitbutton\" value=\"GO!\">"
	                     "</form></body></html>";
	int recvd = recv( clientSock, buf, 4096, 0 );

	if ( recvd > 0 )
	{
		string asText( buf, recvd );
		std::cout << asText << endl;
		send( clientSock, test, sizeof( test ), 0 );
	}

	close( clientSock );
}

restcomet::restcomet( int port ) : m_currentSequence( 0 ), m_terminated( false )
{

	struct sockaddr_in thisAddress;

	m_listenSocket = socket( PF_INET, SOCK_STREAM, IPPROTO_TCP );

	if ( m_listenSocket == 0 )
		throw runtime_error( "Could not create listener socket" );

	int on = 1;

	if ( setsockopt( m_listenSocket, SOL_SOCKET, SO_REUSEADDR, &on, sizeof( on ) ) < 0 )
		throw runtime_error( "Could not set SO_REUSEADDR on listener socket" );

	memset( &thisAddress, 0, sizeof( thisAddress ) );

	thisAddress.sin_family = AF_INET;                  // Internet/IP

	thisAddress.sin_addr.s_addr = htonl( INADDR_ANY ); // Incoming addr

	thisAddress.sin_port = htons( port );       			// server port

	if ( bind( m_listenSocket, ( struct sockaddr* ) &thisAddress, sizeof( thisAddress ) ) < 0 )
		throw runtime_error( "Could not bind to port for incoming connections" );

	if ( listen( m_listenSocket, 100 ) < 0 )
		throw runtime_error( "Could not listen on socket" );

	m_socketDispatchThread.reset( new boost::thread( boost::bind( &restcomet::SocketDispatchThreadFunc, this ) ) );

}

restcomet::~restcomet()
{
	m_terminated = true;

	if ( m_socketDispatchThread.get() != NULL )
	{
		m_socketDispatchThread->interrupt();
		m_socketDispatchThread->join();
	}

	m_connectionHandlerThreadGroup.interrupt_all();

	m_connectionHandlerThreadGroup.join_all();

	close( m_listenSocket );
}

//
// singleton and non-copyable stuff
//

restcomet* restcomet::ms_instance = 0;

restcomet* restcomet::Instance( int port )
{
	if ( ms_instance == 0 )
	{
		ms_instance = new restcomet( port );
	}

	return ms_instance;
}

void restcomet::Release()
{
	if ( ms_instance )
	{
		delete ms_instance;
	}

	ms_instance = 0;
}

}
