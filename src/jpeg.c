/*
 * Copyright (C) 2012-2017 alx@fastestcode.org
 * This software is distributed under the terms of the MIT license.
 * See the included LICENSE file for further information.
 */
 
/*
 * libjpeg bindings
 */

#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <setjmp.h>
#define JPEG_INTERNAL_OPTIONS
#include <jpeglib.h>
#include "imgfile.h"
#include "debug.h"

/* Loader data */
struct jpeg_ld {
	FILE *file;
	jmp_buf jmp;
	struct jpeg_decompress_struct cinfo;
	struct jpeg_error_mgr jerr;
};

/* Local prototypes */
static void close_image(struct img_file *img);
static void error_exit (j_common_ptr cinfo);
static void output_message (j_common_ptr cinfo);
static int read_scanlines(struct img_file *img,
	img_scanline_cbt cb, void *cdata);


static int read_scanlines(struct img_file *img,
	img_scanline_cbt cb, void *cdata)
{
	struct jpeg_ld *ld=(struct jpeg_ld*)img->loader_data;
	void *buf;
	unsigned long iscl;
	
	buf=malloc(ld->cinfo.output_width*ld->cinfo.output_components);
	if(!buf) return IMG_ENOMEM;

	if(setjmp(ld->jmp)){
		free(buf);
		jpeg_finish_decompress(&ld->cinfo);	
		return IMG_ENOMEM;
	}
	for(iscl=0; iscl<ld->cinfo.output_height; iscl++){
		void *bufptr[1]={buf};
		jpeg_read_scanlines(&ld->cinfo,(JSAMPARRAY)bufptr,1);
		if((*cb)(iscl,(void*)buf,cdata)==IMG_READ_CANCEL) break;
	}

	free(buf);
	return 0;
}

int img_open_jpeg(const char *file_name, struct img_file *img, int flags)
{
	struct stat st;
	struct jpeg_ld *ld;
	FILE *file;
	
	if(stat(file_name,&st))	return IMG_EIO;
	file=fopen(file_name,"r");
	if(!file) return IMG_EIO;
	
	memset(img,0,sizeof(struct img_file));
	
	ld=calloc(1,sizeof(struct jpeg_ld));
	if(!ld){
		fclose(file);
		return IMG_ENOMEM;
	}

	ld->file=file;
	ld->cinfo.client_data=ld;
	ld->cinfo.err=jpeg_std_error(&ld->jerr);
	ld->jerr.error_exit=error_exit;
	ld->jerr.output_message=output_message;

	if(setjmp(ld->jmp)){
		fclose(file);
		free(ld);
		return IMG_ENOMEM;
	}
	jpeg_create_decompress(&ld->cinfo);	
	jpeg_stdio_src(&ld->cinfo,file);

	if(setjmp(ld->jmp)){
		jpeg_destroy_decompress(&ld->cinfo);
		fclose(file);
		free(ld);
		return IMG_EFILE;
	}
	if(jpeg_read_header(&ld->cinfo,TRUE)!=JPEG_HEADER_OK){
		free(ld);
		fclose(file);
		jpeg_destroy_decompress(&ld->cinfo);
		return IMG_EFILE;
	}

	if(setjmp(ld->jmp)){
		jpeg_destroy_decompress(&ld->cinfo);
		fclose(file);
		free(ld);
		return IMG_ENOMEM;
	}
	jpeg_start_decompress(&ld->cinfo);
	img->width=ld->cinfo.output_width;
	img->height=ld->cinfo.output_height;
	img->orig_bpp=img->bpp=ld->cinfo.output_components*8;
	img->format=IMG_DIRECT;
	img->cr_time=st.st_mtime;
	img->loader_data=ld;
	img->red_mask=0x000000FF<<(RGB_RED*8);
	img->green_mask=0x000000FF<<(RGB_GREEN*8);
	img->blue_mask=0x000000FF<<(RGB_BLUE*8);
	img->read_scanlines_fnc=read_scanlines;
	img->close_fnc=close_image;
	
	return 0;
}

static void close_image(struct img_file *img)
{
	struct jpeg_ld *ld=(struct jpeg_ld*)img->loader_data;
	if(setjmp(ld->jmp)){
		fclose(ld->file);
		free(ld);
		return;
	}
	jpeg_finish_decompress(&ld->cinfo);	
	jpeg_destroy_decompress(&ld->cinfo);
	longjmp(ld->jmp,1);
}

/* Error handler overrides */
static void error_exit(j_common_ptr cinfo)
{
	struct jpeg_ld *ld=(struct jpeg_ld*)
		((struct jpeg_decompress_struct*)cinfo)->client_data;
	longjmp(ld->jmp,1);
}

static void output_message(j_common_ptr cinfo){	/* don't care */ }

