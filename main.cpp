#include <iostream>
#include <map>
#include "restcomet.h"

using namespace rc;

int main(int argc, char **argv)
{
	string testevent;

	restcomet* rc = restcomet::Instance( 8080 );

	do
	{
		cout << "\nEnter event GUID: " << flush;
		cin >> testevent;
		rc->SubmitEvent( testevent, "some data, yo");
	} while ( testevent != "quit" );



	restcomet::restcomet::Release();

}
