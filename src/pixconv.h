/*
 * Copyright (C) 2012-2025 alx@fastestcode.org
 * This software is distributed under the terms of the MIT license.
 * See the included LICENSE file for further information.
 */
 
/*
 * Typedefs and prototypes for pixel format conversion routines.
 */

#ifndef PIXCONV_H
#define PIXCONV_H

#include <inttypes.h>
#include <X11/Xlib.h>

/* Pixel format descriptor for direct/true color images */
struct pixel_format {
	short flags;
	/* nibble offsets and widths */
	union {
		uint8_t e[8];
		struct {
			uint8_t rfo;
			uint8_t rfw;
			uint8_t gfo;
			uint8_t gfw;
			uint8_t bfo;
			uint8_t bfw;
			uint8_t afo;
			uint8_t afw;
		};
	};

	uint32_t red_mask;
	uint32_t green_mask;
	uint32_t blue_mask;
	uint32_t alpha_mask;
	uint8_t bg_color[3];	/* RGB */
	
	uint8_t pixel_size;
	uint8_t depth; /* bits per pixel */
	
	/* pixel read/write function pointers */
	void* (*write_pixel_fnc)(const struct pixel_format*,void*,
		uint32_t,uint32_t,uint32_t,uint32_t);
	void* (*read_pixel_fnc)(const struct pixel_format*,const void*,
		uint8_t*,uint8_t*,uint8_t*,uint8_t*);
};

/*
 * Initialize the pixel_format structure.
 * 'flags' must be a combination of IMGF_ flags or zero.
 */
int init_pixel_format(struct pixel_format *pf, uint8_t depth,
	uint32_t red_mask, uint32_t green_mask, uint32_t blue_mask,
	uint32_t alpha_mask, uint32_t bg_pixel, short flags);
		
/*
 * Convert RGB pixels.
 * 'src/dest' 'pf_src/dest' must point to the source/destination buffers and
 * according pixel format structures. 'npix' must specify number of pixels
 * to be converted. 'bg_color' may include the background color (only used
 * if 'src' contains alpha channel).
 */
void convert_rgb_pixels(void *dest, const struct pixel_format *pf_dest,
	const void *src, const struct pixel_format *pf_src, unsigned int npix);

/*
 * Convert RGB pixels to CLUT pixels according to the given colormap.
 */
void rgb_pixels_to_clut(void *dest, const void *src,
	const struct pixel_format *pf_src, unsigned int npix);

/*
 * Remap CLUT pixels according to the given colormap.
 */
void remap_pixels(void *dest, const void *src,
	const uint8_t *clut, unsigned int npix);

/*
 * Convert CLUT pixels to RGB pixels.
 * 'src' and 'clut' must point to source image data and its RGB lookup table.
 * 'dest' and 'pf_dest' must point to destination storage and desired pixel
 * format. 'npix' must specify the number of pixels to convert.
 */
void clut_to_rgb_pixels(void *dest, const struct pixel_format *pf_dest,
	const uint8_t *src, const uint8_t *clut, unsigned int npix);

#endif /* PIXCONV_H */
