#include "restcomet.h"
#include <microhttpd.h>

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

restcomet::restcomet(  int port ) : m_currentSequence( 0 )
{

}

restcomet::~restcomet()
{

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
