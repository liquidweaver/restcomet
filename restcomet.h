#ifndef RESTCOMET_H
#define RESTCOMET_H

#include <string>
#include <boost/thread.hpp>
#include <microhttpd.h>


using namespace std;

namespace restcomet
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
	
		boost::shared_mutex m_bufferMutex;
		boost::condition_variable_any m_conditionNewEvent;
		
		Event m_EventBuffer[RESTCOMET_EVENT_BUFFER_SIZE];
		uint m_currentSequence;
		
		static int ConnectHandler(	void * cls,
											struct MHD_Connection * connection,
											const char * url,
											const char * method,
											const char * version,
											const char * upload_data,
											size_t * upload_data_size,
											void ** ptr);
		struct MHD_Daemon* d;
		

	//Singleton & non-copyable stuff		
	public:
		static restcomet* Instance( int port );
		static void Release();
	private:
		restcomet( const restcomet& rhs );
		restcomet& operator=( const restcomet& rhs );
		restcomet();
		~restcomet();
		static restcomet* ms_instance;

};

}

#endif // RESTCOMET_H
