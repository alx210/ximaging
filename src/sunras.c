/*
 * Copyright (C) 2012-2024 alx@fastestcode.org
 * This software is distributed under the terms of the MIT license.
 * See the included LICENSE file for further information.
 */
 
/*
 * Sun raster image file reader.
 */

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <memory.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "imgfile.h"
#include "bswap.h"
#include "debug.h"

struct sun_hdr
{
	uint32_t id;		/* Must be RAS_ID */
	uint32_t width;		/* Image width in pixels */
	uint32_t height;	/* Image height in pixels */
	uint32_t depth;		/* Bits per pixel */
	uint32_t data_len;	/* Image data length */
	uint32_t type;		/* Image type */
	uint32_t cmap_type;	/* Color map type */
	uint32_t cmap_len;	/* Color map length in bytes */
};

/* sun_hdr.id */
#define RAS_ID	0x59a66a95

/* sun_hdr.type constants */
#define RAS_LEGACY		0
#define RAS_STANDARD	1
#define RAS_ENCODED	2	/* Image data is RLE encoded */
#define RAS_RGB		3	/* Image data is RGB rather than BGR */

/* sun_hdr.clr_map_type constants */
#define CMAP_NONE	0
#define CMAP_RGB	1
#define CMAP_RAW	2

/* Loader data */
struct ras_ld
{
	FILE *file;
	struct sun_hdr hdr;
	uint8_t *cmap;
	unsigned int pad;
	size_t scl_size;
	uint8_t *buffer;
	unsigned int data_offset;
};

/* RLE decoder state */
struct dec_state {
	int data;
	int run;
};

/* Local prototypes */
static int read_bw_raw(struct img_file*,img_scanline_cbt,void*);
static int read_bw_rle(struct img_file*,img_scanline_cbt,void*);
static int read_raw(struct img_file*,img_scanline_cbt,void*);
static int read_rle(struct img_file*,img_scanline_cbt,void*);
static int read_cmap(struct img_file*,void*);
static void bits_to_bytes(uint8_t b, uint8_t *buf,
	uint8_t black, uint8_t white);
static void close_image(struct img_file*);
static int rle_get_byte(FILE*, struct dec_state*, uint8_t*);

/*
 * Read raw bitmap scanlines.
 */
static int read_bw_raw(struct img_file *img, img_scanline_cbt cb, void *cdata)
{
	struct ras_ld *ld=(struct ras_ld*)img->loader_data;
	unsigned long i,j;
	uint8_t black=0;
	uint8_t white=(ld->hdr.cmap_type==CMAP_NONE)?0xFF:1;
			
	fseek(ld->file,ld->data_offset,SEEK_SET);
	
	for(i=0, j=0; i<ld->hdr.height; i++){
		for(j=0; j<ld->hdr.width; j+=8){
			int c;
			c=fgetc(ld->file);
			if(c==EOF) break;
			bits_to_bytes(c,&ld->buffer[j],black,white);
		}
		if((*cb)(i,ld->buffer,cdata)==IMG_READ_CANCEL) return 0;
	}
	
	if(ferror(ld->file) || feof(ld->file))
		return IMG_EFILE;

	return 0;
}

/*
 * Read RLE bitmap scanlines.
 */
static int read_bw_rle(struct img_file *img, img_scanline_cbt cb, void *cdata)
{
	struct ras_ld *ld=(struct ras_ld*)img->loader_data;
	struct dec_state ds;
	unsigned long i,j;
	int res;
	uint8_t black=0;
	uint8_t white=(ld->hdr.cmap_type==CMAP_NONE)?0xFF:1;
	
	memset(&ds,0,sizeof(struct dec_state));
	fseek(ld->file,ld->data_offset,SEEK_SET);
	
	for(i=0, j=0; i<ld->hdr.height; i++){
		for(j=0; j<ld->hdr.width; j+=8){
			uint8_t byte;
			res=rle_get_byte(ld->file,&ds,&byte);
			if(res) break;
			bits_to_bytes(byte,&ld->buffer[j],black,white);
		}
		if(res) break;
		if((*cb)(i,ld->buffer,cdata)==IMG_READ_CANCEL) return 0;
	}

	return res;
}

/*
 * Read raw RGB(X)/greyscale/indexed scanlines.
 */
static int read_raw(struct img_file *img, img_scanline_cbt cb, void *cdata)
{
	struct ras_ld *ld=(struct ras_ld*)img->loader_data;
	size_t read;
	unsigned long i;
	int res=0;
	
	fseek(ld->file,ld->data_offset,SEEK_SET);
	
	for(i=0; i<ld->hdr.height; i++){
		read=fread(ld->buffer,1,ld->scl_size,ld->file);
		if(read<ld->scl_size){
			if(feof(ld->file))
				return IMG_EFILE;
			else if(ferror(ld->file))
				return IMG_EIO;
		}
		if((*cb)(i,ld->buffer,cdata)==IMG_READ_CANCEL) return 0;
	}

	return res;
}

/*
 * Read RLE RGB(X)/greyscale/indexed scanlines.
 */
static int read_rle(struct img_file *img, img_scanline_cbt cb, void *cdata)
{
	struct ras_ld *ld=(struct ras_ld*)img->loader_data;
	struct dec_state ds;
	int i, j;
	int res=0;
	
	fseek(ld->file,ld->data_offset,SEEK_SET);
	memset(&ds,0,sizeof(struct dec_state));
	
	for(i=0; i<ld->hdr.height; i++){
		for(j=0; j<ld->scl_size; j++){
			res=rle_get_byte(ld->file,&ds,&ld->buffer[j]);
			if(res) break;
		}
		if(res) break;
		if((*cb)(i,ld->buffer,cdata)==IMG_READ_CANCEL) return 0;
	}
	return res;
}

/*
 * Read the color palette.
 */
static int read_cmap(struct img_file *img, void *buf)
{
	struct ras_ld *ld=(struct ras_ld*)img->loader_data;
	size_t read;
	uint8_t planes[IMG_CLUT_SIZE]={0};
	uint8_t *rgb=(uint8_t*)buf;
	unsigned int n,i;

	fseek(ld->file,sizeof(struct sun_hdr),SEEK_SET);
	read=fread(planes,1,ld->hdr.cmap_len,ld->file);
	if(read<ld->hdr.cmap_len) return IMG_EIO;
	
	/* translate planes to RGB map */
	n=ld->hdr.cmap_len/3;
	for(i=0; i<n; i++){
		rgb[i*3]=planes[i];
		rgb[i*3+1]=planes[i+n];
		rgb[i*3+2]=planes[i+n*2];
	}
	return 0;
}

/*
 * Convert a BW byte to 8 bpp
 */
static void bits_to_bytes(uint8_t b, uint8_t *buf,
	uint8_t black, uint8_t white)
{
	int i;
	for(i=0; i<8; i++){
		buf[i]=((b&0x80)?black:white);
		b<<=1;
	}
}

/*
 * Retrieve next byte from the RLE stream.
 * The dec_state structure fields must be initialized to zero
 * for the first call.
 */
static int rle_get_byte(FILE *file, struct dec_state *st, uint8_t *b)
{
	int data, run=0;
	
	if(st->run){
		*b=(uint8_t)st->data;
		st->run--;
		return 0;
	}
	
	data=fgetc(file);
	if(data==EOF) return (ferror(file)?IMG_EIO:IMG_EFILE);
	
	if(data==0x80){
		run=fgetc(file);
		if(run==EOF)
			return (ferror(file)?IMG_EIO:IMG_EFILE);
		if(run==0){
			data=0x80;
		}else{
			data=fgetc(file);
			if(data==EOF)
				return (ferror(file)?IMG_EIO:IMG_EFILE);
		}
	}
	*b=(uint8_t)data;
	st->data=data;
	st->run=run;
	return 0;
}


static void close_image(struct img_file *img)
{
	struct ras_ld *ld=(struct ras_ld*)img->loader_data;
	
	free(ld->buffer);	
	fclose(ld->file);
	if(ld->cmap) free(ld->cmap);
	free(img->loader_data);
	memset(img,0,sizeof(struct img_file));
}

int img_open_ras(const char *file_name, struct img_file *img, int flags)
{
	struct stat st;
	FILE *file;
	struct sun_hdr hdr;
	struct ras_ld *ld;
	size_t read=0;

	memset(img,0,sizeof(struct img_file));
	if(stat(file_name,&st)== -1) return IMG_EIO;
	img->cr_time=st.st_mtime;

	file=fopen(file_name,"r");
	if(!file) return IMG_EFILE;
	
	read=fread(&hdr,1,sizeof(struct sun_hdr),file);
	if(!is_big_endian()){
	 	int i, n;
		uint32_t *ptr=(uint32_t*)&hdr;
		n=sizeof(struct sun_hdr)/sizeof(uint32_t);
		for(i=0; i<n; i++) ptr[i]=bswap_dword(ptr[i]);
	}
	if(read<sizeof(struct sun_hdr) || !hdr.width || !hdr.height){
		fclose(file);
		return IMG_EFILE;
	}

	if(hdr.id!=RAS_ID){
		fclose(file);
		return IMG_EFILE;
	}

	/* see if the file contains standard data */
	if(hdr.cmap_len>IMG_CLUT_SIZE || hdr.cmap_type==CMAP_RAW){
		fclose(file);
		return IMG_EUNSUP;
	}
	
	ld=calloc(1,sizeof(struct ras_ld));
	if(!ld){
		fclose(file);
		return IMG_ENOMEM;
	}
	memcpy(&ld->hdr,&hdr,sizeof(struct sun_hdr));
	ld->file=file;
	ld->pad=(hdr.width%2)?1:0;
	
	img->loader_data=(void*)ld;
	img->close_fnc=&close_image;
	img->width=hdr.width;
	img->height=hdr.height;
	img->orig_bpp=img->bpp=hdr.depth;
	
	switch(hdr.depth){
		case 1:
		img->read_scanlines_fnc=
			(hdr.type==RAS_ENCODED)?&read_bw_rle:&read_bw_raw;
		img->format=IMG_DIRECT;
		/* data is converted to 8bpp in read_bw_scanlines */
		img->orig_bpp=1;
		img->bpp=8;
		if(hdr.width<16) ld->pad=1;
		ld->scl_size=hdr.width+ld->pad;
		break;
		case 8:
		img->read_scanlines_fnc=
			(hdr.type==RAS_ENCODED)?(&read_rle):(&read_raw);
		if(hdr.cmap_type!=CMAP_NONE){
			img->read_cmap_fnc=&read_cmap;
			img->format=IMG_PSEUDO;
		}else{
			img->format=IMG_DIRECT;
		}
		ld->scl_size=hdr.width+ld->pad;
		break;
		case 24:{
			img->format=IMG_DIRECT;
			ld->scl_size=hdr.width*3+ld->pad;
			if(hdr.type==RAS_RGB){
				img->red_mask=0x000000FF;
				img->green_mask=0x0000FF00;
				img->blue_mask=0x00FF0000;
			}else{
				img->red_mask=0x00FF0000;
				img->green_mask=0x0000FF00;
				img->blue_mask=0x000000FF;
			}
			img->read_scanlines_fnc=
				(hdr.type==RAS_ENCODED)?(&read_rle):(&read_raw);
			img->format=IMG_DIRECT;
		}break;
		case 32:{
			img->format=IMG_DIRECT;
			ld->scl_size=hdr.width*4+ld->pad;
			/* NOTE: The Sun Raster spec doesn't explicitely define
			 *       contents of the extra channel in 32 bit images,
			 *       but apparently there are RAS writers, which use
			 *       it as an alpha channel. */
			#ifdef SUNRAS_ALPHA
			img->alpha_mask=SUNRAS_ALPHA;
			#endif
			if(hdr.type==RAS_RGB){
				img->red_mask=0x000000FF;
				img->green_mask=0x0000FF00;
				img->blue_mask=0x00FF0000;
			}else{
				img->red_mask=0x00FF0000;
				img->green_mask=0x0000FF00;
				img->blue_mask=0x000000FF;
			}
			img->read_scanlines_fnc=
				(hdr.type==RAS_ENCODED)?(&read_rle):(&read_raw);
			img->format=IMG_DIRECT;
		}break;
		default:
		fclose(file);
		free(ld);
		return IMG_EUNSUP;
		break;
	}
	if(is_big_endian()){
		img->red_mask=bswap_dword(img->red_mask);
		img->green_mask=bswap_dword(img->green_mask);
		img->blue_mask=bswap_dword(img->blue_mask);
		img->alpha_mask=bswap_dword(img->alpha_mask);
	}
	dbg_assert(ld->scl_size);
	ld->buffer=malloc(ld->scl_size);
	if(!ld->buffer){
		free(ld);
		fclose(file);
		return IMG_ENOMEM;
	}
	ld->data_offset=sizeof(struct sun_hdr);
	if(ld->hdr.cmap_type!=CMAP_NONE)
		ld->data_offset+=hdr.cmap_len;
	return 0;
}

