/*
 * Copyright (C) 2024 alx@fastestcode.org
 * This software is distributed under the terms of the MIT license.
 * See the included LICENSE file for further information.
 */

/* Implements Netpbm P(4-7) loader, which is also used for filters */

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include "imgfile.h"
#include "bswap.h"
#include "exec.h"
#include "debug.h"

enum pam_tupltype {
	PAM_UNDEF,
	PAM_BW,
	PAM_GRS,
	PAM_RGB,
	PAM_BWA,
	PAM_GRSA,
	PAM_RGBA
};

struct pam_info {
	unsigned short type;
	unsigned int width;
	unsigned int height;
	unsigned short depth;
	unsigned int maxval;
	enum pam_tupltype tupltype;

	FILE *file;	
};

#define PAM_HDR_BUFSIZ 40
#define PAM_MAXVAL_MAX 65535

#define PAM_RED_MASK	0x000000FF
#define PAM_GREEN_MASK	0x0000FF00
#define PAM_BLUE_MASK	0x00FF0000
#define PAM_ALPHA_MASK	0xFF000000

#define PAM_GRSV_MASK	0x000000FF
#define PAM_GRSA_MASK	0x0000FF00

static int init_stream(FILE*, struct img_file *img, struct pam_info*);
static int read_header(FILE*, struct pam_info*);
static int read_pnm_header(FILE*, int type, struct pam_info*);
static int read_pam_header(FILE*, struct pam_info*);
static int read_scanlines(struct img_file*, img_scanline_cbt, void*);
static int read_bw_scanlines(struct img_file*, img_scanline_cbt, void*);


static int read_scanlines(struct img_file *img,
	img_scanline_cbt cb, void *cdata)
{
	struct pam_info *inf = (struct pam_info*)img->loader_data;
	unsigned int nrow, ncol;
	size_t row_size = inf->width * inf->depth;
	size_t read;
	uint8_t *rb;
	int res = 0;
	int big_endian = is_big_endian();

	rb = malloc(inf->width * inf->depth);
	if(!rb) return IMG_ENOMEM;
	
	if(inf->maxval > 255) {
		uint16_t tupl[4];
		int s;
		unsigned int div = (0.5 + ((float)inf->maxval / 256));
		
		for(nrow = 0; nrow < inf->height; nrow++) {
			for(ncol = 0; ncol < inf->width; ncol++) {
				read = fread(tupl, 2, inf->depth, inf->file);
				if(read < inf->depth) {
					res = IMG_EDATA;
					break;
				}

				for(s = 0; s < inf->depth; s++)
					rb[ncol * inf->depth + s] = (big_endian) ?
						(tupl[s] / div) : (bswap_word(tupl[s]) / div);
			}
			if((*cb)(nrow, rb, cdata) == IMG_READ_CANCEL) break;
		}
	} else if(inf->maxval < 255) {
		uint8_t tupl[4];
		int s;
		float fac = 255.0 / inf->maxval;
		
		for(nrow = 0; nrow < inf->height; nrow++) {
			for(ncol = 0; ncol < inf->width; ncol++) {
				read = fread(tupl, 1, inf->depth, inf->file);
				if(read < inf->depth) {
					res = IMG_EDATA;
					break;
				}
				for(s = 0; s < inf->depth; s++)
					rb[ncol * inf->depth + s] = ((float)tupl[s] * fac);
			}
			if((*cb)(nrow, rb, cdata) == IMG_READ_CANCEL) break;
		}

	} else {

		for(nrow = 0; nrow < inf->height; nrow++) {
			read = fread(rb, 1, row_size, inf->file);
			if(read < inf->depth) {
				res = IMG_EDATA;
				break;
			}
			if((*cb)(nrow, rb, cdata) == IMG_READ_CANCEL) break;
		}
	}
	
	free(rb);
	return res;
}

static int read_bw_scanlines(struct img_file *img,
	img_scanline_cbt cb, void *cdata)
{

	struct pam_info *inf = (struct pam_info*)img->loader_data;
	unsigned int nrow, ncol;
	int byte;
	int ibit;
	uint8_t *rb;
	int res = 0;

	rb = malloc(inf->width + 8);
	if(!rb) return IMG_ENOMEM;

	for(nrow = 0; nrow < inf->height; nrow++) {
		for(ncol = 0; ncol < inf->width; ncol += 8) {
			byte = fgetc(inf->file);
			if(byte == EOF) {
				res = IMG_EDATA;
				break;
			}

			for(ibit = 0; ibit < 8; ibit++){
				rb[ncol + ibit] = ((byte & 0x80) ? 0 : 255);
				byte <<= 1;
			}
		}
		if((*cb)(nrow, rb, cdata) == IMG_READ_CANCEL) break;
	}
	
	free(rb);
	return res;
}

static int read_header(FILE *fin, struct pam_info *inf)
{
	int res = 0;
	char magic[3];
	
	if(!fgets(magic, 3, fin)) return IMG_EFILE;

	if(!memcmp(magic, "P4", 2)) {
		inf->type = 4;
		res = read_pnm_header(fin, 4, inf);
	} else if(!memcmp(magic, "P5", 2)) {
		inf->type = 5;
		res = read_pnm_header(fin, 5, inf);
	} else if(!memcmp(magic, "P6", 2)) {
		inf->type = 6;
		res = read_pnm_header(fin, 6, inf);
	} else if(!memcmp(magic, "P7", 2)) {
		inf->type = 7;
		res = read_pam_header(fin, inf);
	} else res = IMG_EUNSUP;

	dbg_trace("(%d): %dx%d depth=%d(max %d), tupl=%d\n", inf->type,
		inf->width, inf->height, inf->depth, inf->maxval, inf->tupltype);

	return res;
}

static int read_pam_header(FILE *fin, struct pam_info *inf)
{
	char rb[PAM_HDR_BUFSIZ + 1];
	
	while(fgets(rb, PAM_HDR_BUFSIZ, fin)) {
		char *p = rb;

		while(isspace(*p)) p++;

		if(!strncmp("ENDHDR", p, 6)) {
			break;
		} else if(!strncmp("WIDTH", p, 5)) {
			if(!sscanf(rb, "WIDTH %u", &inf->width))
				return IMG_EFILE;
		} else if(!strncmp("HEIGHT", p, 6)) {
			if(!sscanf(rb, "HEIGHT %u", &inf->height))
				return IMG_EFILE;
		} else if(!strncmp("DEPTH", p, 5)) {
			if(!sscanf(rb, "DEPTH %hu", &inf->depth))
				return IMG_EFILE;
		} else if(!strncmp("MAXVAL", p, 6)) {
			if(!sscanf(rb, "MAXVAL %u", &inf->maxval))
				return IMG_EFILE;
		} else if(!strncmp("TUPLTYPE", p, 8)) {

			p += 8;
			while(isspace(*p)) p++;

			if(!strncmp("BLACKANDWHITE_ALPHA", p, 19))
				inf->tupltype = PAM_BWA;
			else if(!strncmp("BLACKANDWHITE", p, 13))
				inf->tupltype = PAM_BW;
			else if(!strncmp("GRAYSCALE_ALPHA", p, 15))
				inf->tupltype = PAM_GRSA;
			else if(!strncmp("GRAYSCALE", p, 9))
				inf->tupltype = PAM_GRS;
			else if(!strncmp("RGB_ALPHA", p, 9))
				inf->tupltype = PAM_RGBA;
			else if(!strncmp("RGB", p, 3))
				inf->tupltype = PAM_RGB;
		}
	}

	if(!inf->width || !inf->height ||
				!inf->depth || !inf->maxval) return IMG_EDATA;
	
	if(inf->maxval > PAM_MAXVAL_MAX) return IMG_EUNSUP;
	
	return 0;
}

static int read_pnm_header(FILE *fin, int type, struct pam_info *inf)
{
	int chr, n, i = 0;
	char rdbuf[PAM_HDR_BUFSIZ + 1];
	
	while((chr = fgetc(fin)) != EOF) {
		n = 0;
		
		if(isspace(chr)) {
			continue;
		} else if(chr == '#') {
			while( (chr = fgetc(fin)) != EOF && chr != '\n');
			if(!n) continue;
		} else {
			while(isalnum(chr)) {
				rdbuf[n++] = (char) chr;
				chr = fgetc(fin);
				if((n == PAM_HDR_BUFSIZ) || (chr == EOF))
					return IMG_EDATA;
			}
		}
		rdbuf[n] = '\0';

		switch(i) {
			case 0:
			inf->width = atoi(rdbuf);
			break;
			case 1:
			inf->height = atoi(rdbuf);
			break;
			case 2:
			inf->maxval = atoi(rdbuf);
			break;
		}
		i++;
		if( ((type == 4) && (i == 2)) ||
			( (type > 4) && (i == 3) ) ) break;
	}
	/* last char read must have been a whitespace */
	if(!isspace(chr) || !inf->width || !inf->height ||
		(type > 4 && !inf->maxval) ) return IMG_EDATA;
	
	if(inf->maxval > PAM_MAXVAL_MAX) return IMG_EUNSUP;
	
	inf->depth = (type > 5) ? 3 : 1;

	return 0;
}

static void close(struct img_file *img)
{
	struct pam_info *inf = (struct pam_info*)img->loader_data;

	if(inf->file) fclose(inf->file);
	free(img->loader_data);
}

int img_open_pam(const char *file_name, struct img_file *img, int flags)
{
	struct stat st;
	struct pam_info *inf;
	FILE *file;
	int res;
	
	memset(img, 0, sizeof(struct img_file));
	
	if(stat(file_name, &st)== -1) return IMG_EIO;
	img->cr_time = st.st_mtime;
	 
	file = fopen(file_name, "r");
	if(!file) return (errno == EPERM) ? IMG_EPERM : IMG_EIO;
	
	inf = calloc(1, sizeof(struct img_file));
	if(!inf) {
		fclose(file);
		return IMG_ENOMEM;
	}

	res = init_stream(file, img, inf);

	img->loader_data = inf;
	img->close_fnc = close;
	inf->file = file;
	
	if(res) {
		fclose(file);
		free(inf);
	}

	return res;
}

static int init_stream(FILE *file, struct img_file *img, struct pam_info *inf)
{
	int res;
	
	res = read_header(file, inf);
	if(res) return res;

	if(inf->type == 4) {
		img->read_scanlines_fnc = read_bw_scanlines;
		img->bpp = 8;
		img->orig_bpp = 1;
	} else {
		img->read_scanlines_fnc = read_scanlines;
		img->bpp = inf->depth * 8;
		img->orig_bpp = (inf->maxval > 255) ? 
			(inf->depth * 16) : (inf->depth * 8);
	}
	img->bg_pixel = 0x00FFFFFF;
	img->width = inf->width;
	img->height = inf->height;
	
	if(inf->depth > 2) {
		img->red_mask = PAM_RED_MASK;
		img->green_mask = PAM_GREEN_MASK;
		img->blue_mask = PAM_BLUE_MASK;
		img->alpha_mask = (inf->tupltype > PAM_RGB) ? PAM_ALPHA_MASK : 0;
	} else {
		img->red_mask = PAM_GRSV_MASK;
		img->green_mask = PAM_GRSV_MASK;
		img->blue_mask = PAM_GRSV_MASK;
		img->alpha_mask = (inf->tupltype == PAM_GRSA) ? PAM_GRSA_MASK : 0;
	}

	switch(inf->type) {
	 	case 4:	img->type_str = "Netpbm Bitmap"; break;
		case 5: img->type_str = "Netpbm Graymap"; break;
		case 6: img->type_str = "Netpbm Pixmap"; break;
		case 7: img->type_str = "Netpbm Arbitrary"; break;
	}
	
	return 0;
}

int img_filter_pnm(const char *cmd_spec, const char *file_name,
	struct img_file *img, int flags)
{
	struct stat st;
	struct pam_info *inf;
	FILE *file;
	int res;
	struct exec_subst_rec subst[] = {
		{ 'n', (char*)file_name },
		{ 0, NULL }
	};
	
	
	memset(img, 0, sizeof(struct img_file));

	if(stat(file_name, &st)== -1) return IMG_EIO;
	img->cr_time = st.st_mtime;
	
	res = exec_file(cmd_spec, subst, &file);
	if(res) return IMG_EFILTER;
	
	inf = calloc(1, sizeof(struct img_file));
	if(!inf) {
		fclose(file);
		return IMG_ENOMEM;
	}
	inf->file = file;
	img->loader_data = inf;
	img->close_fnc = close;

	res = init_stream(file, img, inf);
	if(res) {
		fclose(file);
		free(inf);
	}

	return res;
}
