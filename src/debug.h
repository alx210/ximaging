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

void _dbg_trace(int trap, const char *file,
	int line, const char *fmt, ...);

#ifdef DEBUG
#define dbg_trace(fmt,...) _dbg_trace(0,__FILE__,__LINE__,fmt,##__VA_ARGS__)
#define dbg_trap(fmt,...) _dbg_trace(1,__FILE__,__LINE__,fmt,##__VA_ARGS__)
#define dbg_assert(exp)((exp)?(void)0:\
	_dbg_trace(1,__FILE__,__LINE__,"Assertion failed. Exp: %s\n",#exp));
#define dbg_assertmsg(exp,msgfmt,...)((exp)?(void)0:\
	_dbg_trace(1,__FILE__,__LINE__,msgfmt,##__VA_ARGS__));
#define dbg_printf printf

#else /* NON DEBUG */

#define dbg_trace(...) ((void)0)
#define dbg_trap(...) ((void)0)
#define dbg_printf(...) ((void)0)
#define dbg_assert(exp) ((void)0)
#define dbg_assertmsg(...) ((void)0)

#endif /* DEBUG */

#endif /* DEBUG_H */
