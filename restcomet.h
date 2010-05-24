#ifndef RESTCOMET_H
#define RESTCOMET_H

#include <string>
#include <boost/thread.hpp>
#include <boost/regex.hpp>

using namespace std;

namespace rc
{

const uint RESTCOMET_EVENT_BUFFER_SIZE = 2048;

struct Event
{
	uint sequence;
	string guid;
	time_t timestamp;
	string eventData;
};

class restcomet
{
	public:
	
		void SubmitEvent( const string& guid, const string& eventData );

	private:
		void DispatchEvent( const Event& event );
		static map<string, string> DecodePostData( const string& rawPostData );
		static void ReplacePercentEncoded( string& workString );	
		void SocketDispatchThreadFunc();
		void ConnectionHandlerThreadFunc( int clientSock );

		
		boost::shared_mutex m_bufferMutex;
		boost::condition_variable_any m_conditionNewEvent;
		auto_ptr<boost::thread> m_socketDispatchThread;
		boost::thread_group m_connectionHandlerThreadGroup;
		Event m_EventBuffer[RESTCOMET_EVENT_BUFFER_SIZE];
		uint m_currentSequence;
		
		volatile bool m_terminated;
		int m_listenSocket;
		

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
