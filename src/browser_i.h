/*
 * Copyright (C) 2012-2017 alx@fastestcode.org
 * This software is distributed under the terms of the MIT license.
 * See the included LICENSE file for further information.
 */
 
/*
 * Private browser data structures, constants and typedefs
 */

#ifndef BROWSER_I_H
#define BROWSER_I_H

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

/* Browser file states */
enum file_state {
	FS_PENDING,
	FS_BROKEN,
	FS_ERROR,
	FS_VIEWABLE,
	_NUM_FS_VALUES
};

/* Tile size presets */
enum tile_preset{
	TS_SMALL,
	TS_MEDIUM,
	TS_LARGE,
	_NUM_TS_PRESETS
};

/* File info container */
struct browser_file {
	char *name;
	XmString label;
	Dimension label_width;
	XImage *image;
	enum file_state state;
	Boolean selected;
	
	/* image metadata */
	unsigned long xres;
	unsigned long yres;
	unsigned short bpp;
	int loader_result;
	time_t time;
};

/* Browser instance data */
struct browser_data {
	Widget wshell;
	Widget wmain;
	Widget wmenubar;
	Widget wview;
	Widget wvscroll;
	Widget wmsg;
	Widget wpopup;
	Widget wnavbar;
	Widget wdlscroll;
	Widget wdirlist;
	Widget wpaned;
	
	/* Motif colors */
	Pixel fg_pixel;	/* foreground (text) */
	Pixel bg_pixel;	/* background */
	Pixel bs_pixel;	/* bottom shadow */
	Pixel ts_pixel;	/* top shadow */
	Pixel sbg_pixel; /* selected background */
	
	/* shell state */
	Boolean pinned;	/* the shell is pinned; don't reuse */
	char *title;	/* base shell title */
	
	/* loader data */
	pthread_t ldr_thread;
	pthread_t rdr_thread;
	pthread_cond_t ldr_cond;
	pthread_mutex_t ldr_cond_mutex;
	pthread_cond_t rdr_cond;
	pthread_mutex_t rdr_cond_mutex;
	pthread_mutex_t data_mutex;
		
	/* view data */
	Dimension view_width; /* cached view widget size */
	Dimension view_height;
	unsigned short state;
	int border_width; /* tile border width in pixels */
	struct browser_file *files;
	long nfiles; /* number of entries in 'files' */
	char **subdirs; /* subdirectories */
	long nsubdirs;
	long nsel_files; /* number of selected files */
	long ifocus; /* focused tile */
	Boolean owns_primary;
	char *path;	/* current path */
	time_t dir_modtime; /* modification time of the current directory */
	time_t latest_file_mt; /* latest file modification time */
	size_t path_max;	/* max path+file name characters */
	XmRenderTable render_table;	/* for labels */
	Dimension max_label_height; /* maximum label height in pixels */
	GC draw_gc;
	GC text_gc;
	XtIntervalId dblclk_timer; /* doubleclick timer */
	Dimension yoffset; /* vertical scroll offset */
	long sel_start; /* selection start index */
	int refresh_int; /* refresh interval */
	XtIntervalId update_timer; /* modification check timer */
	char *last_dest_dir; /* last move/copy to directory */
	Boolean show_dot_files;
	
	/* tile aspect ratio and size */
	short tile_asr[2];
	short tile_size[_NUM_TS_PRESETS];
	enum tile_preset itile_size;

	#ifdef ENABLE_CDE
	/* ToolTalk contract state */	
	Tt_message tt_disp_req;	/* display request message */
	Tt_message tt_quit_req;	/* quit request message */
	#endif /* ENABLE_CDE */

	/* thread notification pipe */
	int tnfd[2];
	XtInputId thread_notify_input;

	struct browser_data *next;
};

/* browser_data.state flags */
#define BSF_READING	0x0001	/* directory reader thread is active */
#define BSF_LOADING	0x0002	/* image loader thread is active */
#define BSF_RCANCEL	0x0004	/* cancelling reader thread */
#define BSF_LCANCEL	0x0008	/* cancelling loader thread */
#define BSF_READY	0x0010	/* data available */
#define BSF_RESET	0x0020	/* reset state */

/* Initial file list size and grow-by */
#define FILE_LIST_GROWBY	256

/* Margin between tiles */
#define TILE_XMARGIN	4
#define TILE_YMARGIN	4

/* Padding around the image inside a tile */
#define TILE_PADDING	2

/* Vertical margin between tiles and labels */
#define LABEL_MARGIN	2

/* Loader thread callback data */
struct loader_cb_data {
	struct browser_data *bd; /* the browser */
	long ifile; /* index of the file entry */
	struct pixel_format display_pf; /* display pixel format */
	struct pixel_format image_pf; /* source image pixel format */
	struct img_file img_file; /* image reader handle */
	uint8_t clut[IMG_CLUT_SIZE]; /* color lookup table for 8bpp images */
	XImage *buf_image; /* intermediate storage for the full sized image */
};

/* Directory thread notification message data */
enum tmsg_code {
	TMSG_ADD,
	TMSG_REMOVE,
	TMSG_UPDATE,
	TMSG_RELOAD,
	TMSG_FINISHED,
	TMSG_CANCELLED
};

/* TMSG_ADD/REMOVE */
struct tmsg_change_data {
	char **files;
	char **dirs;
	long nfiles;
	long ndirs;
};

/* TMSG_UPDATE */
struct tmsg_update_data {
	long index;
	int status;
};

/* TMSG_RELOAD/FINISHED/CANCELLED */
struct tmsg_notify_data {
	int reason;
	int status;
};

struct thread_msg {
	enum tmsg_code code;
	union {
		struct tmsg_change_data change_data;
		struct tmsg_update_data update_data;
		struct tmsg_notify_data notify_data;
	};
};

enum tnfd_io {
	TNFD_IN,
	TNFD_OUT
};

/* Selection modes */
enum sel_mode {
	SM_SINGLE,
	SM_MULTI,
	SM_EXTEND
};

#endif /* BROWSER_I_H */
