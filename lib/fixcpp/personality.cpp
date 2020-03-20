/*
    c++ personality handler (for toolchain incompatibility)
*/

#include <unistd.h>

namespace __cxxabiv1
{

// fake GNU C++ personality routine, Version 0.
extern "C" int __gxx_personality_v0
     (int, int, unsigned long,
      void *, void *)
      {
          return 9 ;    // return (_Unwind_Reason_Code)_URC_FAILURE ;
      }

}
