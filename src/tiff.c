/*
 * Copyright (C) 2012-2022 alx@fastestcode.org
 * This software is distributed under the terms of the MIT license.
 * See the included LICENSE file for further information.
 */
 
/*
 * libtiff bindings
 */
 
#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <math.h>
#include <tiffio.h>
#include "imgfile.h"
#include "debug.h"

struct tiff_ld {
	TIFF *file;
	void *data;
};

/* Local prototypes */
static void close_image(struct img_file *img);
static int read_scanlines(struct img_file *img,
	img_scanline_cbt cb, void *cdata);
static int read_directory(struct img_file *img, unsigned int ndir);

static int read_scanlines(struct img_file *img,
	img_scanline_cbt cb, void *cdata)
{
	struct tiff_ld *ld = (struct tiff_ld*) img->loader_data;
	unsigned int i;
	uint32_t *data = ld->data;
	
	for(i = 0; i < img->height; i++) {
		if( (*cb)(i, (uint8_t*)data, cdata) == IMG_READ_CANCEL) break;
		data += img->width;
	}
	return 0;
}

static int read_directory(struct img_file *img, unsigned int ndir)
{
	struct tiff_ld *ld = (struct tiff_ld*) img->loader_data;
	uint32_t width = 0;
	uint32_t height = 0;
	uint16_t bps = 0;
	uint16_t bpp = 0;

	if(ld->data) {
		free(ld->data);
		ld->data = NULL;
	}
	
	if(!TIFFSetDirectory(ld->file, ndir) ||
		!TIFFGetField(ld->file, TIFFTAG_BITSPERSAMPLE, &bps) ||
		!TIFFGetField(ld->file, TIFFTAG_IMAGEWIDTH, &width) ||
		!TIFFGetField(ld->file, TIFFTAG_IMAGELENGTH, &height) ) {
			return IMG_EFILE;
	}
	TIFFGetFieldDefaulted(ld->file, TIFFTAG_SAMPLESPERPIXEL, &bpp);

	img->width = width;
	img->height = height;
	
	img->format = IMG_DIRECT;
	img->bpp = 32;
	img->red_mask = 0x000000ff;
	img->green_mask = 0x000ff00;
	img->blue_mask = 0x00ff0000;
	img->alpha_mask = 0xff000000;
	img->flags = IMGF_PMALPHA;
	img->orig_bpp = bpp * bps;
	img->read_scanlines_fnc = &read_scanlines;

	ld->data = malloc((width * height) * 8);
	if(!ld->data) return IMG_ENOMEM;

	if(!TIFFReadRGBAImageOriented(
		ld->file, width, height, ld->data, ORIENTATION_TOPLEFT, 0) ) {
		free(ld->data);
		ld->data = NULL;
		return IMG_EUNSUP;
	}
	return 0;
}

int img_open_tiff(const char *file_name, struct img_file *img, int flags)
{
	struct stat st;
	struct tiff_ld *ld;
	int npages = 0;
	
	memset(img, 0, sizeof(struct img_file));
	if(stat(file_name, &st)) return IMG_EIO;
	ld = calloc(1, sizeof(struct tiff_ld));
	if(!ld) return IMG_ENOMEM;
	
	ld->file = TIFFOpen(file_name, "r");
	if(!ld->file){
		free(ld);
		return IMG_EIO;
	}

	do {
		npages++;
	} while(TIFFReadDirectory(ld->file));

	img->npages = npages;
	img->cr_time = st.st_ctime;	
	img->close_fnc = &close_image;
	if(npages > 1) img->set_page_fnc = &read_directory;
	img->loader_data = ld;
	
	#ifndef DEBUG
	TIFFSetWarningHandler(NULL);
	TIFFSetErrorHandler(NULL);
	#endif
	
	if(read_directory(img, 0)) {
		TIFFClose(ld->file);
		free(ld);
		return IMG_EIO;	
	}
	
	return 0;
}

static void close_image(struct img_file *img)
{
	struct tiff_ld *ld=(struct tiff_ld*)img->loader_data;
	TIFFClose(ld->file);
	if(ld->data) free(ld->data);
	free(ld);
}
