#include <iostream>
#include "restcomet.h"

using namespace rc;

int main( int argc, char* argv[] )
{
	restcomet* theComet = restcomet::Instance( 8080 );
	int i = 0;
	for( ;; ) {
		string eventGuid;

		switch ( rand() % 3 ) {
			case 0:
				eventGuid = "blargh";
				break;
			case 1:
				eventGuid = "foo";
				break;
			case 2:
				eventGuid = "bar";
				break;
		}

		theComet->SubmitEvent( eventGuid, "EVENT DATA" );
		if ( i++  % 10000 == 0 )
			cout << i - 1 << endl;
	}
}
