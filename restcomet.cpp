#include "restcomet.h"
#include <microhttpd.h>

namespace restcomet
{


void rescomet::SubmitEvent( const string& guid, const string& eventData )
{
	boost::mutex::unique_lock<boost::shared_mutex> eventsLock;
	
	//increment sequence
	m_currentSequence++;
	//insert event
	Event& currentEvent = m_EventBuffer[m_currentSequence % RESTCOMET_EVENT_BUFFER_SIZE];
	
	currentEvent.sequence = m_currentSequence;
	currentEvent.guid = guid;
	currentEvent.timestamp = time( NULL );
	currentEvent.eventData = eventData;
	
	
	//notify all
	condition_variable_any.notify_all();
}

int restcomet::ConnectHandler(	void * cls,
											struct MHD_Connection * connection,
											const char * url,
											const char * method,
											const char * version,
											const char * upload_data,
											size_t * upload_data_size,
											void ** ptr)
{
  static int dummy;
  const char * page = cls;
  struct MHD_Response * response;
  int ret;

  if (0 != strcmp(method, "GET"))
    return MHD_NO; //unexpected method

	//If sequence given, all events happening after that sequence are returned that match given event filter set.
	//If sequence not found, a 408 Request Timeout is returned. The client should take appropriate action given that 
	//	events are lost, for example resetting the state of status controls. For the next request, the client shoud
	//	behave as if it were new and not specify a sequence.
	
	//while no events to return
	//	wait_condition_any
	
}
	
restcomet::restcomet(  int port ) : m_currentSequence( 0 )
{
	  d = MHD_start_daemon(MHD_USE_THREAD_PER_CONNECTION,
		       port,
		       NULL,
		       NULL,
		       &restcomet::ConnectHandler,
		       PAGE,
		       MHD_OPTION_END);
				 
	if ( d == NULL )
		throw runtime_error( "restcomet:ctor() : Could not start web listener" );

}

restcomet::~restcomet()
{
	  MHD_stop_daemon(d);
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
