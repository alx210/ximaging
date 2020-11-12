/*
 * Copyright (C) 2012-2017 alx@fastestcode.org
 * This software is distributed under the terms of the MIT license.
 * See the included LICENSE file for further information.
 */
 
/*
 * PC-Paintbrush V5 reader.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "imgfile.h"
#include "bswap.h"
#include "debug.h"

struct pcx_header
{
	uint8_t mf;		/* manufacturer ID, must be 0x0A */
	uint8_t version;
	uint8_t encoding;
	uint8_t bpp;		/* bits per pixel in a plane */
	uint16_t xmin;	/* max-min+1 is used to calculate image dimensions */
	uint16_t ymin;
	uint16_t xmax;
	uint16_t ymax;
	
	/*
	uint16_t hdpi;
	uint16_t vdpi;
	uint8_t clrmap[48];
	uint8_t reserved;
	*/
	
	uint8_t planes;	/* actual bits-per-pixel is calculated as planes*bpp */
	uint16_t bpl;		/* uncompressed scanline size in bytes */
	uint16_t palinfo;
	
	/*
	uint16_t hscreen;
	uint16_t vscreen;
	uint8_t filler[54];
	*/
};
#define DATA_OFFSET	128 /* header size in PCX file*/

struct pcx_ld {
	FILE *file;
	size_t row_size;
	size_t padding;
};

#define PCX_RMASK	(0x000000FF)
#define PCX_GMASK	(0x0000FF00)
#define PCX_BMASK	(0x00FF0000)

/* Locals */
static int read_rgb_scanlines(struct img_file *img,
	img_scanline_cbt cb, void *cdata);
static int read_cm_scanlines(struct img_file *img,
	img_scanline_cbt cb, void *cdata);
static int decode_row(FILE*, unsigned char*, int);
static void bswap_dstruct(struct pcx_header*);
static void close_image(struct img_file *img);

/*
 * Decode an RLE row into the pointed buffer, which must be of bpl size.
 */
static int decode_row(FILE* file, unsigned char *buff, int bpl)
{
	unsigned char *pos;
	int cb=0;
	unsigned char run=1; /* assume run of 1 */
	int scan=0;

	pos=buff;
	for(;;)
	{
		cb=fgetc(file);
		if(cb==EOF) return IMG_EFILE;
		if(0xC0==(0xC0&cb))
		{
			run=0x3F&cb;
			cb=fgetc(file);
			if(cb==EOF) return IMG_EFILE;
		}else{
			run=1;
		}
		/* there must be an encoding break at the end of the scanline */
		if(scan+run>bpl) return IMG_EFILE;
		memset(pos,cb,run);
		scan+=run;
		pos+=run;
		if(scan==bpl) break;
	}
	return 0;
}

/* Read 24 bit RLE scanlines */
static int read_rgb_scanlines(struct img_file *img,
	img_scanline_cbt cb, void *cdata)
{
	struct pcx_ld *ld=(struct pcx_ld*)img->loader_data;
	unsigned char *row_buf;
	unsigned char *scl_buf;
	unsigned int iscl;
	int i;
	int res=0;
	
	if(fseek(ld->file,DATA_OFFSET,SEEK_SET)== -1)
		return IMG_EFILE;
	
	row_buf=malloc(ld->row_size);
	scl_buf=malloc(img->width*(img->bpp/8));
	if(!row_buf || !scl_buf){
		if(row_buf) free(row_buf);
		if(scl_buf) free(scl_buf);
		return IMG_ENOMEM;
	}

	for(iscl=0; iscl<img->height; iscl++){
		res=decode_row(ld->file,row_buf,ld->row_size);

		if(res) break;
		/* convert planes to RGB rows */
		for(i=0; i<img->width; i++){
			scl_buf[i*3]=row_buf[i];
			scl_buf[i*3+1]=row_buf[i+img->width+ld->padding];
			scl_buf[i*3+2]=row_buf[i+(ld->padding+img->width)*2];
		}
		if((*cb)(iscl,scl_buf,cdata)==IMG_READ_CANCEL) break;
	}
	free(row_buf);
	free(scl_buf);
	return res;
}

/* Read 8 bpp RLE scanlines */
static int read_cm_scanlines(struct img_file *img,
	img_scanline_cbt cb, void *cdata)
{
	struct pcx_ld *ld=(struct pcx_ld*)img->loader_data;	
	unsigned int iscl;
	unsigned char *row_buf;
	int res;

	if(fseek(ld->file,DATA_OFFSET,SEEK_SET)== -1)
		return IMG_EFILE;

	row_buf=malloc(ld->row_size);
	if(!row_buf) return IMG_ENOMEM;

	for(iscl=0; iscl<img->height; iscl++){
		res=decode_row(ld->file,row_buf,ld->row_size);
		if(res) break;
		if((*cb)(iscl,row_buf,cdata)==IMG_READ_CANCEL) break;
	}
	free(row_buf);
	return 0;
}

/* Read the RGB color map */
static int read_cmap(struct img_file *img, void *buffer)
{
	int magic;
	struct pcx_ld *ld=(struct pcx_ld*)img->loader_data;
	size_t read;
	
	/* 0x0C at 769b up the end of file means RGB color table is there */
	if(fseek(ld->file,-769,SEEK_END)== -1)
		return IMG_EINVAL;
	magic=fgetc(ld->file);
	if(magic==EOF || ((unsigned char)magic)!=0x0C)
		return IMG_EINVAL;
	read=fread(buffer,1,IMG_CLUT_SIZE,ld->file);

	if(read<IMG_CLUT_SIZE) return IMG_EFILE;
	
	return 0;
}

int img_open_pcx(char *file_name, struct img_file *img, int flags)
{
	FILE *file;
	struct stat st;
	struct pcx_header hdr;
	struct pcx_ld *ld;
	
	memset(img,0,sizeof(struct img_file));	
	if(stat(file_name,&st)== -1) return IMG_EIO;
	img->cr_time=st.st_mtime;

	file=fopen(file_name,"r");
	if(!file) return IMG_EIO;
	
	hdr.mf=fgetc(file);
	hdr.version=fgetc(file);
	hdr.encoding=fgetc(file);
	hdr.bpp=fgetc(file);
	fread(&hdr.xmin,sizeof(uint16_t),1,file);
	fread(&hdr.ymin,sizeof(uint16_t),1,file);
	fread(&hdr.xmax,sizeof(uint16_t),1,file);
	fread(&hdr.ymax,sizeof(uint16_t),1,file);
	fseek(file,53,SEEK_CUR);
	hdr.planes=fgetc(file);
	fread(&hdr.bpl,sizeof(uint16_t),1,file);
	fread(&hdr.palinfo,sizeof(uint16_t),1,file);
	if(ferror(file) || feof(file)){
		fclose(file);
		return IMG_EFILE;
	}

	if(is_big_endian()) bswap_dstruct(&hdr);
	if(hdr.mf!=10 || hdr.version!=5 || hdr.bpp<8){
		fclose(file);
		return IMG_EUNSUP;
	}

	ld=malloc(sizeof(struct pcx_ld));
	if(!ld){
		fclose(file);
		return IMG_ENOMEM;
	}

	img->width=hdr.xmax-hdr.xmin+1;
	img->height=hdr.ymax-hdr.ymin+1;
	img->bpp=(int)hdr.bpp*hdr.planes;
	img->orig_bpp=img->bpp;
	
	img->red_mask=PCX_RMASK;
	img->green_mask=PCX_GMASK;
	img->blue_mask=PCX_BMASK;
	img->loader_data=(void*)ld;
	img->close_fnc=&close_image;
	
	if(is_big_endian()){
		img->red_mask=bswap_dword(img->red_mask);
		img->green_mask=bswap_dword(img->green_mask);
		img->blue_mask=bswap_dword(img->blue_mask);
	}

	if(hdr.bpp*hdr.planes==8){
		img->format=IMG_PSEUDO;
		img->read_cmap_fnc=&read_cmap;
		img->read_scanlines_fnc=&read_cm_scanlines;
	}else{
		img->format=IMG_DIRECT;
		img->read_scanlines_fnc=&read_rgb_scanlines;
	}
	
	ld->row_size=hdr.bpl*hdr.planes;
	ld->padding=hdr.bpl-img->width;
	ld->file=file;
	
	return 0;
}

static void close_image(struct img_file *img)
{
	struct pcx_ld *ld=(struct pcx_ld*)img->loader_data;
	fclose(ld->file);
	free(ld);
}

static void bswap_dstruct(struct pcx_header *hdr)
{
	hdr->xmin=bswap_word(hdr->xmin);
	hdr->ymin=bswap_word(hdr->ymin);
	hdr->xmax=bswap_word(hdr->xmax);
	hdr->ymax=bswap_word(hdr->ymax);
	hdr->bpl=bswap_word(hdr->bpl);
	hdr->palinfo=bswap_word(hdr->palinfo);
}
