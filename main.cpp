#include <iostream>
#include <map>
#include "restcomet.h"

using namespace rc;

int main(int argc, char **argv)
{
	string presstocont;
	
	restcomet* rc = restcomet::Instance( 8080 );
	
	cin >> presstocont;
	
		
	restcomet::restcomet::Release();
	 
}
