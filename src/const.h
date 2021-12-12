/*
 * Copyright (C) 2012-2017 alx@fastestcode.org
 * This software is distributed under the terms of the MIT license.
 * See the included LICENSE file for further information.
 */
 
/*
 * Common constants and macro definitions.
 */

#ifndef CONST_H
#define CONST_H

/* Release info */
#define APP_VERSION 1
#define APP_REVISION 4

#define BASE_NAME "ximaging"	/* X application name */
#define APP_CLASS "XImaging"	/* X application class */
#define BASE_TITLE "XImaging"	/* base title for GUI elements */

/* Copyright string */
#define COPYRIGHT "Copyright (C) 2012-2021 alx@fastestcode.org" \
	"\nThis program is distributed under the terms of the MIT license." \
	"\nSee the included LICENSE file for detailed information."

#ifdef ENABLE_CDE

/* ToolTalk ptype */
#define APP_TT_PTYPE "XImaging"

/* ToolTalk op used to send server requests */
#define APP_TT_COM_OP "XImaging_Server_Reqest"

/* Timeout in ms to wait for a server response */
#define APP_TT_COM_TIMEOUT 3000

#else /* ENABLE_CDE */

/* Selection atoms */
#define XA_SERVER "XImaging_Server"
#define XA_SERVER_REQ "XImaging_Server_Request"
	
#endif /* ENABLE_CDE */

/* Tile size and aspect ratio defaults */
#define DEF_TILE_ASR "4:3"
#define DEF_TILE_PRESETS "80,120,160"
#define DEF_TILE_SIZE "small"
#define MIN_TILE_SIZE 60
#define MAX_TILE_SIZE 600
#define DEF_ZOOM_INC "1.6"
#define MIN_ZOOMED_SIZE 32

/* Directory refresh interval in seconds */
#define DEF_REFRESH_INT 3

/* Default amount of pixels to scroll with direction keys */
#define DEF_KEY_PAN_AMOUNT 15

/* Number of colors to allocate on PseudoColor visuals */
#define SHARED_CMAP_ALLOC	128
#define PRIV_CMAP_ALLOC		256

/* Minimum amount of colors before falling back to a private colormap */
#define MIN_SHARED_CMAP		16

#endif /* CONST_H */
