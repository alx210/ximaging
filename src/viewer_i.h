/*
 * Copyright (C) 2012-2024 alx@fastestcode.org
 * This software is distributed under the terms of the MIT license.
 * See the included LICENSE file for further information.
 */
 
/*
 * Private viewer data structures, constants and typedefs
 */

#ifndef VIEWER_I_H
#define VIEWER_I_H

#include <inttypes.h>
#include <time.h>
#include <pthread.h>
#include <sys/stat.h>
#include <X11/Xlib.h>
#ifdef ENABLE_CDE
#include <Tt/tt_c.h>
#endif /* ENABLE_CDE */
#include "imgfile.h"
#include "pixconv.h"

/* 
 * Viewer instance data 
 */
struct viewer_data {
	Widget wshell;
	Widget wmain;
	Widget wmenubar;
	Widget wview;
	Widget wmsg;
	Widget wprops;
	Widget wzoom;
	Widget wtoolbar;
	
	/* shell properties */
	Boolean pinned;	/* the shell is pinned (not reused for load requests) */
	char *title;	/* base title string */
	Boolean vprog;	/* visual progress */
	Boolean vprog_zfit; /* fit zoom when loading */
	int key_pan_amount; /* direction key pan amount */
	Boolean panning;
	Boolean show_tbr; /* toolbar visible */
	Boolean large_tbr; /* use large pixmaps in toolbar buttons */
	
	/* loader thread data */
	pthread_t ldr_thread;
	pthread_cond_t ldr_finished_cond;
	pthread_mutex_t ldr_cond_mutex;
	
	/* viewer state */
	unsigned short state; /* current state of the viewer */
	time_t mod_time; /* file's last modified time */
	int cur_page; /* current page */
	short load_prog;	/* progress 0-100% */
	XtIntervalId prog_timer;
	XtIntervalId vprog_timer;
	
	/* view properties */
	float zoom;		/* current zoom */
	Boolean zoom_fit;	/* whether zoom follows window size */
	short xoff;		/* panning offsets (top,left) */
	short yoff;		/* note that these are always negative */
	unsigned short tform;	/* transformation flags IMGT_* */
	Boolean keep_tform;	/* preserve transformation across images */
	struct pixel_format display_pf; /* display pixel format */	
	GC blit_gc;		/* GC for image drawing */
	Pixel bg_pixel;
	float zoom_inc;
		
	/* image data */
	XImage *image;	/* the X visual compatible data of complete image */
	XImage *bkbuf;	/* a view sized back-buffer for transformed image data */
	size_t bkbuf_size;	/* current back-buffer memory block size */
	
	/* file data */
	struct img_file img_file;	/* handle to the image loader */
	char *file_name;	/* the current file name */
	off_t file_size;	/* size of the file in bytes */
	
	/* directory reader thread data */
	pthread_t rdr_thread;
	pthread_cond_t rdr_finished_cond;
	pthread_mutex_t rdr_cond_mutex;

	/* directory cache data */
	char *dir_name;
	char **dir_files;	/* list of image file names */
	unsigned long dir_nfiles; /* dir_files size */
	unsigned long dir_cur_file;
	Boolean dir_forward;
	struct stat dir_stat; /* current directory stat */
	
	/* dialog data cache */
	char *last_dest_dir;
	Widget wfile_dlg;
	
	#ifdef ENABLE_CDE
	/* ToolTalk contract state */
	Tt_message tt_disp_req;	/* display request message */
	Tt_message tt_quit_req;	/* quit request message */
	#endif /* ENABLE_CDE */
	
	/* Thread notification pipe */
	int tnfd[2];
	XtInputId thread_notify_input;
	pthread_mutex_t thread_notify_mutex;	
	
	struct viewer_data *next;
};

/* viewer_data.state flags */
#define ISF_OPENED	0x01	/* handle to an image is valid */
#define ISF_LOADING	0x02	/* the loader thread is active */
#define ISF_READY	0x04	/* the image was successfully loaded */
#define ISF_CANCEL	0x08	/* loader cancelation pending */
#define DSF_READING 0x10	/* directory is being read */
#define DSF_CANCEL	0x20	/* directory reader cancelation pending */

#define MAX_ZOOM	12 /* Maximum zoom value */

/* Load progress (status window message) update interval in ms */
#define PROG_UPDATE_INT	120

/* Frame buffer update interval in ms when visual progress is enabled */
#define LOADER_FB_UPDATE_INT 120

/* Maximum scroll amount per key press (in pixels) */
#define MAX_KEY_PAN_AMOUNT	100

/* Initial size and the grow-by value of the directory cache */
#define DIR_CACHE_GROWBY 64

/* Loader thread callback data */
struct loader_cb_data {
	struct viewer_data *vd; /* the viewer */
	struct pixel_format image_pf; /* image file pixel format */
	unsigned char *clut; /* color lookup table (for 8 bpp images) */
	unsigned long nscl; /* number of scanlines read */
};

/* Thread message data */
enum tnfd_io { TNFD_IN, TNFD_OUT };
enum thread_proc { TP_IMG_LOAD, TP_DIR_READ };

struct proc_thread_msg {
	enum thread_proc proc;
	Boolean cancelled;
	int result;
};

#endif /* VIEWER_I_H */
