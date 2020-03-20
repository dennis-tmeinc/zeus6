#include <unistd.h>
#include <stdio.h>

// run argv[1] as a daemon
int main( int argc, char * argv[])
{
	if( argc>1 && daemon( 1, 0 ) == 0 ) {
		execvp(argv[1], &argv[1]);
	}
	printf("Can't run as a daemon\n");
	return 0 ;
}