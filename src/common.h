/*
 * Copyright (C) 2012-2024 alx@fastestcode.org
 * This software is distributed under the terms of the MIT license.
 * See the included LICENSE file for further information.
 */
 
/*
 * Core data structures, application global data and prototypes.
 */

#ifndef COMMON_H
#define COMMON_H

#include <Xm/Xm.h>

/* Application resources */
struct app_resources {
	char *geometry;		/* geometry from xrdb or tooltalk message */
	char *tile_size;	/* tile size (small|medium|large)*/
	char *tile_presets;	/* list of tile sizes <small,medium,large> */
	char *tile_asr;		/* tile aspect ratio (x:y) */
	Boolean zoom_fit;	/* fit image to window (in viewer) */
	char *zoom_inc;		/* zoom increment */
	int key_pan_amount; /* direction key pan amount */
	Boolean pin_window;	/* pin the window (don't reuse for load requests) */
	Boolean server;		/* launch as a ToolTalk message server */
	int refresh_int;	/* browser refresh interval */
	Boolean fast_pan; /* disable interpolation when panning */
	Boolean vprog;	/* display the image while it's being loaded */
	Boolean int_up;	/* interpolate when upsampling */
	Boolean int_down;	/* interpolate on downsampling */
	Boolean quiet;		/* don't show warning messages */
	Boolean keep_tform;	/* preserve transformation flags */
	Boolean show_viewer_tbr; /* show toolbar in the viewer window */
	Boolean large_tbr_icons; /* use large pixmaps in toolbar buttons */
	Boolean large_cursors;	/* use large cursor pixmaps */
	Boolean confirm_rm; /* display a confirmation dialog on file removal */
	Boolean browse;	/* launch in browser mode */
	Boolean show_dirs; /* show subdirectories in the browser */
	Boolean show_dot_files; /* show files/dirs starting with . */
	char *edit_cmd; /* the command to invoke for File/Edit */
};

/* defined in main.c */
extern struct app_resources init_app_res;

/* Application instance data */
struct app_inst_data {
	XtAppContext context;
	Display *display;
	Widget session_shell;
	char *bin_name;	/* binary name the application was invoked with */
	unsigned int active_shells; /* number of windows in this instance */
	XVisualInfo visual_info; /* common visual */
	Colormap colormap; /* common colormap */
	int pixel_size; /* padded pixel size in bits */
	Atom XaWM_DELETE_WINDOW;
	Atom XaTEXT;
	#ifndef ENABLE_CDE
	Atom XaSERVER;
	Atom XaSERVER_REQ;
	#endif /* ENABLE_CDE */
};

/*  main.c exports */
extern struct app_inst_data app_inst;

/* Reliable signal handling (using POSIX sigaction) */
typedef void (*sigfunc_t)(int);
sigfunc_t rsignal(int sig, sigfunc_t);

/* 
 * Display an error message and terminate.
 * if "title" is null "Fatal Error" will be used.
 * if "message" is NULL then strerror() text for errno_value is used.
 */
void fatal_error(int errno_value, const char *title, const char *message);

/* Print a warning message to stderr */
void warning_msg(const char *msg);

/* Set the exit flag and the return value */
void set_exit_flag(int ret);

#endif /* COMMON_H */
