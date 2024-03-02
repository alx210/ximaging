/*
 * Copyright (C) 2012-2024 alx@fastestcode.org
 * This software is distributed under the terms of the MIT license.
 * See the included LICENSE file for further information.
 */
 
/*
 * Prototypes for XImage blitting routines.
 */

#ifndef IMGBLT_H
#define IMGBLT_H

#include <X11/Xlib.h>

/*
 * Transform and blit a rectangle of pixels from 'src' to 'dest' 
 */
void img_blt(XImage *src, unsigned int sxc, unsigned int syc,
	unsigned int sw, unsigned int sh, XImage *dest,
	float scale, short transfm, short flags);

/*
 * Fill a rectangular area in 'img' with a pixel value.
 */
void img_fill_rect(XImage *img, unsigned int x, unsigned int y,
	unsigned int width, unsigned int height, unsigned long pixel);

/* Blitter flags */
#define BLTF_INT_UP 0x0001 /* Interpolate when scaling up */
#define BLTF_INT_DOWN 0x0002 /* Interpolate when decimating */
#define BLTF_INTERPOLATE (BLTF_INT_UP|BLTF_INT_DOWN)

#endif /* IMGBLT_H */
