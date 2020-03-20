/*
    c++ pure virtual error handler (for toolchain incompatibility)
*/

#include <unistd.h>
#include <stdlib.h>
#include <string.h>

namespace __cxxabiv1 {

extern "C" void
__cxa_pure_virtual (void)
{
    const char * err = "pure virtual method called\n" ;
    write(2,err,strlen(err));
    abort();    
}

extern "C" void
__cxa_deleted_virtual (void)
{
    const char * err = "deleted virtual method called\n" ;
    write(2,err,strlen(err));
    abort();    
}

}