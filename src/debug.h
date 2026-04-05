/*
 * Copyright (C) 2012-2026 alx@fastestcode.org
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
#define dtrace(fmt,...) _dbg_trace(0,__FILE__,__LINE__,fmt,##__VA_ARGS__)
#define dtrap(fmt,...) _dbg_trace(1,__FILE__,__LINE__,fmt,##__VA_ARGS__)
#define dassert(exp)((exp)?(void)0:\
	_dbg_trace(1,__FILE__,__LINE__,"Assertion failed. Exp: %s\n",#exp));
#define dassertmsg(exp,msgfmt,...)((exp)?(void)0:\
	_dbg_trace(1,__FILE__,__LINE__,msgfmt,##__VA_ARGS__));
#define dprintf printf

#else /* NON DEBUG */

#define dtrace(...) ((void)0)
#define dtrap(...) ((void)0)
#define dprintf(...) ((void)0)
#define dassert(exp) ((void)0)
#define dassertmsg(...) ((void)0)

#endif /* DEBUG */

#endif /* DEBUG_H */
