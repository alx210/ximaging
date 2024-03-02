/*
 * Copyright (C) 2012-2024 alx@fastestcode.org
 * This software is distributed under the terms of the MIT license.
 * See the included LICENSE file for further information.
 */
 
/*
 * Pixel format conversion routines.
 */

#include <stdlib.h>
#include <memory.h>
#include <math.h>
#include "pixconv.h"
#include "imgfile.h"
#include "bswap.h"
#include "cmap.h"
#include "debug.h"

/* Local prototypes */
static void* write_pixel8(const struct pixel_format *pf,
	void *ptr, uint32_t r, uint32_t g, uint32_t b, uint32_t a);
static void* write_pixel16(const struct pixel_format *pf,
	void *ptr, uint32_t r, uint32_t g, uint32_t b, uint32_t a);
static void* write_pixel24(const struct pixel_format *pf,
	void *ptr, uint32_t r, uint32_t g, uint32_t b, uint32_t a);
static void* write_pixel24_be(const struct pixel_format *pf,
	void *ptr, uint32_t r, uint32_t g, uint32_t b, uint32_t a);
static void* write_pixel32(const struct pixel_format *pf,
	void *ptr, uint32_t r, uint32_t g, uint32_t b, uint32_t a);
static void* write_pixel32_l(const struct pixel_format *pf,
	void *ptr, uint32_t r, uint32_t g, uint32_t b, uint32_t a);
static void* write_pf_unsup(const struct pixel_format *pf,
	void *ptr, uint32_t r, uint32_t g, uint32_t b, uint32_t a);
static void* read_pixel8(const struct pixel_format *pf,
	const void *ptr, uint8_t *r, uint8_t *g, uint8_t *b, uint8_t *a);
static void* read_pixel16gsa(const struct pixel_format *pf,
	const void *ptr, uint8_t *r, uint8_t *g, uint8_t *b, uint8_t *a);
static void* read_pixel16rgb(const struct pixel_format *pf,
	const void *ptr, uint8_t *r, uint8_t *g, uint8_t *b, uint8_t *a);
static void* read_pixel24(const struct pixel_format *pf,
	const void *ptr, uint8_t *r, uint8_t *g, uint8_t *b, uint8_t *a);
static void* read_pixel24_be(const struct pixel_format *pf,
	const void *ptr, uint8_t *r, uint8_t *g, uint8_t *b, uint8_t *a);
static void* read_pixel32(const struct pixel_format *pf,
	const void *ptr, uint8_t *r, uint8_t *g, uint8_t *b, uint8_t *a);
static void* read_pixel32_l(const struct pixel_format *pf,
	const void *ptr, uint8_t *r, uint8_t *g, uint8_t *b, uint8_t *a);

/* Convenience inlines for pixel_format pixel proc calls */
static inline void* read_pixel(const struct pixel_format *pf,
	const void *ptr, uint8_t *r, uint8_t *g, uint8_t *b, uint8_t *a){
	return (*pf->read_pixel_fnc)(pf,ptr,r,g,b,a);
}
static inline void* write_pixel(const struct pixel_format *pf,
	void *ptr, uint32_t r, uint32_t g, uint32_t b, uint32_t a){
	return (*pf->write_pixel_fnc)(pf,ptr,r,g,b,a);
}

/* Default background color */
static const uint8_t def_bg_rgb[]={180,180,180};

/*
 * Convert RGB pixels.
 * 'src/dest' 'pf_src/dest' must point to the source/destination buffers and
 * according pixel format structures. 'npix' must specify number of pixels
 * to be converted. 'bg_color' may include the background color (only used
 * if 'src' contains alpha channel).
 */
void convert_rgb_pixels(void *dest, const struct pixel_format *pf_dest,
	const void *src, const struct pixel_format *pf_src, unsigned int npix)
{
	int i;
	uint8_t r,g,b,a;
	const void *psrc=src;
	void *pdest=dest;
		
	if(pf_src->alpha_mask){
		uint8_t const *bg=(pf_src->flags&IMGF_BGCOLOR)?
			pf_src->bg_color:def_bg_rgb;
		
		if(pf_src->flags&IMGF_PMALPHA){
			for(i=0; i < npix; i++) {
				psrc=read_pixel(pf_src,psrc,&r,&g,&b,&a);
				r+=(((255-a)*bg[0])>>8);
				g+=(((255-a)*bg[1])>>8);
				b+=(((255-a)*bg[2])>>8);
				pdest=write_pixel(pf_dest,pdest,r,g,b,0);
			}
		}else{
			for(i=0; i < npix; i++) {
				psrc=read_pixel(pf_src,psrc,&r,&g,&b,&a);
				r=(r*a+(255-a)*bg[0])>>8;
				g=(g*a+(255-a)*bg[1])>>8;
				b=(b*a+(255-a)*bg[2])>>8;
				pdest=write_pixel(pf_dest,pdest,r,g,b,0);
			}
		}		
	}else{
		for(i=0; i < npix; i++) {
			psrc=read_pixel(pf_src,psrc,&r,&g,&b,&a);
			pdest=write_pixel(pf_dest,pdest,r,g,b,0);
		}
	}
}


/*
 * Convert RGB pixels to CLUT pixels according to the given colormap.
 */
void rgb_pixels_to_clut(void *dest, const void *src,
	 const struct pixel_format *pf_src, unsigned int npix)
{
	int i;
	uint8_t r,g,b,a;
	const void *psrc=src;
	uint8_t *pdest=dest;

	if(pf_src->alpha_mask){
		uint8_t const *bg=(pf_src->flags&IMGF_BGCOLOR)?
			pf_src->bg_color:def_bg_rgb;
		
		if(pf_src->flags&IMGF_PMALPHA){
			for(i=0; i < npix; i++) {
				psrc=read_pixel(pf_src,psrc,&r,&g,&b,&a);
				r+=(((255-a)*bg[0])>>8);
				g+=(((255-a)*bg[1])>>8);
				b+=(((255-a)*bg[2])>>8);
				pdest[i]=cm_match_rgb(r,g,b);
			}
		}else{
			for(i=0; i < npix; i++) {
				psrc=read_pixel(pf_src,psrc,&r,&g,&b,&a);
				r=(r*a+(255-a)*bg[0])>>8;
				g=(g*a+(255-a)*bg[1])>>8;
				b=(b*a+(255-a)*bg[2])>>8;
				pdest[i]=cm_match_rgb(r,g,b);
			}
		}		
	}else{
		for(i=0; i < npix; i++) {
			psrc=read_pixel(pf_src,psrc,&r,&g,&b,&a);
			pdest[i]=cm_match_rgb(r,g,b);
		}
	}
}

/*
 * Remap CLUT pixels according to the given colormap.
 */
void remap_pixels(void *dest, const void *src,
	const uint8_t *clut, unsigned int npix)
{
	unsigned int i;
	uint8_t *dp=(uint8_t*)dest;
	uint8_t *sp=(uint8_t*)src;

	for(i=0; i<npix; i++){
		dp[i]=cm_match_rgb(clut[sp[i]*3],
			clut[sp[i]*3+1],clut[sp[i]*3+2]);
	}
}

/*
 * Convert CLUT pixels to RGB pixels.
 * 'src' and 'clut' must point to the source image data and its RGB lookup
 * table. 'dest' and 'pf_dest' must point to the destination storage and
 * desired pixel format. 'npix' must specify the number of pixels to convert.
 */
void clut_to_rgb_pixels(void *dest, const struct pixel_format *pf_dest,
	const uint8_t *src, const uint8_t *clut, unsigned int npix)
{
	int i;
	uint32_t r,g,b;
	
	for(i=0; i<npix; i++){
		r=clut[src[i]*3];
		g=clut[src[i]*3+1];
		b=clut[src[i]*3+2];
		dest=write_pixel(pf_dest,dest,r,g,b,0);
	}
}

/*
 * Initialize the pixel_format structure.
 * 'flags' must be a combination of IMGF_* flags or zero.
 */
int init_pixel_format(struct pixel_format *pf, uint8_t depth,
	uint32_t red_mask, uint32_t green_mask, uint32_t blue_mask,
	uint32_t alpha_mask, uint32_t bg_pixel, short flags)
{
	int res=0;	
	int i;
	uint32_t rgba_masks[4]={red_mask,green_mask,blue_mask,alpha_mask};
	
	for(i=0; i<4; i++){
		int bits=0;
		uint32_t mask=rgba_masks[i];
		if(!mask){
			pf->e[i*2]=0;
			pf->e[i*2+1]=0;
			continue;
		}
		while(!(mask & 0x00000001)){
			bits++;
			mask >>= 1;
		}
		pf->e[i*2]=bits;
		bits=0;
		while(mask & 0x00000001){
			bits++;
			mask >>= 1;
		}
		pf->e[i*2+1]=bits;
	}
	pf->red_mask=red_mask;
	pf->green_mask=green_mask;
	pf->blue_mask=blue_mask;
	pf->alpha_mask=alpha_mask;
	pf->depth=depth;
	pf->flags=flags;
	pf->pixel_size=ceilf((float)depth/8);
	
	switch(depth){
		case 8:
		pf->read_pixel_fnc=&read_pixel8;
		pf->write_pixel_fnc=&write_pixel8;
		break;
		case 15:
		case 16:
		if(alpha_mask){
			/* grayscale + alpha */
			pf->read_pixel_fnc=&read_pixel16gsa;
			pf->write_pixel_fnc=&write_pf_unsup;
		}else{
			/* 555/565 RGB */
			pf->read_pixel_fnc=&read_pixel16rgb;
			pf->write_pixel_fnc=&write_pixel16;
		}
		break;
		case 24:
		if(is_big_endian()){
			pf->read_pixel_fnc=&read_pixel24_be;
			pf->write_pixel_fnc=&write_pixel24_be;
		}else{
			pf->read_pixel_fnc=&read_pixel24;
			pf->write_pixel_fnc=&write_pixel24;
		}
		break;
		case 32:
		if(pf->rfw+pf->gfw+pf->bfw > 24){
			/* 10 bpc RGB */
			pf->read_pixel_fnc=&read_pixel32_l;
			pf->write_pixel_fnc=&write_pixel32_l;
		}else{
			/* 8 bpc RGBA */
			pf->read_pixel_fnc=&read_pixel32;
			pf->write_pixel_fnc=&write_pixel32;
		}
		break;
		default:
		res=IMG_EUNSUP;
		break;
	}
	
	/* extract background color if any */
	if(flags&IMGF_BGCOLOR){
		uint8_t unused;
		pf->read_pixel_fnc(pf,&bg_pixel,&pf->bg_color[0],
			&pf->bg_color[1],&pf->bg_color[2],&unused);
	}else{
		memset(pf->bg_color,255,3);
	}

	return res;
}

/*
 * Pixel de/composition routines. Each returns 'ptr'+'pixel size'.
 */

static void* write_pixel16(const struct pixel_format *pf,
	void *ptr, uint32_t r, uint32_t g, uint32_t b, uint32_t a)
{
	uint16_t *dest=(uint16_t*)ptr;
	r>>=((pf->rfw<6)?3:2);
	g>>=((pf->gfw<6)?3:2);
	b>>=((pf->bfw<6)?3:2);
	*dest=(r<<pf->rfo)|(g<<pf->gfo)|(b<<pf->bfo);
	return dest+1;
}

static void* write_pixel24(const struct pixel_format *pf,
	void *ptr, uint32_t r, uint32_t g, uint32_t b, uint32_t a)
{
	uint32_t pixel;
	uint8_t *dest=(uint8_t*)ptr;
	pixel=((r<<pf->rfo)|(g<<pf->gfo)|(b<<pf->bfo));
	dest[2]=(pixel>>16);
	dest[1]=(pixel>>8);
	dest[0]=pixel;
	return dest+=3;
}

static void* write_pixel24_be(const struct pixel_format *pf,
	void *ptr, uint32_t r, uint32_t g, uint32_t b, uint32_t a)
{
	uint32_t pixel;
	uint8_t *dest=(uint8_t*)ptr;
	pixel=((r<<pf->rfo)|(g<<pf->gfo)|(b<<pf->bfo));
	dest[0]=(pixel>>24);
	dest[1]=(pixel>>16);
	dest[2]=(pixel>>8);
	return dest+=3;
}

static void* write_pixel32(const struct pixel_format *pf,
	void *ptr, uint32_t r, uint32_t g, uint32_t b, uint32_t a)
{
	uint32_t *dest=(uint32_t*)ptr;
	*dest=((r<<pf->rfo)|(g<<pf->gfo)|(b<<pf->bfo)|(a<<pf->afo));
	return dest+1;
}

static void* write_pixel32_l(const struct pixel_format *pf,
	void *ptr, uint32_t r, uint32_t g, uint32_t b, uint32_t a)
{
	uint32_t *dest=(uint32_t*)ptr;
	if(pf->rfw>8) r>>=(pf->rfw-8);
	if(pf->gfw>8) g>>=(pf->gfw-8);
	if(pf->bfw>8) b>>=(pf->bfw-8);
	*dest=((r<<pf->rfo)|(g<<pf->gfo)|(b<<pf->bfo));
	return dest+1;
}

static void* write_pixel8(const struct pixel_format *pf,
	void *ptr, uint32_t r, uint32_t g, uint32_t b, uint32_t a)
{
	uint8_t *dest=(uint8_t*)ptr;
	*dest=(r+g+b)/255;
	return dest+1;
}

static void* read_pixel16rgb(const struct pixel_format *pf,
	const void *ptr, uint8_t *r, uint8_t *g, uint8_t *b, uint8_t *a)
{
	uint16_t *src=(uint16_t*)ptr;
	/* 5/6 bit values get scaled up to 8 */
	*r=((src[0]&pf->red_mask)>>pf->rfo)<<((pf->rfw<6)?3:2);
	*g=((src[0]&pf->green_mask)>>pf->gfo)<<((pf->gfw<6)?3:2);
	*b=((src[0]&pf->blue_mask)>>pf->bfo)<<((pf->bfw<6)?3:2);
	*a=0;
	return src+1;
}

static void* read_pixel16gsa(const struct pixel_format *pf,
	const void *ptr, uint8_t *r, uint8_t *g, uint8_t *b, uint8_t *a)
{
	uint16_t *src=(uint16_t*)ptr;
	/* greyscale + aplha channel */
	*g=*b=*r=((src[0]&pf->red_mask)>>pf->rfo);
	*a=((src[0]&pf->alpha_mask)>>pf->afo);
	return src+1;
}

static void* read_pixel24(const struct pixel_format *pf,
	const void *ptr, uint8_t *r, uint8_t *g, uint8_t *b, uint8_t *a)
{
	uint8_t *src=(uint8_t*)ptr;
	uint32_t pixel=
		((uint32_t)src[2]<<16)|((uint32_t)src[1]<<8)|src[0];
	*r=(pixel&pf->red_mask)>>pf->rfo;
	*g=(pixel&pf->green_mask)>>pf->gfo;
	*b=(pixel&pf->blue_mask)>>pf->bfo;
	*a=0;
	return src+3;
}

static void* read_pixel24_be(const struct pixel_format *pf,
	const void *ptr, uint8_t *r, uint8_t *g, uint8_t *b, uint8_t *a)
{
	uint8_t *src=(uint8_t*)ptr;
	uint32_t pixel=
		((uint32_t)src[2]<<8)|
		((uint32_t)src[1]<<16)|
		((uint32_t)src[0]<<24);
	*r=(pixel&pf->red_mask)>>pf->rfo;
	*g=(pixel&pf->green_mask)>>pf->gfo;
	*b=(pixel&pf->blue_mask)>>pf->bfo;
	*a=0;
	return src+3;
}

static void* read_pixel32(const struct pixel_format *pf,
	const void *ptr, uint8_t *r, uint8_t *g, uint8_t *b, uint8_t *a)
{
	uint32_t *src=(uint32_t*)ptr;
	uint32_t pixel=src[0];
	*r=(pixel&pf->red_mask)>>pf->rfo;
	*g=(pixel&pf->green_mask)>>pf->gfo;
	*b=(pixel&pf->blue_mask)>>pf->bfo;
	*a=(pixel&pf->alpha_mask)>>pf->afo;

	if(pf->rfw>8) *r>>=(pf->rfw-8);
	if(pf->gfw>8) *g>>=(pf->gfw-8);
	if(pf->bfw>8) *b>>=(pf->bfw-8);
	if(pf->afw>8) *a>>=(pf->afw-8);

	return src+1;
}

static void* read_pixel32_l(const struct pixel_format *pf,
	const void *ptr, uint8_t *r, uint8_t *g, uint8_t *b, uint8_t *a)
{
	uint32_t *src=(uint32_t*)ptr;
	uint32_t pixel=src[0];
	*r=(pixel&pf->red_mask)>>pf->rfo;
	*g=(pixel&pf->green_mask)>>pf->gfo;
	*b=(pixel&pf->blue_mask)>>pf->bfo;
	*a=(pixel&pf->alpha_mask)>>pf->afo;

	if(pf->rfw>8) *r>>=(pf->rfw-8);
	if(pf->gfw>8) *g>>=(pf->gfw-8);
	if(pf->bfw>8) *b>>=(pf->bfw-8);
	if(pf->afw>8) *a>>=(pf->afw-8);

	return src+1;
}

static void* read_pixel8(const struct pixel_format *pf,
	const void *ptr, uint8_t *r, uint8_t *g, uint8_t *b, uint8_t *a)
{
	uint8_t *src=(uint8_t*)ptr;
	*r=src[0]; *g=src[0]; *b=src[0];
	return src+1;
}

static void* write_pf_unsup(const struct pixel_format *pf,
	void *ptr, uint32_t r, uint32_t g, uint32_t b, uint32_t a)
{
	dbg_trap("unsupported pixel format for \'write_pixel\': "
		"%d bpp, R %X, G %X, B %X, A %X\n",
		pf->depth,pf->red_mask,pf->green_mask,pf->blue_mask,pf->alpha_mask);
	return ptr;
}

