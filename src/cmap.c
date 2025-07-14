/*
 * Copyright (C) 2012-2025 alx@fastestcode.org
 * This software is distributed under the terms of the MIT license.
 * See the included LICENSE file for further information.
 */

#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <math.h>
#include <errno.h>
#include <X11/Xlib.h>
#include "common.h"
#include "const.h"
#include "debug.h"

/* Local prototypes */
static int try_alloc_shared_entries(Colormap,XColor*,int);
static int match_rgb(XColor *colors, int ncolors, int r,int g,int b);

/* intensity range for color generation */
#define XINT_RANGE	65535
/* number of colormap entries */
#define XCLUT_SIZE	256

 /* cached X colormap */
static XColor g_colors[XCLUT_SIZE];
static unsigned int g_ncolors=0;
 /* pixels allocated in the shared X colormap */
static unsigned long g_pixels[XCLUT_SIZE];

/*
 * Convert HSV to RGB values
 */
void hsv_to_rgb(float h, float s, float v, float *r, float *g, float *b)
{
	float x,y,z,h1,c2;
	int c1;

	h1=fmodf(h*6,6);
	c1=floorf(h1);
	c2=h1-c1;
	
	x=(1.0-s)*v;
	y=(1.0-(s*c2))*v;
	z=(1.0-(s*(1-c2)))*v;
	
	switch(c1){
		case 0: *r=v; *g=z; *b=x; break;
		case 1: *r=y; *g=v; *b=x; break;
		case 2: *r=x; *g=v; *b=z; break;
		case 3: *r=x; *g=y; *b=v; break;
		case 4: *r=z; *g=x; *b=v; break;
		case 5: *r=v; *g=x; *b=y; break;
	}
}

/*
 * Generate a uniform color palette .
 */
int generate_palette(XColor *buf, unsigned int count, unsigned short irange)
{
	int ih,i;
	float h,s,v,inc,inc2;
	float r, g, b;
	const int max_h=sqrt(count);	/* hue levels */
	const int max_v=max_h;	/* saturation levels */
	int j=0;
	
	/* max_h*max_v spectrum */
	inc=1.0/max_h;
	inc2=(1.0/(max_v/2));
	for(ih=0, h=0; ih<max_h; ih++, h+=inc){
		for(i=0, v=inc2; i<(max_v/2)-1; i++, v+=inc2){
			hsv_to_rgb(h,1.0,v,&r,&g,&b);
			buf[j].red=roundf(r*irange);
			buf[j].green=roundf(g*irange);
			buf[j].blue=roundf(b*irange);
			j++;
		}
		for(s=1.0; i<max_v-1; i++, s-=inc2){
			hsv_to_rgb(h,s,v,&r,&g,&b);
			buf[j].red=roundf(r*irange);
			buf[j].green=roundf(g*irange);
			buf[j].blue=roundf(b*irange);
			j++;
		}
	}
	/* b/w and shades of gray */
	inc=(1.0/(max_v-1));
	for(i=0, v=0; i<max_v; v+=inc, i++){
		buf[j].red=buf[j].green=buf[j].blue=v*irange; j++;
	}
	return j;
}

/* 
 * Returns color map index that closely matches the specified RGB
 * cm_alloc_spectrum must have been called prior to this.
 */
int cm_match_rgb(int r,int g,int b)
{
	dbg_assertmsg(g_ncolors,"no colormap allocated");
	return match_rgb(g_colors,g_ncolors,r,g,b);
}

/*
 * Try and allocate shared color cells.
 * Return number of cells allocated on success.
 */
static int try_alloc_shared_entries(Colormap cm,
	XColor *colors, int num_colors)
{
	int i, j, failed=0;

	if(g_ncolors){
		XFreeColors(app_inst.display,cm,g_pixels,g_ncolors,0);
		g_ncolors=0;
	}

	for(i=0, j=0; i<num_colors; i++){
		g_colors[i].flags=DoRed|DoGreen|DoBlue;
		if(!XAllocColor(app_inst.display,cm,&g_colors[i])){
			failed++;;
		}else{
			g_colors[j]=g_colors[i];
			g_pixels[j]=g_colors[i].pixel;
			j++;
		}
	}
	g_ncolors=j;
	return j;
}

/* 
 * Allocate at least 'n' uniform X colormap entries.
 * Returns the actual number of entries allocated.
 * If 'private' is True, 'cm' must be a read/write colormap.
 */
int cm_alloc_spectrum(Colormap cm, int entries, Bool private)
{
	int res;
	
	if(private){
		int i;
		generate_palette(g_colors,entries,XINT_RANGE);
		for(i=0; i<XCLUT_SIZE; i++){
			g_colors[i].pixel=i;
			g_colors[i].flags=DoRed|DoGreen|DoBlue;
		}
		XStoreColors(app_inst.display,cm,g_colors,XCLUT_SIZE);
		g_ncolors=entries;
		res=entries;
	}else{
		int ntry=entries;
		do {
			generate_palette(g_colors,ntry,XINT_RANGE);
			res=try_alloc_shared_entries(cm,g_colors,ntry);
			if(res<ntry){
				if(res<MIN_SHARED_CMAP){
					if(g_ncolors){
						XFreeColors(app_inst.display,cm,g_pixels,g_ncolors,0);
						g_ncolors=0;
					}
					return 0;
				}
				ntry=res;
			}
		}while(res!=ntry);
	}
	return res;
}

/* 
 * Returns a CLUT index that closely matches the specified RGB.
 */
static int match_rgb(XColor *colors, int ncolors, int r,int g,int b)
{
	unsigned int i, j, k=0;
	int x,y,z;
	unsigned int min=(~0);
	
	for(i=0; i<ncolors; i++){
		x=r-(colors[i].red>>8);
		y=g-(colors[i].green>>8);
		z=b-(colors[i].blue>>8);
		j=x*x+y*y+z*z;
		if(j<min){
			k=i;
			if(j==0) break;
			min=j;
		}
	}
	return colors[k].pixel;
}
