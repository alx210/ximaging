/*
 * Copyright (C) 2012-2024 alx@fastestcode.org
 * This software is distributed under the terms of the MIT license.
 * See the included LICENSE file for further information.
 */
 
/*
 * Constants type definitions and prototypes for image reading functions.
 */

#ifndef IMGFILE_H
#define IMGFILE_H

#include <time.h>
#include <inttypes.h>
#include "imgblt.h"

/* Transform flags */
#define IMGT_VFLIP	1	/* Flip vertically */
#define IMGT_HFLIP	2	/* Flip horizontally */
#define IMGT_ROTATE	4	/* Rotate clockwise */

/* Pixel format */
#define IMG_DIRECT		0	/* data is RGB(A) */
#define IMG_PSEUDO		1	/* color mapped (img_read_cmap is valid) */

/* Pixel format flags */
#define IMGF_PMALPHA	1	/* pixel values are premultiplied with alpha */
#define IMGF_BGCOLOR	2	/* image has background color */

/* Default color lookup table size */
#define IMG_CLUT_SIZE	768

/*
 * Callback function type for read_scanlines.
 * Called for each scanline with 'iscl' set to the scanline number,
 * 'scl_data' pointing to the internal scanline buffer, and 'client_data'
 * pointing to the data passed to img_read_scanlines. The callback Should
 * return IMG_READ_CONT to continue reading, or IMG_READ_CANCEL otherwise.
 */
typedef int (*img_scanline_cbt)
	(unsigned long iscl, const uint8_t *scl_data, void *client_data);
#define IMG_READ_CONT	1
#define IMG_READ_CANCEL	0

/* Image loader instance data */
struct img_file {
	unsigned long width;
	unsigned long height;
	short bpp;

	int npages;		/* number of pages in the image */	
	short format;	/* IMG_DIRECT/PSEUDO */
	short flags;	/* combination of IMGF* constants */

	/* RGB bitmasks */
	unsigned long red_mask;
	unsigned long green_mask;
	unsigned long blue_mask;
	unsigned long alpha_mask;
	uint32_t bg_pixel;
	
	/* image metadata */
	short orig_bpp;		/* original bpp (if converted by the loader) */
	unsigned int tform;	/* rotation flags IMGT* */
	time_t cr_time;		/* creation time */
	char *type_str;		/* descriptive string for the image type */
	
	/* pointers set up by the loader */
	void *loader_data;
	void (*close_fnc)(struct img_file*);
	int (*read_cmap_fnc)(struct img_file*,void*);
	int (*read_scanlines_fnc)(struct img_file*,img_scanline_cbt,void *cdata);
	int (*set_page_fnc)(struct img_file*,unsigned int);
};

/* Return values for the public imgfile functions */
#define IMG_ERROR	(-1)	/* unexpected error */
#define IMG_ENOMEM	(-2)	/* out of memory */
#define IMG_EUNSUP	(-3)	/* unsupported file format */
#define IMG_EFILE	(-4)	/* damaged or invalid file */
#define IMG_EINVAL	(-5)	/* invalid function call */
#define IMG_EIO		(-6)	/* read error */
#define IMG_EPERM	(-7)	/* no permission */

/* 'open' function type */
typedef int (*img_open_proc_t)(const char*,struct img_file*,int);

/* Image format descriptor */
struct img_format_info {
	char *desc;		/* description */
	char *suffixes;	/* space separated list of file suffixes */
	img_open_proc_t open_proc;
};

/* 
 * Returns file format info for suffix or fname (in that order of preference),
 * or NULL if unable to identify.
 */
struct img_format_info* const img_ident(const char *fname, const char *suffix);

/* Retrieve the list of supported image formats */
int img_get_formats(struct img_format_info const **ptr);

/* Open the image file (type_suffix may be NULL) */
int img_open(const char *file_name, const char *type_suffix,
	struct img_file *img, int flags);

/* Retrieve descriptive text for an IMG error code */
char * const img_strerror(int img_errno);

/* Inlines for calling the loader assigned functions */
static inline int img_read_cmap(struct img_file *img, void *buf){
	if(!img->read_cmap_fnc) return IMG_EINVAL;
	return img->read_cmap_fnc(img,buf);
}

static inline int img_read_scanlines(struct img_file *img,
	img_scanline_cbt cb, void *client){
	return img->read_scanlines_fnc(img,cb,client);
}

static inline int img_set_page(struct img_file *img, unsigned int page){
	return img->set_page_fnc(img,page);
}

static inline void img_close(struct img_file *img){
	img->close_fnc(img);
}


#endif /* IMGFILE_H */
