/*
    c++ memory op replacement (for toolchain incompatibility)
*/

#include <stdlib.h>

// simplified c++ memory op
void* operator new(size_t sz) { return malloc(sz); }
