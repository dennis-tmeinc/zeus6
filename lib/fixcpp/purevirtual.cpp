/*
    c++ pure virtual error handler (for toolchain incompatibility)
*/

#include <unistd.h>
#include <stdlib.h>
#include <string.h>

extern "C" void __cxa_pure_virtual()
{
    const char * err = "Pure virtual function called!\n" ;
    write(2,err,strlen(err));
    abort();
}
