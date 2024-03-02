/*
 * Copyright (C) 2012-2024 alx@fastestcode.org
 * This software is distributed under the terms of the MIT license.
 * See the included LICENSE file for further information.
 */
 
/*
 * Prototypes for color mapping routines.
 */

#ifndef CMAP_H
#define CMAP_H

#include <X11/Xlib.h>

/* Convert HSV to RGB values */
void hsv_to_rgb(float h, float s, float v, float *r, float *g, float *b);

/* Generate a uniform color palette */
int generate_palette(XColor *buf, unsigned int count, unsigned short irange);

/* 
 * Allocate at least 'n' uniform X colormap entries.
 * Returns the actual number of entries allocated.
 * If 'private' is True, 'cm' must be a read/write colormap.
 */
int cm_alloc_spectrum(Colormap cm, int n, Bool private);

/* 
 * Returns color map index that closely matches the specified RGB
 * cm_alloc_spectrum must have been called prior to this.
 */
int cm_match_rgb(int r,int g,int b);

#endif /* CMAP_H */
