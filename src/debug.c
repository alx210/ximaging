/*
 * Copyright (C) 2012-2025 alx@fastestcode.org
 * This software is distributed under the terms of the MIT license.
 * See the included LICENSE file for further information.
 */
 
/*
 * Debugging helpers
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <signal.h>
#include "debug.h"

void _dbg_trace(int trap, const char *file,
	int line, const char *fmt, ...)
{
	FILE *out;
	#ifdef DBG_TO_STDERR
	out=stderr;
	#else
	out=stdout;
	#endif /* DBG_TO_STDERR */
	va_list vl;
	va_start(vl,fmt);
	fprintf(out,"%s (%u): ",file,line);	
	vfprintf(out,fmt,vl);
	va_end(vl);
	fflush(out);
	if(trap) raise(SIGTRAP);
}
