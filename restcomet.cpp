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
#include <set>

using namespace std;

namespace rc
{

int TimedAccept(int sock, struct ::sockaddr* sockAddress, ::socklen_t* sockAddressLen, int timeout)
{
    int sfd = sock;
    if ( sfd < 0 )	//couldn't get socket
    {   throw runtime_error("no socket");
    }

    int isReadReady; //number of items ready to be read
    fd_set wfds;
    struct timeval tVal;
    tVal.tv_sec = timeout;	//timeout after n seconds
    tVal.tv_usec = 0;
    FD_ZERO ( &wfds );
    FD_SET ( ( unsigned int ) sfd, &wfds );

    //since this is likely being called from an accept, this should be immediate
    struct timeval* tValPtr = &tVal;
    if ( tVal.tv_sec < 0)
            tValPtr = NULL;
    isReadReady = select ( sfd+1, &wfds, NULL, NULL, tValPtr );	//sfd should still be in wfds
    if ( isReadReady > 0  && FD_ISSET ( sfd, &wfds ) )
    {    return accept(sock, ( struct sockaddr* ) sockAddress, sockAddressLen);
    }
    else
    {   return -1;
    }
}


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

	for ( unsigned int i=0; i < spacedPostData.length(); ++i )
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
	size_t pos = workString.find( "%" );
	while( pos != string::npos )
	{
		size_t numPos = pos + 1;
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

string restcomet::GenerateRandomString() throw()
{

	static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
	string retString = "0123456789012345";

	 for (unsigned int i = 0; i < retString.length(); ++i)
	 {
        retString[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
    }

    return retString;
}

string restcomet::SerializeEvents( const string& boundary, const vector<Event>& events )
{
	ostringstream sEvents;

	for ( vector<Event>::const_iterator anEvent = events.begin();
			anEvent != events.end();
			++anEvent)
	{
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
					"Content-Type: " << contentType << "\r\n"
					"Content-Length: " << body.length() << "\r\n\r\n"
					<< body;
	return response.str();
}

void restcomet::SocketDispatchThreadFunc()
{
	while ( !m_terminated )
	{   
            int clientSock = 0;
            bool sockOpen = false;
            try
            {
		struct sockaddr_in clientAddress;
		unsigned int clientLen = sizeof( clientAddress );
		clientSock = TimedAccept( m_listenSocket, ( struct sockaddr* ) &clientAddress, &clientLen, 5 );
                sockOpen = true;
                int on = 1;
		setsockopt( clientSock, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof( on ) );
	

		if ( clientSock >= 0 )
		{
			m_connectionHandlerThreadGroup.create_thread( boost::bind( &restcomet::ConnectionHandlerThreadFunc, this, clientSock ) );
		}
            }
            catch(boost::thread_interrupted)
            {
            }
            catch(boost::thread_exception &e)
            {   //did someone spam the socket?
                if(sockOpen)
                {   close( clientSock );
                }
            }
	}
}

void restcomet::ConnectionHandlerThreadFunc( int clientSock )
{
	try
	{
		string rawRequest;
		ReceiveHTTPRequest( clientSock, rawRequest );

		if (fcntl( clientSock, F_SETFL, O_NONBLOCK) == 1)
			throw CreateHTTPResponse( "500 Internal Server Error", "text/html", "Could not set non-blocking on socket" );
		uint sequence = m_currentSequence + 1;
		boost::regex rxResource( "^POST /EVENTS\\.LIST HTTP/1\\.1" );
		boost::regex rxSequence( "X-RESTCOMET-SEQUENCE: (\\d+)" );
		boost::regex rxGuid( "[^:|]+" );
		boost::smatch matches;
		if (!boost::regex_search(rawRequest, matches, rxResource ) )
			throw CreateHTTPResponse( "404 Not Found", "text/html", "<h3>404 Not Found</h3><small>Did you want to post to EVENTS.LIST by chance?</small>" );
		if ( boost::regex_search(rawRequest, matches, rxSequence ) )
		{
			stringstream seqStream;
			seqStream << matches[1];
			seqStream >> sequence;
		}
		set<string> eventFilter;

		unsigned int postStart = rawRequest.find( "\r\n\r\n" );
		string postString = "";
		if ( postStart == string::npos || postStart + 4 > rawRequest.length() )
			throw CreateHTTPResponse( "400 No Post Data", "text/html", "400 No Post Data" );

		//Yeah, length will be greater but I want all of it anyways
		postString = rawRequest.substr( postStart + 4, rawRequest.length() );
		map<string, string> postData = DecodePostData( postString );

		if ( postData.find("events") == postData.end() )
			throw CreateHTTPResponse( "400 No Events Specified", "text/html", "400 No Events Specified" );

		boost::match_results<std::string::const_iterator> terms;
		string::const_iterator start, end;
		start = postData["events"].begin();
		end = postData["events"].end();
		while ( boost::regex_search( start, end, terms, rxGuid, boost::match_default ) )
		{
			eventFilter.insert( string( terms[0].first, terms[0].second ) );
			start = terms[0].second;
		}

		vector<Event> eventsToReport;
		do
		{
			boost::shared_lock<boost::shared_mutex> eventBufferSharedLock( m_bufferMutex );
			if ( sequence > m_currentSequence + 1 )
				throw CreateHTTPResponse( "400 Future Sequence", "text/html", "400 Future Sequence" );

			for( ;sequence <= m_currentSequence; ++sequence )
			{
				uint eventPos = sequence % RESTCOMET_EVENT_BUFFER_SIZE;
				if ( eventFilter.find( m_EventBuffer[eventPos].guid ) != eventFilter.end() )
					eventsToReport.push_back( m_EventBuffer[eventPos] );
			}

			if (eventsToReport.empty() )
				m_conditionNewEvent.wait( eventBufferSharedLock );

			//Check socket health
			char buf[1];
			if ( recv( clientSock, buf, 0, MSG_PEEK ) == 0 ) //Will be 0 is socket disconnected. Requires O_NONBLOCK to work
				throw 0;
		} while (eventsToReport.empty());

		//Ok - send events
		string boundary, serializedEvents;
		boundary = GenerateRandomString();
		serializedEvents = SerializeEvents( boundary, eventsToReport );
		string outData = CreateHTTPResponse( "200 OK", string( "multipart/mixed; boundary=\"" ) + boundary + "\"", serializedEvents );
		/* int bytesSent =*/ send( clientSock, outData.c_str(), outData.length(), 0 );
		
	}
	catch ( const string& response )
	{
		send( clientSock, response.c_str(), response.length(), 0 );
	}
	catch(...)
	{ } //Cancel this request

	close( clientSock );
}

void	restcomet::ReceiveHTTPRequest( const int clientSock, string& rawRequest )
{
	const char rnrn[] = "\r\n\r\n";
	char buf[4096];
	ssize_t recvd = 0;
	long int contentLen = LONG_MAX, headerLen = 0;
	do
	{
		ssize_t lastRecvd = 0;
		
		if ( recvd >= 4096 )
			throw CreateHTTPResponse( "500 Internal Server Error", "text/html", "Ran out of buffer to process POST request" );

		lastRecvd = recv( clientSock, &buf[recvd], 4096 - recvd, 0 );
		if ( lastRecvd == 0 )
			throw runtime_error( "Connection reset" );
		else if ( lastRecvd < 0 )
		{
			perror( "ReceiveHTTPRequest" );
			abort();
		}
		recvd += lastRecvd;

		char* headerEnd = reinterpret_cast<char*>( memmem( buf, recvd, rnrn, sizeof( rnrn ) - 1 ) ); //-1 for null zero 
		if ( headerEnd != NULL )
		{
			boost::regex rxContentLength( "Content-Length: (\\d+)", boost::regbase::icase );
			boost::smatch matches;
			rawRequest = string( buf, recvd );	

			if ( boost::regex_search(rawRequest, matches, rxContentLength ) )
			{
				stringstream clStream;
				headerLen = (headerEnd - buf) + 2;
				clStream << matches[1];
				clStream >> contentLen;	
			}
			else
				throw CreateHTTPResponse( "400 Bad Header", "text/html", "400 Bad Header (Content-Length not found)" );
		}
	} while ( recvd < contentLen + headerLen );
}

restcomet::restcomet( int port ) : m_currentSequence( 0 ), m_terminated( false )
{

	struct sockaddr_in thisAddress;
	signal(SIGPIPE, SIG_IGN); // Block SIGPIPE
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
