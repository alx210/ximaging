/*
 * Copyright (C) 2012-2025 alx@fastestcode.org
 * This software is distributed under the terms of the MIT license.
 * See the included LICENSE file for further information.
 */

#ifndef TOOLTALK_H
#define TOOLTALK_H

#ifdef ENABLE_CDE
/* Initialize the ToolTalk session and register patterns if server */
void init_tt_media_exchange(Boolean server);

/* Close the ToolTalk session */
void quit_tt_session(void);

/* 
 * Pass 'open_spec' to the server instance if it exists.
 * Returns True if a server instance has accepted the request.
 */
Boolean query_server(const char *open_spec, const char*);

#else

/* 
 * Implemented in xipc.c; This is a substitute for ToolTalk IPC.
 * Checks for an existing instance and returns True if no other instance
 * exists, initiates client/server communication and returns False otherwise.
 */
Boolean init_x_ipc(const char *open_spec, const char *force_suffix);

#endif /* ENABLE_CDE */

#endif /* TOOLTALK_H */
