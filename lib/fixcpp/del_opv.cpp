/*
    c++ memory op replacement (for toolchain incompatibility)
*/

#include <stdlib.h>

// simplified c++ memory op
void operator delete[](void* ptr) { ::operator delete(ptr); }
