/*
 * Copyright (C) 2012-2017 alx@fastestcode.org
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
	uint32 rps;
	uint32 tile_w;
	uint32 tile_h;
};

/* Local prototypes */
static void close_image(struct img_file *img);
static int read_strips(struct img_file *img,
	img_scanline_cbt cb, void *cdata);
static int read_tiles(struct img_file *img,
	img_scanline_cbt cb, void *cdata);
static int set_page(struct img_file *img, unsigned int page);

static int read_strips(struct img_file *img,
	img_scanline_cbt cb, void *cdata)
{
	unsigned long iscl;
	uint32 istr;
	struct tiff_ld *ld=(struct tiff_ld*)img->loader_data;
	uint32 *buffer;
	int cont;
	int img_errno=0;
	
	buffer=malloc((img->width*4)*ld->rps);
	
	if(!buffer) return IMG_ENOMEM;
	
	for(istr=0, iscl=0, cont=1; istr<(img->height/ld->rps) && cont; istr++){
		long irow;

		cont=TIFFReadRGBAStrip(ld->file,istr*ld->rps,buffer);
		if(!cont) img_errno=IMG_EFILE;
		for(irow=ld->rps-1; irow>=0 && cont; irow--){
			if((*cb)(iscl,(uint8_t*)
				&buffer[irow*img->width],cdata)==IMG_READ_CANCEL) cont=0;
			iscl++;
		}
	}
	
	free(buffer);
	return img_errno;
}

static int read_tiles(struct img_file *img,
	img_scanline_cbt cb, void *cdata)
{
	struct tiff_ld *ld=(struct tiff_ld*)img->loader_data;
	long iscl;
	long irow;
	uint32 xtiles, ytiles, ix, iy;
	uint32 *tile_buffer;
	uint32 *scl_buffer;
	int cont=1;
	int img_errno=0;
	
	xtiles=ceilf((float)img->width/ld->tile_w);
	ytiles=ceilf((float)img->height/ld->tile_h);
	
	tile_buffer=calloc((ld->tile_h*ld->tile_w)*xtiles,4);
	scl_buffer=calloc(ld->tile_w*xtiles,4);
		   
	if(!tile_buffer || !scl_buffer){
		if(tile_buffer) free(tile_buffer);
		if(scl_buffer) free(scl_buffer);
		return IMG_ENOMEM;
	}
	
	for(iy=0, iscl=0, cont=1; iy<ytiles && cont; iy++){
		for(ix=0; ix<xtiles && cont; ix++){
			cont=TIFFReadRGBATile(ld->file,ix*ld->tile_w,
				iy*ld->tile_h,&tile_buffer[ix*(ld->tile_w*ld->tile_h)]);
			if(!cont) img_errno=IMG_EFILE;
		}
		for(irow=ld->tile_h-1; irow>=0 && iscl<img->height; iscl++, irow--){
			int itile;
			for(itile=0; itile<xtiles; itile++){
				memcpy(&scl_buffer[itile*ld->tile_w],
					&tile_buffer[itile*(ld->tile_w*ld->tile_h)+irow*ld->tile_w],
					ld->tile_w*4);
			}
			if((*cb)(iscl,(uint8_t*)scl_buffer,cdata)==IMG_READ_CANCEL)
				cont=0;
		}
	}
	
	free(tile_buffer);
	free(scl_buffer);
	return img_errno;
}

int img_open_tiff(const char *file_name, struct img_file *img, int flags)
{
	struct stat st;
	struct tiff_ld *ld;
	int npages=0;
	uint32 width=0, height=0;
	uint16 bps=0, orientation=0;
	
	memset(img,0,sizeof(struct img_file));
	if(stat(file_name,&st)) return IMG_EIO;
	ld=calloc(1,sizeof(struct tiff_ld));
	if(!ld) return IMG_ENOMEM;
	
	ld->file=TIFFOpen(file_name,"r");
	if(!ld->file){
		free(ld);
		return IMG_EIO;
	}

	do {
		npages++;
	} while(TIFFReadDirectory(ld->file));
	
	if(!TIFFSetDirectory(ld->file,0) ||
		!TIFFGetField(ld->file,TIFFTAG_BITSPERSAMPLE,&bps) ||
		!TIFFGetField(ld->file,TIFFTAG_IMAGEWIDTH,&width) ||
		!TIFFGetField(ld->file,TIFFTAG_IMAGELENGTH,&height)){
			TIFFClose(ld->file);
			free(ld);
			return IMG_EFILE;
	}
	
	img->width=width;
	img->height=height;
	img->cr_time=st.st_ctime;
	
	img->format=IMG_DIRECT;
	img->bpp=32;
	img->red_mask=0x000000ff;
	img->green_mask=0x000ff00;
	img->blue_mask=0x00ff0000;
	img->alpha_mask=0xff000000;
	
	img->flags=IMGF_PMALPHA;
	img->orig_bpp=bps;
	img->npages=npages;	

	if(TIFFGetField(ld->file,TIFFTAG_ORIENTATION,&orientation)){
		switch(orientation){
			case ORIENTATION_BOTLEFT:
			img->tform=IMGT_VFLIP;
			break;
			case ORIENTATION_BOTRIGHT:
			img->tform=IMGT_HFLIP|IMGT_VFLIP;
			break;
			case ORIENTATION_TOPRIGHT:
			img->tform=IMGT_HFLIP;
			break;
		}
	}
	
	if(TIFFIsTiled(ld->file) && 
		TIFFGetField(ld->file,TIFFTAG_TILEWIDTH,&ld->tile_w) &&
		TIFFGetField(ld->file,TIFFTAG_TILELENGTH,&ld->tile_h)){
		img->read_scanlines_fnc=&read_tiles;
	}else if(TIFFGetField(ld->file,TIFFTAG_ROWSPERSTRIP,&ld->rps)){
		img->read_scanlines_fnc=&read_strips;
	}else{
		TIFFClose(ld->file);
		free(ld);
		return IMG_EUNSUP;
	}
		
	img->close_fnc=&close_image;
	if(npages>1) img->set_page_fnc=&set_page;
	img->loader_data=ld;
	
	#ifndef DEBUG
	TIFFSetWarningHandler(NULL);
	TIFFSetErrorHandler(NULL);
	#endif
		
	return 0;
}

static int set_page(struct img_file *img, unsigned int page)
{
	struct tiff_ld *ld=(struct tiff_ld*)img->loader_data;
	if(!TIFFSetDirectory(ld->file,page)) return IMG_EFILE;
	return 0;
}

static void close_image(struct img_file *img)
{
	struct tiff_ld *ld=(struct tiff_ld*)img->loader_data;
	TIFFClose(ld->file);
	free(ld);
}
