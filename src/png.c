/*
 * Copyright (C) 2012-2025 alx@fastestcode.org
 * This software is distributed under the terms of the MIT license.
 * See the included LICENSE file for further information.
 */
 
/*
 * libpng bindings
 */

#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <png.h>
#include "imgfile.h"
#include "debug.h"

/* Loader data */
struct png_ld {
	FILE *file;
	png_structp png;
	png_infop info;
	unsigned int passes;
};

/* Local prototypes */
static void close_image(struct img_file *img);
static int read_scanlines(struct img_file *img,
	img_scanline_cbt cb, void *cdata);
static void error_cb(png_structp png, png_const_charp msg);

static int read_scanlines(struct img_file *img,
	img_scanline_cbt cb, void *cdata)
{
	struct png_ld *ld=(struct png_ld*)img->loader_data;
	unsigned long iscl;
	int passes=ld->passes;
	void *buf;
	
	buf=malloc((img->bpp/8)*img->width);
	if(!buf) return IMG_ENOMEM;
	
	if(setjmp(png_jmpbuf(ld->png))){
		free(buf);
		return IMG_EFILE;
	}	
	
	while(passes){
		for(iscl=0; iscl<img->height; iscl++){
			png_read_row(ld->png,buf,NULL);
			if((*cb)(iscl,(void*)buf,cdata)==IMG_READ_CANCEL) break;
		}
		passes--;
	}
	
	free(buf);
	return 0;
}

int img_open_png(const char *file_name, struct img_file *img, int flags)
{
	struct stat st;
	struct png_ld *ld;
	FILE *file;
	uint8_t png_sig[8];
	png_uint_32 width, height;
	int bit_depth, clr_type;
	
	memset(img,0,sizeof(struct img_file));
	if(stat(file_name,&st)) return IMG_EIO;
	ld=calloc(1,sizeof(struct png_ld));
	if(!ld) return IMG_ENOMEM;
	file=fopen(file_name,"r");
	if(!file){
		free(ld);
		return IMG_EIO;
	}
	fread(png_sig,8,1,file);
	if(png_sig_cmp(png_sig,0,8)){
		free(ld);
		fclose(file);
		return IMG_EFILE;
	}
	rewind(file);
	ld->file=file;
	
	ld->png=png_create_read_struct(PNG_LIBPNG_VER_STRING,NULL,error_cb,NULL);
	if(ld->png)	ld->info=png_create_info_struct(ld->png);
	if(!ld->png || !ld->info){
		if(ld->png) png_destroy_read_struct(&ld->png,&ld->info,NULL);
		fclose(file);
		free(ld);
		return IMG_ENOMEM;
	}
	
	if(setjmp(png_jmpbuf(ld->png))){
		png_destroy_read_struct(&ld->png,&ld->info,NULL);
		free(ld);
		fclose(file);
		return IMG_EFILE;
	}
	png_init_io(ld->png,file);
	png_read_info(ld->png,ld->info);
	png_get_IHDR(ld->png,ld->info,&width,&height,&bit_depth,&clr_type,
		NULL,NULL,NULL);
	
	img->width=width;
	img->height=height;
	if(png_get_valid(ld->png,ld->info,PNG_INFO_bKGD)){
		png_color_16p bg;
		png_get_bKGD(ld->png,ld->info,&bg);
		img->bg_pixel=(bg->red|(bg->green<8)|(bg->blue<<16));
	}
	switch(clr_type){
		case PNG_COLOR_TYPE_RGB:
		if(png_get_valid(ld->png,ld->info,PNG_INFO_tRNS)){
			png_set_tRNS_to_alpha(ld->png);
			png_set_invert_alpha(ld->png);
			img->orig_bpp=24;
			img->bpp=32;
		}else{
			img->orig_bpp=img->bpp=24;
		}
		img->red_mask=0x000000FF;
		img->green_mask=0x0000FF00;
		img->blue_mask=0x00FF0000;
		if(img->bpp==32) img->alpha_mask=0xFF000000;
		break;
		case PNG_COLOR_TYPE_RGB_ALPHA:
		img->red_mask=0x000000FF;
		img->green_mask=0x0000FF00;
		img->blue_mask=0x00FF0000;
		img->alpha_mask=0xFF000000;
		img->orig_bpp=img->bpp=32;
		break;
		case PNG_COLOR_TYPE_PALETTE:
		png_set_palette_to_rgb(ld->png);
		if(png_get_valid(ld->png,ld->info,PNG_INFO_tRNS)){
			img->bpp=32;
		}else{
			img->bpp=24;
		}
		img->orig_bpp=bit_depth;
		img->red_mask=0x000000FF;
		img->green_mask=0x0000FF00;
		img->blue_mask=0x00FF0000;
		if(img->bpp==32) img->alpha_mask=0xFF000000;
		break;
		case PNG_COLOR_TYPE_GRAY:
		img->orig_bpp=bit_depth;
		img->bpp=8;
		if(bit_depth<8) png_set_expand_gray_1_2_4_to_8(ld->png);
		img->red_mask=0x000000FF;
		img->green_mask=0x000000FF;
		img->blue_mask=0x000000FF;
		img->alpha_mask=0;
		break;
		case PNG_COLOR_TYPE_GA:
		img->orig_bpp=bit_depth;
		img->bpp=16;
		if(bit_depth<8) png_set_expand_gray_1_2_4_to_8(ld->png);
		img->red_mask=0x000000FF;
		img->green_mask=0x000000FF;
		img->blue_mask=0x000000FF;
		img->alpha_mask=0x000FF00;
		break;
	}
	if(bit_depth==16) png_set_strip_16(ld->png);
	img->format=IMG_DIRECT;
	if(png_get_interlace_type(ld->png,ld->info)!=PNG_INTERLACE_NONE){
		ld->passes=png_set_interlace_handling(ld->png);		
	}else{
		ld->passes=1;
	}
	png_read_update_info(ld->png,ld->info);
	
	img->cr_time=st.st_mtime;
	img->loader_data=ld;
	img->read_scanlines_fnc=read_scanlines;
	img->close_fnc=close_image;
	
	return 0;
}

static void close_image(struct img_file *img)
{
	struct png_ld *ld=(struct png_ld*)img->loader_data;
	png_destroy_read_struct(&ld->png,&ld->info,NULL);
	fclose(ld->file);
	free(ld);
}

static void error_cb(png_structp png, png_const_charp msg){	/* don't care */ }
