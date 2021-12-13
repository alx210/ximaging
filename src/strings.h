/*
 * Copyright (C) 2012-2017 alx@fastestcode.org
 * This software is distributed under the terms of the MIT license.
 * See the included LICENSE file for further information.
 */
 
/*
 * String IDs for catgets.
 */

#ifndef STRINGS_H
#define STRINGS_H

/* 
 * Viewer menus 
 */

#define VIEWER_MENU_MSGSET	1

#define SID_VMFILE		1	/* File (cascade) */
#define SID_VMOPEN		2	/* Open... */
#define SID_VMBROWSE	3	/* Browse... */
#define SID_VMBHERE		4	/* Browse Here */
#define SID_VMCLOSE		5	/* Close */
#define SID_VMFEDIT		6	/* Edit */
#define SID_VMNEXT		7	/* Next File */
#define SID_VMPREV		8	/* Previous File */

#define SID_VMVIEW		20	/* View (cascade) */
#define SID_VMREFRESH	21	/* Refresh */
#define SID_VMPROPS		22	/* Properties */
#define SID_VMNEXTPAGE	23	/* Next Page */
#define SID_VMPREVPAGE	24	/* Previous Page */
#define SID_VMTOOLBAR	25	/* Toolbar */

#define SID_VMZOOM		30	/* Zoom (cascade) */
#define SID_VMZOOMIN	31	/* Zoom In */
#define SID_VMZOOMOUT	32	/* Zoom Out */
#define SID_VMNOZOOM	33	/* Original Size */
#define SID_VMZOOMFIT	34	/* Fit to Window */

#define SID_VMROTATE	40	/* Rotate */
#define SID_VMVFLIP		41	/* Flip Vertically */
#define SID_VMHFLIP		42	/* Flip Horizontally */
#define SID_VMROTATECW	43	/* Rotate Right */
#define SID_VMROTATECCW	44	/* Rotate Left */
#define SID_VMRESET		45	/* Reset */
#define SID_VMLOCK		46	/* Lock */

#define SID_VMEDIT		50	/* Edit (cascade) */
#define SID_VMCOPYTO	51	/* Copy to... */
#define SID_VMMOVETO	52	/* Move to... */
#define SID_VMRENAME	53	/* Rename... */
#define SID_VMDEL		54	/* Delete */

#define SID_VMWINDOW	60	/* Window */
#define SID_VMPIN		61	/* Pin This*/
#define SID_VMNEW		62	/* Open New */

#define SID_VMHELP		100	/* Help (cascade) */
#define SID_VMTOPICS	101	/* Manual */
#define SID_VMABOUT		102	/* About */


/* 
 * Browser Menus 
 */

#define BROWSER_MENU_MSGSET 2

#define SID_BMFILE		1	/* File (cascade) */
#define SID_BMBROWSE	2	/* Browse */
#define SID_BMFEDIT		3	/* Edit */
#define SID_BMDISPLAY	4	/* Display */
#define SID_BMCLOSE		5	/* Close */

#define SID_BMVIEW			20	/* View (cascade) */
#define SID_BMREFRESH		21	/* Refresh */
#define SID_BMPROPERTIES	23	/* Properties */
#define SID_BMSMALL			24	/* Small Tiles */
#define SID_BMMEDIUM		25	/* Medium Tiles */
#define SID_BMLARGE			26	/* Large Tiles */
#define SID_BMSUBDIRS		27	/* Directories */
#define SID_BMDOTFILES		28	/* Dot Files */

#define SID_BMWINDOW		30	/* Window */
#define SID_BMPIN			31	/* Pin This */
#define SID_BMNEWWINDOW		32	/* Open New */


#define SID_BMEDIT			40	/* Edit (cascade) */
#define SID_BMSELECTALL		41	/* Select All */
#define SID_BMSELECTNONE	42	/* Select None */
#define SID_BMINVERTSEL		43
#define SID_BMCOPYTO		44	/* Copy To... */
#define SID_BMMOVETO		45	/* Move To... */
#define SID_BMRENAME		46	/* Rename... */
#define SID_BMDELETE		47	/* Delete */

#define SID_BMHELP			100	/* Help (cascade) */
#define SID_BMTOPICS		101	/* Manual */
#define SID_BMABOUT			102	/* About */

/*
 * Application messages
 */
#define APP_MSGSET 		3

#define SID_NOVISUAL	1	/* No appropriate X visual found. */
#define SID_EHOSHM		2	/* Shared memory extension not available. */
#define SID_EARG		3	/* Ignoring unrecognized arguments. */
#define SID_EDTSVC		4	/* Unable to initialize desktop services. */
#define SID_ETTSVC		5	/* ToolTalk service not available. */
#define SID_ETTMSG		6	/* ToolTalk message delivery failed. */
#define SID_EFATAL		7	/* Fatal error */
#define SID_EDMX		8	/* Could not initialize desktop media exchange. */
#define SID_LOADING		9	/* Loading... */
#define SID_READY		10	/* Ready */
#define SID_IMGUNSUP	11	/* Unsupported image format. */
#define SID_IMGINVAL	12	/* Invalid or damaged image file. */
#define SID_IMGERROR	13	/* An error occured while reading file. */
#define SID_ENORES		14	/* Not enough resources available for this task. */
#define SID_EFPERM		15	/* No permission to access specified file. */
#define SID_EFILETYPE	16	/* Specified file is invalid. */
#define SID_ESERVING	17	/* Application is already serving this display. */
#define SID_ENOPTYPE	18	/* ToolTalk ptype could not be registered... */
#define SID_NOCOLORSRV	19	/* Could not retrieve desktop color palette. */
#define SID_WARNING		20	/* Warning */
#define SID_NOTHREADS	21	/* Multithreading support in X Toolkit required. */
#define SID_EOPEN		22	/* Could not open file. */
#define SID_FREPLACE	23	/* File already exists. Overwrite? */
#define SID_EFILEPROC	24	/* Error processing file: */
#define SID_NOIMAGE		25	/* No image loaded */
#define SID_EREADDIR	26	/* Error reading directory. */
#define SID_READDIRPG	27	/* Reading directory... */
#define SID_BUSY		28	/* Processing... */
#define SID_CFUNLINK	29	/* The file will be irv. deleted. Continue? */
#define SID_CMFUNLINK	30	/* Selected files will be irv. deleted. Continue? */
#define SID_ENOFILE		31	/* No image files in current directory. */
#define SID_EFAILED		32	/* The action couldn't be completed. */
#define SID_FILES		33	/* Files */
#define SID_NOITEMS		34	/* No items to display */
#define SID_SELECTED	35	/* selected */
#define SID_EFEXIST		36	/* File already exists. */
#define SID_QSTIMEOUT	37	/* Another instance is running...*/

/*
 * Dialog strings
 */
#define DLG_MSGSET		4

#define SID_YES			1	/* Yes */
#define SID_NO			2	/* No */
#define SID_OK			3	/* OK */
#define SID_CANCEL		4 	/* Cancel */
#define SID_RETRY		5	/* Retry */
#define SID_DISMISS		6	/* Dismiss */
#define SID_HELP		7	/* Help */
#define SID_OPENFILE	8	/* Open File */
#define SID_OPENDIR		9	/* Open Directory */
#define SID_SOURCE		10	/* Source */
#define SID_DESTINATION	11	/* Destination */
#define SID_COPYING		12	/* Copying */
#define SID_MOVING		13	/* Moving */
#define SID_DELETING	14	/* Deleting */
#define SID_SELDESTDIR	15	/* Select Destination */
#define SID_INPUTFNAME	16	/* Specify a new name for */
#define SID_RENAME		17	/* Rename */
#define SID_UNLINK		18	/* Delete */
#define SID_XRES		19	/* Width */
#define SID_YRES		20	/* Height */
#define SID_BPP			21	/* Depth */
#define SID_DATE		22	/* Date */
#define SID_FSIZE		23	/* Size */

/* Use catgets for message strings */
#ifdef ENABLE_NLS
#include <nl_types.h>
extern nl_catd msg_cat_desc;	/* declared in main.c */
#define nlstr(set,msg,text) catgets(msg_cat_desc,set,msg,text)
#else
#define nlstr(set,msg,text) text
#endif /* ENABLE_NLS */

#endif /* STRINGS_H */
