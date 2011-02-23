#ifndef RESTCOMET_H
#define RESTCOMET_H

#include <string>
#include <vector>
#include <set>
#include <boost/thread.hpp>
#include <boost/regex.hpp>

using namespace std;

namespace rc
{

const int RESTCOMET_EVENT_BUFFER_SIZE = 8192;

struct Event
{
	uint sequence;
	string guid;
	time_t timestamp;
	string eventData;
};

class http_client
{
	public:
	http_client() : state( 0 ), writepos( 0 ), readStarted( time( NULL ) ), current_sequence( 0 ) {}
	int state; /* reading; waiting; writing */
	string readbuffer;
	string writebuffer;
	uint writepos;
	time_t readStarted; /* used for timing out invalid clients */
	int current_sequence;
	set<string> eventFilter;
};

class restcomet
{
	public:

		void SubmitEvent( const string& guid, const string& eventData );

	private:
		void DispatchEvent( const Event& event );
		static map<string, string> DecodePostData( const string& rawPostData );
		static void ReplacePercentEncoded( string& workString );
		static string GenerateRandomString() throw();
		static string SerializeEvents( const string& boundary, const vector<Event>& events );
		static string CreateHTTPResponse( const string& codeAndDescription, const string& contentType, const string& body );
		static string CreateCORSResponse();
		void SocketThreadFunc();
		void RecvClientData( http_client& client );
		/** 
		* @brief Checks if there are current events, if so fills the client writebuffer and sets state to 2 (writing)
		* 
		* @param client The client to check
		* @warning You MUST be in a critical section of m_bufferMutex when calling this function!!
		*/
		void CheckClientEvents( http_client& client );
		static string TrimStr(const string& src, const string& c = " \r\n" );

		boost::mutex m_bufferMutex;
		auto_ptr<boost::thread> m_socketThread;
		Event m_EventBuffer[RESTCOMET_EVENT_BUFFER_SIZE];
		int m_currentSequence;

		volatile bool m_terminated;
		int m_listenSocket;
		int m_newEventPipes[2];

	//Singleton & non-copyable stuff
	public:
		static restcomet* Instance( int port );
		static void Release();
	private:
		restcomet( const restcomet& rhs );
		restcomet& operator=( const restcomet& rhs );
		restcomet( int port );
		~restcomet();
		static restcomet* ms_instance;
};

}

#endif // RESTCOMET_H
