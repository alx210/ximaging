/*
 * Copyright (C) 2012-2024 alx@fastestcode.org
 * This software is distributed under the terms of the MIT license.
 * See the included LICENSE file for further information.
 */
 
/*
 * X Bitmap image file reader.
 */

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <inttypes.h>
#include <memory.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "imgfile.h"
#include "debug.h"

static int read_chunk(FILE *file, unsigned short *bits);
static int parse_xbm_header(FILE *file, unsigned int *width,
	unsigned int *height, short *bpc);
static int find_data(FILE *file, fpos_t *pos);
static int read_scanlines(struct img_file*,img_scanline_cbt,void*);
static void close(struct img_file *img);

#define CTOSZ(v) #v
#define STRINGIFY(v) CTOSZ(v)
#define XBM_MAX_LINE 256

struct xbm_ld {
	FILE *img_file;
	FILE *mask_file;
	unsigned int width;
	unsigned int height;
	fpos_t img_data_offset;
	fpos_t mask_data_offset;
	unsigned short bpc;
};

/* Reads the next hexadecimal value and returns it in 'bits' */
static int read_chunk(FILE *file, unsigned short *bits)
{
	char buf[10];
	short i=0;
	int c=0;
	
	do{
		c=fgetc(file);
		if(c==EOF) return (ferror(file))?IMG_EIO:IMG_EFILE;
	}while(isspace(c) || c==',');

	while(isalnum(c)){
		buf[i]=c;
		i++;
		c=fgetc(file);
		if(c==EOF || i==7) return (ferror(file))?IMG_EIO:IMG_EFILE;
	}
	buf[i]='\0';
	*bits=(unsigned short)strtoul(buf,NULL,16);
	return 0;
}

static int read_scanlines(struct img_file *img,
	img_scanline_cbt cb, void *cdata)
{
	struct xbm_ld *ld=(struct xbm_ld*)img->loader_data;
	unsigned int iscl;
	unsigned int cx;
	unsigned short bits;
	short nbits=ld->bpc*8;
	unsigned char *buf;
	int i;
	int res=0;
	
	/* if a mask file is available, merge pixels to a 2 bpp
	 * (gray+alpha) pixmap */
	if(ld->mask_file){
		const unsigned int bpl=ld->width*2;
		unsigned short mask_bits;
		fsetpos(ld->img_file,&ld->img_data_offset);
		fsetpos(ld->mask_file,&ld->mask_data_offset);
		buf=malloc((ld->width+8)*2);
		if(!buf) return IMG_ENOMEM;

		for(iscl=0; iscl<ld->height; iscl++){
			for(cx=0; cx<bpl; ){
				res=read_chunk(ld->img_file,&bits);
				res|=read_chunk(ld->mask_file,&mask_bits);
				if(res){
					free(buf);
					return res;
				}
				for(i=0; i<nbits; i++, cx++){
					buf[cx]=(bits&0x01)?0:255;
					bits>>=1;
					cx++;
					buf[cx]=(mask_bits&0x01)?255:0;
					mask_bits>>=1;
				}
			}
			if((*cb)(iscl,buf,cdata)==IMG_READ_CANCEL) break;
		}
	}else{
		fsetpos(ld->img_file,&ld->img_data_offset);
		buf=malloc(ld->width+8);
		if(!buf) return IMG_ENOMEM;

		for(iscl=0; iscl<ld->height; iscl++){
			for(cx=0; cx<ld->width; ){
				res=read_chunk(ld->img_file,&bits);
				if(res){
					free(buf);
					return res;
				}
				for(i=0; i<nbits; i++, cx++){
					buf[cx]=(bits&0x01)?0:255;
					bits>>=1;
				}
			}
			if((*cb)(iscl,buf,cdata)==IMG_READ_CANCEL) break;
		}
	}
	free(buf);
	return 0;
}

/* 
 * Parse XBM header and return the dimensions and the number of
 * bytes-per-chunk (1 for X11, 2 for X10 bitmaps) 
 */
static int parse_xbm_header(FILE *file, unsigned int *width,
	unsigned int *height, short *bpc)
{
	char buf[XBM_MAX_LINE+1];
	char *res;
	fseek(file,0,SEEK_SET);
	
	*width=0;
	*height=0;
	*bpc=0;
	
	while((res=fgets(buf,XBM_MAX_LINE,file))){
		int n;
		char cstr[XBM_MAX_LINE+1];

		if(sscanf(res,"#define %" STRINGIFY(XBM_MAX_LINE) "s %d",cstr,&n)==2){
			if(strstr(cstr,"_width")) *width=n;
			if(strstr(cstr,"_height")) *height=n;
		}
		if((sscanf(res,"static const unsigned %5s",cstr)==1) ||
			(sscanf(res,"static unsigned %5s",cstr)==1) ||
			(sscanf(res,"static %5s",cstr)==1)){
			if(strstr(cstr,"char")) *bpc=1;
			if(strstr(cstr,"short")) *bpc=2;
		}
		if(*width && *height && *bpc) break;
	}
	if(!res)
		return (ferror(file))?IMG_EIO:IMG_EFILE;
	return 0;
}

static int find_data(FILE *file, fpos_t *pos)
{
	int c;	
	fseek(file,0,SEEK_SET);
	while(EOF!=(c=fgetc(file))){
		if(c=='{'){
			fgetpos(file,pos);
			break;
		}
	}
	if(c==EOF) return (ferror(file))?IMG_EIO:IMG_EFILE;
	return 0;
}

int img_open_xbm(const char *file_name, struct img_file *img, int flags)
{
	struct stat st;
	FILE *img_file=NULL, *mask_file=NULL;
	struct xbm_ld *ld;
	unsigned int img_width, mask_width;
	unsigned int img_height, mask_height;
	short img_bpc, mask_bpc;
	char *mask_file_name;
	char *token;
	int res;
	fpos_t img_data_offset, mask_data_offset;

	memset(img,0,sizeof(struct img_file));
	if(stat(file_name,&st)== -1) return IMG_EIO;
	img->cr_time=st.st_mtime;

	img_file=fopen(file_name,"r");
	if(!img_file) return IMG_EFILE;
	
	#ifndef XBM_DONT_LOOK_FOR_MASK
	/* make a mask filename (filename+_m.bm|xbm) and open
	 * the file for reading if it exists */
	mask_file_name=malloc(strlen(file_name)+3);
	if(!mask_file_name){
		fclose(img_file);
		return IMG_ENOMEM;
	}
	strcpy(mask_file_name,file_name);
	token=strrchr(mask_file_name,'.');
	if(token && strlen(token)>1){
		char *ext=strdup(token);
		*token='\0';
		strcat(mask_file_name,"_m");
		strcat(mask_file_name,ext);
		free(ext);
		mask_file=fopen(mask_file_name,"r");
	}
	free(mask_file_name);
	#endif
	
	res=parse_xbm_header(img_file,&img_width,&img_height,&img_bpc);
	if(res){
		fclose(img_file);
		if(mask_file) fclose(mask_file);
		dbg_trace("failed to parse xbm header\n");
		return res;
	}
	if(mask_file){
		res=parse_xbm_header(mask_file,&mask_width,&mask_height,&mask_bpc);
		if(res || mask_width!=img_width || mask_height!=img_height ||
			mask_bpc!=img_bpc){
				fclose(mask_file);
				mask_file=NULL;
		}else{
			res=find_data(mask_file,&mask_data_offset);
			if(res){
				fclose(mask_file);
				mask_file=NULL;
			}
		}
	}
	res=find_data(img_file,&img_data_offset);
	if(res){
		fclose(img_file);
		if(mask_file) fclose(mask_file);
		dbg_trace("failed to locale xbm data offset\n");
		return res;
	}
	
	ld=malloc(sizeof(struct xbm_ld));
	if(!ld){
		fclose(img_file);
		if(mask_file) fclose(mask_file);
		return IMG_ENOMEM;
	}
	ld->img_file=img_file;
	ld->mask_file=mask_file;
	ld->width=img_width;
	ld->height=img_height;
	ld->bpc=img_bpc;
	ld->img_data_offset=img_data_offset;
	ld->mask_data_offset=mask_data_offset;
	img->format=IMG_DIRECT;
	img->width=img_width;
	img->height=img_height;
	img->bpp=(mask_file)?16:8;
	img->orig_bpp=(mask_file)?2:1;
	img->red_mask=0x000000FF;
	img->green_mask=0x000000FF;
	img->blue_mask=0x000000FF;
	if(mask_file) img->alpha_mask=0x0000FF00;
	img->loader_data=(void*)ld;
	img->read_scanlines_fnc=&read_scanlines;
	img->close_fnc=&close;
	return 0;
}


static void close(struct img_file *img)
{
	struct xbm_ld *ld=(struct xbm_ld*)img->loader_data;
	fclose(ld->img_file);
	if(ld->mask_file) fclose(ld->mask_file);
}
