#include <unistd.h>

int main( int argc, char * argv[])
{
	if( argc>1 ) {
		int i;
		char * xargv[argc+1] ;		

		// make this process a daemon
		daemon( 1, 0 );
		for( i=1; i<argc; i++ ) {
			xargv[i-1] = argv[i] ;
		}
		xargv[argc-1] = NULL ;
		execvp(argv[1], xargv);
	}
	return 0 ;
}
