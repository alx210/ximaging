/*
 * Copyright (C) 2012-2017 alx@fastestcode.org
 * This software is distributed under the terms of the MIT license.
 * See the included LICENSE file for further information.
 */
 
/*
 * Debugging helpers
 */

#ifndef DEBUG_H
#define DEBUG_H 

#ifdef DEBUG
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <signal.h>

static void _dbg_trace(int trap, const char *file,
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

#define dbg_trace(fmt,...) _dbg_trace(0,__FILE__,__LINE__,fmt,##__VA_ARGS__)
#define dbg_trap(fmt,...) _dbg_trace(1,__FILE__,__LINE__,fmt,##__VA_ARGS__)
#define dbg_assert(exp)((exp)?(void)0:\
	_dbg_trace(1,__FILE__,__LINE__,"Assertion failed. Exp: %s\n",#exp));
#define dbg_assertmsg(exp,msgfmt,...)((exp)?(void)0:\
	_dbg_trace(1,__FILE__,__LINE__,msgfmt,##__VA_ARGS__));
#define dbg_printf printf

#ifdef ENABLE_MEMDB
#include "memdb.h"
#endif

#else /* NON DEBUG */

#define dbg_trace(...) ((void)0)
#define dbg_trap(...) ((void)0)
#define dbg_printf(...) ((void)0)
#define dbg_assert(exp) ((void)0)
#define dbg_assertmsg(...) ((void)0)

#endif /* DEBUG */

#endif /* DEBUG_H */
