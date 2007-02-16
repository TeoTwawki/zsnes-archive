// Included in source files after other includes
#include <stdbool.h>
#include <assert.h>
#include <limits.h>

#if INT_MAX < 0x7FFFFFFF
	#error "Requires that int type have at least 32 bits"
#endif

#ifndef BLARGG_DEBUG_H
	#undef check
	#define check( expr ) ((void) 0)
	
	#undef dprintf
	#define dprintf( ... ) ((void) 0)
#endif
