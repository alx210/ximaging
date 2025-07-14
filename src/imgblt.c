/*
 * Copyright (C) 2012-2025 alx@fastestcode.org
 * This software is distributed under the terms of the MIT license.
 * See the included LICENSE file for further information.
 */
 
/*
 * XImage blitting routines.
 */

#include <stdlib.h>
#include <memory.h>
#include <inttypes.h>
#include <math.h>
#include "common.h"
#include "imgfile.h"
#include "imgblt.h"
#include "debug.h"

/* Pixel set/get function pointer types */
typedef unsigned long (*get_pixel_fnc_t)(XImage*,int,int);
typedef int (*set_pixel_fnc_t)(XImage*,int,int,unsigned long);

/* Local prototypes */
#ifndef USE_XIMAGE_PIXFNC
static void select_pixel_func(int bpp, set_pixel_fnc_t *set_fnc,
	get_pixel_fnc_t *get_fnc);
static unsigned long get_pixel8(XImage *img, int x, int y);
static unsigned long get_pixel16(XImage *img, int x, int y);
static unsigned long get_pixel24(XImage *img, int x, int y);
static unsigned long get_pixel32(XImage *img, int x, int y);
static int set_pixel8(XImage *img, int x, int y, unsigned long pixel);
static int set_pixel16(XImage *img, int x, int y, unsigned long pixel);
static int set_pixel24(XImage *img, int x, int y, unsigned long pixel);
static int set_pixel32(XImage *img, int x, int y, unsigned long pixel);
#endif /* USE_XIMAGE_PIXFNC */

/*
 * Transform and blit a rectangle of pixels from 'src' to 'dest'
 */
void img_blt(XImage *src, unsigned int sxc, unsigned int syc,
	unsigned int sw, unsigned int sh, XImage *dest,
	float scale, short transfm, short flags)
{
	float int_sx, sx = 0, sy = 0;
	float inc = (1.0 / scale);
	unsigned int dx = 0, dy = 0;
	unsigned int del_x, del_y;
	get_pixel_fnc_t get_pixel_fnc;
	set_pixel_fnc_t set_pixel_fnc;
	
	#ifdef USE_XIMAGE_PIXFNC
	get_pixel_fnc=src->f.get_pixel;
	set_pixel_fnc=dest->f.put_pixel;
	#else
	select_pixel_func(dest->bitmap_pad,&set_pixel_fnc,&get_pixel_fnc);
	#endif /* USE_XIMAGE_PIXFUNC */

	/* Discard interpolation options for unsupported pixel formats */
	if(app_inst.visual_info.class != TrueColor)
		flags &= ~BLTF_INTERPOLATE;
	
	/* figure out initial coordinates */
	int_sx = (ceilf((float)sxc * scale) / scale);
	sy = (ceilf((float)syc * scale) / scale);
	del_x = (float)sw * scale;
	del_y = (float)sh * scale;
	
	/* clip source coordinates to the destination image */
	if(transfm & IMGT_ROTATE){
		if(del_x > dest->height) del_x = dest->height;
		if(del_y > dest->width) del_y=dest->width;	
		/* rotation (as done below) causes vertical flip */
		transfm ^= IMGT_VFLIP;
	}else{
		if(del_x > dest->width) del_x = dest->width;
		if(del_y > dest->height) del_y = dest->height;	
	}
	
	/*  bi-linear interpolation */
	if((scale >= 0.5 && scale < 1.0) ||
		((scale > 1.0) && (flags & BLTF_INT_UP))){
		for(dy = 0, sx = int_sx; dy < del_y; dy++){
			#ifdef ENABLE_OMP
			#pragma omp parallel for firstprivate(dy)
			#endif
			for(dx = 0; dx < del_x; dx++){
				float fsx = sx + (dx * inc);
				float fsy = sy + (dy * inc);
				float fx = fsx - floorf(fsx);
				float fy = fsy - floorf(fsy);
				unsigned int csx = (fsx + 1 < src->width) ? ceilf(fsx):fsx;
				unsigned int csy = (fsy + 1 < src->height) ? ceilf(fsy):fsy;
				unsigned long psrc[4];
				unsigned long pixel;
				unsigned int x, y;
				
				psrc[0] = get_pixel_fnc(src, fsx, fsy);
				psrc[1] = get_pixel_fnc(src, csx, fsy);
				psrc[2] = get_pixel_fnc(src, csx, csy);
				psrc[3] = get_pixel_fnc(src, fsx, csy);
				
				pixel = (unsigned long)(
					(1 - fx) * (1 - fy) * (psrc[0] & src->red_mask) +
					fx * (1 - fy) * (psrc[1] & src->red_mask) +
					fx * fy * (psrc[2] & src->red_mask) +
					(1 - fx) * fy * (psrc[3] & src->red_mask)) &
					dest->red_mask;
				
				pixel |= (unsigned long)
					((1 - fx) * (1 - fy) * (psrc[0] & src->green_mask) +
					fx * (1 - fy) * (psrc[1] & src->green_mask) +
					fx * fy * (psrc[2] & src->green_mask) +
					(1 - fx) * fy * (psrc[3] & src->green_mask)) &
					dest->green_mask;
				
				pixel |= (unsigned long)
					(( 1 - fx) * (1 - fy) * (psrc[0] & src->blue_mask) +
					fx * (1 - fy) * (psrc[1] & src->blue_mask) +
					fx * fy * (psrc[2] & src->blue_mask) +
					(1 - fx) * fy * (psrc[3] & src->blue_mask)) &
					dest->blue_mask;
				
				if(transfm&IMGT_HFLIP) x=(del_x-1)-dx; else x=dx;
				if(transfm&IMGT_VFLIP) y=(del_y-1)-dy; else y=dy;
				if(transfm&IMGT_ROTATE){ unsigned int t=x; x=y; y=t; }
				set_pixel_fnc(dest,x,y,pixel);
			}
		}
	}else if((flags & BLTF_INT_DOWN) && (scale < 0.5)){
		/* decimation with sample averaging */
		unsigned int bw = (0.5 + sw / (0.5 + sw * scale));
		unsigned int bh = (0.5 + sh / (0.5 + sh * scale));
		float box = bw * bh;

		/* Round up if we're sub-pixel, so we can generate thumbnails for
		 * 'thin' images, albeit with incorrect ratio */
		del_x = (del_x) ? del_x : 1;
		del_y = (del_y) ? del_y : 1;

		for(dy = 0, sx = int_sx; dy < del_y; dy++){
			#ifdef ENABLE_OMP
			#pragma omp parallel for firstprivate(dy)
			#endif
			for(dx=0; dx < del_x; dx++){
				float red=0;
				float green=0;
				float blue=0;
				unsigned long pixel;
				unsigned int x, y;
				unsigned int bx, by;
				
				for(by = 0; by < bh; by++){
					for(bx = 0; bx < bw; bx++){
						pixel = get_pixel_fnc(src,
							sx + (inc * dx) + bx,
							sy + (inc * dy) + by);
						red += (float)(pixel & src->red_mask) / box;
						green += (float)(pixel & src->green_mask) / box;
						blue += (float)(pixel & src->blue_mask) / box;
					}
				}
				pixel = ((unsigned long)red & src->red_mask)|
					((unsigned long)green & src->green_mask)|
					((unsigned long)blue & src->blue_mask);
				
				if(transfm&IMGT_HFLIP) x=(del_x-1)-dx; else x=dx;
				if(transfm&IMGT_VFLIP) y=(del_y-1)-dy; else y=dy;
				if(transfm&IMGT_ROTATE){ unsigned int t=x; x=y; y=t; }
				set_pixel_fnc(dest, x, y, pixel);
			}
		}
	}else{
		/* point sampling */
		for(dy = 0, sx = int_sx; dy < del_y; dy++){
			#ifdef ENABLE_OMP
			#pragma omp parallel for firstprivate(dy)
			#endif
			for(dx = 0; dx < del_x; dx++){
				unsigned long pixel;
				unsigned int x, y;

				pixel = get_pixel_fnc(src,
					(sx + inc * dx), (sy + inc * dy));

				if(transfm&IMGT_HFLIP) x=(del_x-1)-dx; else x=dx;
				if(transfm&IMGT_VFLIP) y=(del_y-1)-dy; else y=dy;
				if(transfm&IMGT_ROTATE){ unsigned int t=x; x=y; y=t; }
				set_pixel_fnc(dest,x,y,pixel);
			}
		}
	}
}


/*
 * Fill given rectangle in ximage with a pixel value.
 */
void img_fill_rect(XImage *img, unsigned int x, unsigned int y,
	unsigned int width, unsigned int height, unsigned long pixel)
{
	unsigned int cx, cy;
	set_pixel_fnc_t set_pixel_fnc;
	
	dbg_assert(x+width<=img->width && y+height<=img->height);
	#ifdef USE_XIMAGE_PIXFNC
	set_pixel_fnc=img->f.put_pixel;
	#else
	select_pixel_func(img->bitmap_pad,&set_pixel_fnc,NULL);
	#endif /* USE_XIMAGE_PIXFNC */

	for(cy=y; cy<height; cy++){
		for(cx=x; cx<width; cx++){
			set_pixel_fnc(img,cx,cy,pixel);
		}
	}
}

#ifndef USE_XIMAGE_PIXFNC

/*
 * Returns appropriate pixel functions for 'bpp'.
 * set_fnc/get_fnc may be NULL.
 */
static void select_pixel_func(int bpp, set_pixel_fnc_t *set_fnc,
	get_pixel_fnc_t *get_fnc)
{
	set_pixel_fnc_t set = NULL;
	get_pixel_fnc_t get = NULL;
	switch(bpp){
		case 8:
		get=get_pixel8;
		set=set_pixel8;
		break;
		case 15:
		case 16:
		get=get_pixel16;
		set=set_pixel16;
		break;
		case 24:
		get=get_pixel24;
		set=set_pixel24;
		break;
		case 32:
		get=get_pixel32;
		set=set_pixel32;
		break;
		default:
		dbg_trap("unsupported bit-depth %d",bpp);
		break;
	}
	if(set_fnc) *set_fnc=set;
	if(get_fnc) *get_fnc=get;
}

/*
 * Pixel pushing routines.
 */
static unsigned long get_pixel8(XImage *img, int x, int y)
{
	dbg_assert(x<img->width && y<img->height);
	return img->data[img->width*y+x];
}

static unsigned long get_pixel16(XImage *img, int x, int y)
{
	dbg_assert(x<img->width && y<img->height);
	return ((uint16_t*)img->data)[img->width*y+x];
}

static unsigned long get_pixel32(XImage *img, int x, int y)
{
	dbg_assert(x<img->width && y<img->height);
	return ((uint32_t*)img->data)[img->width*y+x];
}

static unsigned long get_pixel24(XImage *img, int x, int y)
{
	uint32_t pixel=0;
	dbg_assert(x<img->width && y<img->height);
	memcpy(&pixel,&img->data[img->bytes_per_line*y+x*3],3);
	return pixel;
}

static int set_pixel8(XImage *img, int x, int y, unsigned long pixel)
{
	dbg_assert(x<img->width && y<img->height);
	img->data[img->width*y+x]=(uint8_t)pixel;
	return 0;
}

static int set_pixel16(XImage *img, int x, int y, unsigned long pixel)
{
	dbg_assert(x<img->width && y<img->height);
	((uint16_t*)img->data)[img->width*y+x]=(uint16_t)pixel;
	return 0;
}

static int set_pixel24(XImage *img, int x, int y, unsigned long pixel)
{
	dbg_assert(x<img->width && y<img->height);
	memcpy(&img->data[img->bytes_per_line*y+x*3],&pixel,3);
	return 0;
}

static int set_pixel32(XImage *img, int x, int y, unsigned long pixel)
{
	dbg_assert(x<img->width && y<img->height);
	((uint32_t*)img->data)[img->width*y+x]=pixel;
	return 0;
}

#endif /* USE_XIMAGE_PIXFNC */
