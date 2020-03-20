#ifndef __genclass_h__
#define __genclass_h__

// General purpose classes

#include "util/string.h"
#include "util/array.h"

int savetxtfile(const char *filename, array <string> & strlist );
int readtxtfile(const char *filename, array <string> & strlist);

#endif		// __genclass_h__
