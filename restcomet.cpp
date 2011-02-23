#include "restcomet.h"
#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/fcntl.h>
#include <signal.h>
#include <map>
#include <sstream>
#include <vector>
#include <boost/regex.hpp>
#include <fcntl.h>
#include <set>
#include <iostream>

using namespace std;

namespace rc
{

void restcomet::SubmitEvent( const string& guid, const string& eventData )
{
	boost::mutex::scoped_lock eventsLock( m_bufferMutex );

	//increment sequence
	m_currentSequence++;
	//insert event
	Event& currentEvent = m_EventBuffer[m_currentSequence % RESTCOMET_EVENT_BUFFER_SIZE];

	currentEvent.sequence = m_currentSequence;
	currentEvent.guid = guid;
	currentEvent.timestamp = time( NULL );
	currentEvent.eventData = eventData;
	write( m_newEventPipes[1], "n", 1 ); //MUST NOT BLOCK
}


map<string, string> restcomet::DecodePostData( const string& rawPostData )
{
	map<string, string> pairs;
	string spacedPostData = rawPostData;
	boost::regex termRegex( "([^\\&]+?)=([^\\&]+)" );

	for ( unsigned int i=0; i < spacedPostData.length(); ++i )
		if ( spacedPostData[i] == '+')
			spacedPostData[i] = ' ';

	boost::match_results<std::string::const_iterator> terms;
	string::const_iterator start, end;
	start = spacedPostData.begin();
	end = spacedPostData.end();

	while ( boost::regex_search( start, end, terms, termRegex, boost::match_default ) ) {
		if ( terms[2].matched )
			pairs[ string( terms[1].first, terms[1].second ) ] = string( terms[2].first, terms[2].second );
		else
			pairs[ string( terms[1].first, terms[1].second ) ] = "";

		start = terms[0].second;
	}

	for ( map<string, string>::iterator aPair = pairs.begin();
	      aPair != pairs.end();
	      ++aPair ) {
		ReplacePercentEncoded( aPair->second );
	}

	return pairs;
}

void restcomet::ReplacePercentEncoded( string& workString )
{
	size_t pos = workString.find( "%" );
	while( pos != string::npos ) {
		size_t numPos = pos + 1;
		while ( numPos < workString.length() && isdigit( workString[numPos] ) ) {
				numPos++;
		}

		if ( numPos > pos ) { //Yay, a valid code was found
			int value;
			sscanf( workString.substr( pos + 1, numPos - pos ).c_str(), "%x", &value);
			char translated = (char) value;
			workString.replace( pos, numPos - pos + 1, string( &translated, 1 ).c_str() );
		}

		pos = workString.find( "%" );
	}
}

string restcomet::GenerateRandomString() throw()
{

	static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
	string retString = "0123456789012345";

	 for (unsigned int i = 0; i < retString.length(); ++i) 
        retString[i] = alphanum[rand() % (sizeof(alphanum) - 1)];

    return retString;
}

string restcomet::SerializeEvents( const string& boundary, const vector<Event>& events )
{
	ostringstream sEvents;

	for ( vector<Event>::const_iterator anEvent = events.begin();
			anEvent != events.end();
			++anEvent) {
		ostringstream eventContent;

		eventContent	<< "S[" << anEvent->sequence
							<< "] G[" << anEvent->guid
							<< "] T[" << anEvent->timestamp
							<< "] L[" << anEvent->eventData.length() << "]\r\n"
							<< anEvent->eventData;

		sEvents 	<< "--" << boundary << "\r\n"
					<< "Content-Type: application/restcomet-event\r\n"
					<< "Content-Length: " << eventContent.str().length() << "\r\n\r\n"
					<< eventContent.str() << "\r\n";
	}
	sEvents << "--" << boundary << "--";

	return sEvents.str();
}

string restcomet::CreateHTTPResponse( const string& codeAndDescription, const string& contentType, const string& body )
{
	ostringstream response;
	char dateBuffer[200];
	time_t t;
	tm * ptm;
	time ( & t );
	ptm = gmtime ( & t );
	strftime( dateBuffer, 200, "%a, %d %b %Y %X GMT", ptm);



	response << "HTTP/1.1 " << codeAndDescription << "\r\n"
					"Date: " << dateBuffer << "\r\n"
					"Pragma: no-cache\r\n"
					"Cache-Control: no-cache\r\n"
					"Server: RestComet\r\n"
					"Access-Control-Allow-Origin: *\r\n"
					"Access-Control-Allow-Methods: POST\r\n"
					"Access-Control-Allow-Headers: x-requested-with, x-restcomet-sequence\r\n"
					"Content-Type: " << contentType << "\r\n"
					"Content-Length: " << body.length() << "\r\n\r\n"
					<< body;
	return response.str();
}
string restcomet::CreateCORSResponse()
{
	ostringstream response;
	char dateBuffer[200];
	time_t t;
	tm * ptm;
	time ( & t );
	ptm = gmtime ( & t );
	strftime( dateBuffer, 200, "%a, %d %b %Y %X GMT", ptm);



	response << "HTTP/1.1 200 OK \r\n"
					"Date: " << dateBuffer << "\r\n"
					"Pragma: no-cache\r\n"
					"Cache-Control: no-cache\r\n"
					"Server: RestComet\r\n"
					"Access-Control-Allow-Origin: *\r\n"
					"Access-Control-Allow-Methods: POST\r\n"
					"Access-Control-Allow-Headers: x-requested-with, x-restcomet-sequence\r\n"
					"Content-Type: text/plain\r\n"
					"Content-Length: 0\r\n\r\n";
		return response.str();
}

void restcomet::SocketThreadFunc()
{

	//int is the socket
	map<int, http_client> clients;

	fd_set readers, writers;
	while ( !m_terminated )	{
		time_t now = time( NULL );
		int fd_max = 0;
		FD_ZERO( &readers ); FD_ZERO( &writers );
		if ( m_listenSocket > m_newEventPipes[0] )
			fd_max = m_listenSocket;
		else
			fd_max = m_newEventPipes[0];

		FD_SET( m_listenSocket, &readers );
		FD_SET( m_newEventPipes[0], &readers );

		for(  map<int, http_client>::iterator client = clients.begin();
				client != clients.end(); )	{
			if ( client->second.state == 2 )
				FD_SET( client->first, &writers );

			if ( client->second.state == 0 && client->second.readStarted + 5 < now ) {
				close( client->first );
				clients.erase( client++ );
				continue;
			}
			else {
				if ( client->first > fd_max )
					fd_max = client->first;
				FD_SET( client->first, &readers );
			}
			++client;
		}

		struct timeval tv;
		tv.tv_sec = 3;
		tv.tv_usec = 0;

		select( fd_max+1, &readers, &writers, NULL, &tv );
		
		if ( FD_ISSET( m_listenSocket, &readers ) ) {
			struct sockaddr_in newClientAddr;
			socklen_t ncalen = sizeof( newClientAddr );
			int newClientSock = accept( m_listenSocket, reinterpret_cast<sockaddr*>(&newClientAddr), &ncalen );
			if ( newClientSock > 0 ) {
				http_client client;
				clients[newClientSock] = client;
			}
		}

		//Is there new events to submit?
		if ( FD_ISSET( m_newEventPipes[0], &readers ) )	{
			char unused[1024];
			boost::mutex::scoped_lock eventsLock( m_bufferMutex );

			//Flush event pipe
			while ( read( m_newEventPipes[0], &unused, 1024 ) > 0 ) {}

			for(  map<int, http_client>::iterator client = clients.begin();
					client != clients.end(); ++client) {
				//skip unless waiting for events
				if ( client->second.state != 1 )
					continue;

				CheckClientEvents( client->second );
			}
		}

		for(  map<int, http_client>::iterator client = clients.begin();
				client != clients.end(); )	{
			try {
				if ( FD_ISSET( client->first, &readers ) ) {
					char buffer[2048];

					ssize_t recvd = recv( client->first, buffer, 2048, (int) NULL );
					if ( recvd > 0 ) {
						if ( client->second.state == 0 )	{
							//Limit amount of data we can recv (8k? That should be plenty...)
							if ( recvd + client->second.readbuffer.length() > 8192 )
								throw CreateHTTPResponse( "400 Request Too Large", "text/html", "Request too large." );
							else	{
								client->second.readbuffer += string( buffer, recvd );
								RecvClientData( client->second );
							}

						}
					}
					else { //Far side hungup or socket is in error, so close
						close( client->first );
						clients.erase( client++ );
						continue;	
					}
				}
			}
			catch( string& response ) {
				client->second.state = 2;
				client->second.writebuffer = response;
			}

			if ( client->second.state == 2 && FD_ISSET( client->first, &writers ) )	{
				int written = send( client->first, &client->second.writebuffer.c_str()[client->second.writepos], client->second.writebuffer.length() - client->second.writepos, (int) NULL );
				client->second.writepos += written;
				if ( client->second.writepos == client->second.writebuffer.length() ) {
					close( client->first );
					clients.erase( client++ );
					continue;
				}
			}
			++client;
		}	


	}


}

void restcomet::RecvClientData( http_client& client )
{
	try {
		const char rnrn[] = "\r\n\r\n";
		uint headerLen, contentLen;

		size_t headerEnd = client.readbuffer.find( rnrn );
		if ( headerEnd != string::npos )
		{
			if ( client.readbuffer.find( "OPTIONS" ) == 0 ) 
				throw CreateCORSResponse();
			boost::regex rxContentLength( "Content-Length: (\\d+)", boost::regbase::icase );
			boost::smatch matches;

			if ( boost::regex_search(client.readbuffer, matches, rxContentLength ) ) {
				stringstream clStream;
				headerLen = headerEnd + 4;
				clStream << matches[1];
				clStream >> contentLen;	
			}
			else
				throw CreateHTTPResponse( "400 Bad Header", "text/html", "400 Bad Header (Content-Length not found)" );

			if ( client.readbuffer.length() >= headerLen + contentLen ) {
				client.current_sequence = m_currentSequence + 1;

				boost::regex rxResource( "^POST /EVENTS\\.LIST HTTP/1\\.1" );
				boost::regex rxSequence( "X-RESTCOMET-SEQUENCE: (\\d+)" );
				boost::regex rxGuid( "[^:|]+" );
				boost::smatch matches;
				if (!boost::regex_search(client.readbuffer, matches, rxResource ) )
					throw CreateHTTPResponse( "404 Not Found", "text/html", "<h3>404 Not Found</h3><small>Did you want to post to EVENTS.LIST by chance?</small>" );
				if ( boost::regex_search(client.readbuffer, matches, rxSequence ) ) {
					stringstream seqStream;
					seqStream << matches[1];
					seqStream >> client.current_sequence;
				}

				if ( client.current_sequence > m_currentSequence + 1 )
					throw CreateHTTPResponse( "400 Future Sequence", "text/html", "400 Future Sequence" );

				string postString = "";
				if ( headerEnd == string::npos || headerEnd + 4 > client.readbuffer.length() )
					throw CreateHTTPResponse( "400 No Post Data", "text/html", "400 No Post Data" );

				//Yeah, length will be greater but I want all of it anyways
				postString = client.readbuffer.substr( headerLen, client.readbuffer.length() );
				map<string, string> postData = DecodePostData( postString );

				if ( postData.find("events") == postData.end() )
					throw CreateHTTPResponse( "400 No Events Specified", "text/html", "400 No Events Specified" );

				boost::match_results<std::string::const_iterator> terms;
				string::const_iterator start, end;
				start = postData["events"].begin();
				end = postData["events"].end();
				while ( boost::regex_search( start, end, terms, rxGuid, boost::match_default ) )	{
					client.eventFilter.insert( TrimStr( string( terms[0].first, terms[0].second ) ) );
					start = terms[0].second;
				}

				client.state = 1;
				{ 	boost::mutex::scoped_lock eventsLock( m_bufferMutex );
					if ( m_currentSequence - client.current_sequence >= RESTCOMET_EVENT_BUFFER_SIZE )
						throw CreateHTTPResponse( "400 Sequence Too Old", "text/html", "<h3>400 Sequence Too Old</h3>" );
					CheckClientEvents( client );
				}

			}
		}
	}
	catch ( const string& response )	{ 
		//Rethrow since it's an http response
		throw;
	}
	catch( exception& e ) {
		throw CreateHTTPResponse( "500 Internal Server Error", "text/html", "<h3>500 Internal Server Error</h3>" );
	} 


}

void restcomet::CheckClientEvents( http_client& client )
{
	vector<Event> eventsToReport;
	int& sequence = client.current_sequence;
	set<string>& eventFilter = client.eventFilter;
	
	for( ;sequence <= m_currentSequence; ++sequence ) {
		int eventPos = sequence % RESTCOMET_EVENT_BUFFER_SIZE;
		if ( eventFilter.find( m_EventBuffer[eventPos].guid ) != eventFilter.end() )
		  eventsToReport.push_back( m_EventBuffer[eventPos] );
	}

	if ( !eventsToReport.empty() ) {
		//Ok - send events 
		string boundary, serializedEvents;
		boundary = GenerateRandomString();
		serializedEvents = SerializeEvents( boundary, eventsToReport );
		client.writebuffer =  CreateHTTPResponse( "200 OK", string( "multipart/mixed; boundary=\"" ) + boundary + "\"", serializedEvents );
		client.state = 2;
	}
}

string restcomet::TrimStr(const string& src, const string& c )
{
	size_t p2 = src.find_last_not_of( c );
	if ( p2 == std::string::npos )
		return std::string();

	size_t p1 = src.find_first_not_of( c );
	if ( p1 == std::string::npos )
		p1 = 0;

	return src.substr( p1, ( p2-p1 )+1 );
}

restcomet::restcomet( int port ) : m_currentSequence( 0 ), m_terminated( false )
{

	struct sockaddr_in thisAddress;

	signal(SIGPIPE, SIG_IGN); // Block SIGPIPE
	try {
		int flags = 0;
		if ( pipe( m_newEventPipes ) == -1 )
			throw runtime_error( "Could not create new event pipe pair" );
		if ( ( flags = fcntl( m_newEventPipes[0], F_GETFL ) ) < 0 )
			throw runtime_error( "Could not get flags on event pipe reader" );
		flags |= O_NONBLOCK;
		if ( fcntl( m_newEventPipes[0], F_SETFL, flags ) < 0 )
			throw runtime_error( "Could not set O_NONBLOCK on event pipe reader" );
		if ( ( flags = fcntl( m_newEventPipes[1], F_GETFL ) ) < 0 )
			throw runtime_error( "Could not get flags on event pipe writer" );
		flags |= O_NONBLOCK;
		if ( fcntl( m_newEventPipes[1], F_SETFL, flags ) < 0 )
			throw runtime_error( "Could not set O_NONBLOCK on event pipe writer" );
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
			throw runtime_error( string( "Could not bind to port for incoming connections: " ) + strerror(errno) );

		if ( listen( m_listenSocket, 100 ) < 0 )
			throw runtime_error( string( "Could not listen on socket: " ) + strerror(errno) );

		m_socketThread.reset( new boost::thread( boost::bind( &restcomet::SocketThreadFunc, this ) ) );
	}
	catch ( exception& e ) {
		if ( close( m_listenSocket ) != 0 )
			throw runtime_error( string( "Unable to close socket: " ) + strerror(errno) );

		close( m_newEventPipes[0] );
		close( m_newEventPipes[1] );
		throw;
	}
}

restcomet::~restcomet()
{
	m_terminated = true;

	if ( m_socketThread.get() != NULL ) {
		//Induce select() to return
		write( m_newEventPipes[1], "n", 1 );
		m_socketThread->join();
	}

	close( m_newEventPipes[0]);
	close( m_newEventPipes[1]);
	close( m_listenSocket );
}

//
// singleton and non-copyable stuff
//

restcomet* restcomet::ms_instance = 0;

restcomet* restcomet::Instance( int port )
{
	if ( ms_instance == 0 )
		ms_instance = new restcomet( port );

	return ms_instance;
}

void restcomet::Release()
{
	if ( ms_instance )
		delete ms_instance;

	ms_instance = 0;
}

}
