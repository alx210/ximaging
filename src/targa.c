/*
 * Copyright (C) 2012-2017 alx@fastestcode.org
 * This software is distributed under the terms of the MIT license.
 * See the included LICENSE file for further information.
 */
 
/*
 * TrueVision TARGA reader. Supports 8-32 bpp raw and RLE encoded image data.
 */

#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <string.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "imgfile.h"
#include "bswap.h"
#include "debug.h"

/* Image type constants */
#define TGA_IMT_NONE	0
#define TGA_IMT_RAW_PAL	1
#define TGA_IMT_RAW_RGB	2
#define TGA_IMT_RAW_BW	3
#define TGA_IMT_RLE_PAL	9
#define TGA_IMT_RLE_RGB	10
#define TGA_IMT_RLE_BW	11

/* Colormap type constants */
#define TGA_CMT_NONE	0
#define TGA_CMT_RGB		1

/* Image descriptor tga_header.im_desc macros */
#define TGA_IMD_ORIGIN(im_desc) ((im_desc&0x30)>>4)
#define TGA_ORIGIN_BL	0
#define TGA_ORIGIN_BR	1
#define TGA_ORIGIN_TL	2
#define TGA_ORIGIN_TR	3
#define TGA_IMD_ALPHABITS(im_desc) (im_desc&0x0F)

/* TGA main header */
struct tga_header{
	uint8_t	id_len;		/* length of image ID field */
	uint8_t	cm_type;	/* One of TGA_CMT */
	
	uint8_t	im_type;	/* One of TGA_IMT */
	
	/* color map specs */
	uint16_t cm_first;	/* first color map entry */
	uint16_t cm_length;	/* number of color map entries */
	uint8_t	cm_esize;	/* color map entry size in bits 15|16|24|32 */
	
	/* image origin and dimensions */
	uint16_t im_origin_x;
	uint16_t im_origin_y;
	uint16_t im_width;
	uint16_t im_height;
	uint8_t	im_bpp;		/* bits per pixel */
	
	/* image descriptor bits:
	 * 6-8 reserved, 4-5 image origin, 0-3 aplha channel
	 * Origin bits set: 00=bottom left, 01=bottom right,
	 * 10=top left, 11=top right
	 */
	uint8_t	im_desc;
};
#define TGA_HEADER_SIZE	18	/* struct size in file */

/* TGA footer */
struct tga_footer{
	uint32_t ext_area_offset;
	uint32_t dev_dir_offset;
};
#define TGA_FOOTER_SIZE 8	/* struct size in file */

#define TGA_SIGNATURE "TRUEVISION-XFILE."

struct tga_ld{
	struct tga_header hdr;
	FILE *file;
	size_t scl_size;
	size_t data_offset;
	size_t cmap_offset;
	unsigned char *scl_buf;
};

#define TGA_16RMASK (0x1F<<10)
#define TGA_16GMASK (0x1F<<5)
#define TGA_16BMASK (0x1F)
#define TGA_24RMASK 0x00FF0000
#define TGA_24GMASK 0x0000FF00
#define TGA_24BMASK 0x000000FF
#define TGA_32AMASK 0xFF000000

/* Local prototypes */
static int read_cmap(struct img_file *img, void *buffer);
static int read_rgb_scanlines(struct img_file *img,
	img_scanline_cbt cb, void *cdata);
static int read_cm_scanlines(struct img_file *img,
	img_scanline_cbt cb, void *cdata);
static int build_grs_cmap(struct img_file *img, void *buffer);
static void bswap_header(struct tga_header *hdr);
static void bswap_footer(struct tga_footer *ftr);
static void close_image(struct img_file *img);
static size_t fread_be(void*,size_t,size_t,FILE*);

/* 
 * Read color map.
 *
 * TODO: if there are known TGA writers with other than 24 bpp color
 *       map entry support, a conversion routine is needed here.
 */
static int read_cmap(struct img_file *img, void *buffer)
{
	size_t read;
	unsigned char clut[768];
	struct tga_ld *ld=(struct tga_ld*)img->loader_data;
	unsigned char *ptr=buffer;
	int i;
	
	if(ld->hdr.cm_esize!=24){
		dbg_trace("color map size: %d\n",ld->hdr.cm_esize);
		return IMG_EUNSUP;
	}

	if(fseek(ld->file,ld->cmap_offset,SEEK_SET)== -1)
		return IMG_EFILE;
	
	read=fread(clut,1,768,ld->file);
	if(read<768) return IMG_EFILE;
	
	/* transform BGR to RGB */
	for(i=0; i<256; i++){
		ptr[i*3]=clut[i*3+2];
		ptr[i*3+1]=clut[i*3+1];
		ptr[i*3+2]=clut[i*3];
	}
	return 0;
}

/* Read true color scanline */
static int read_rgb_scanlines(struct img_file *img,
	img_scanline_cbt cb, void *cdata)
{
	struct tga_ld *ld=(struct tga_ld*)img->loader_data;
	unsigned int iscl;
	unsigned int scl_size;
	unsigned char *buffer;
	size_t bypp;
	size_t read;
	size_t (*read_fp)(void*,size_t,size_t,FILE*);
	
	read_fp=(is_big_endian())? &fread_be:&fread;
	if(fseek(ld->file,ld->data_offset,SEEK_SET)== -1)
		return IMG_EFILE;
	bypp=ld->hdr.im_bpp/8;
	
	scl_size=img->width*(img->bpp/8);
	buffer=malloc(scl_size);
	if(!buffer) return IMG_ENOMEM;
	
	if(ld->hdr.im_type==TGA_IMT_RLE_RGB){
		size_t total, run, i;
		int lead;
		unsigned char chunk[4];
		unsigned char *ptr;
		
		for(iscl=0; iscl<img->height; iscl++){
			ptr=buffer;
			total=0;

			while(total<scl_size){
				lead=fgetc(ld->file);
				run=(lead&0x7f)+1;
				total+=(run*bypp);
				if(total>scl_size){
					free(buffer);
					return IMG_EFILE;
				}
				if(lead&0x80){
					(*read_fp)(chunk,bypp,1,ld->file);
					for(i=0; i<run; ptr+=bypp, i++){
						memcpy(ptr,chunk,bypp);
					}
				}else{
					read=(*read_fp)(ptr,bypp,run,ld->file);
					ptr+=(run*bypp);
				}
				if(feof(ld->file)||ferror(ld->file)){
					free(buffer);
					return IMG_EFILE;
				}
			}
			if((*cb)(iscl,buffer,cdata)==IMG_READ_CANCEL) break;
		}
	}else{
		for(iscl=0; iscl<img->height; iscl++){
			read=(*read_fp)(buffer,bypp,img->width,ld->file);
			if(read<img->width){
				free(buffer);
				return IMG_EFILE;
			}
			if((*cb)(iscl,buffer,cdata)==IMG_READ_CANCEL) break;
		}
	}
	free(buffer);
	return 0;
}

/* Read multiword values converting them to big endian */
static size_t fread_be(void *ptr, size_t size, size_t nmemb, FILE *file)
{
	size_t i=0;
	size_t read;

	switch(size){
		case 2:{
			uint16_t *p=(uint16_t*)ptr;
			for(i=0; i<nmemb; i++){
				read=fread(p,size,1,file);
				if(!read) return i;
				*p=bswap_word(*p);
				p++;				
			}
		}break;
		case 4:{
			uint32_t *p=(uint32_t*)ptr;
			for(i=0; i<nmemb; i++){
				read=fread(p,size,1,file);
				if(!read) return i;
				*p=bswap_dword(*p);
				p++;				
			}
		}break;
	}
	return i;
}

/* Read pseudo color scanlines */
static int read_cm_scanlines(struct img_file *img,
	img_scanline_cbt cb, void *cdata)
{
	struct tga_ld *ld=(struct tga_ld*)img->loader_data;
	size_t read;
	unsigned char *buffer;
	unsigned int iscl;
	unsigned int scl_size;
	
	if(fseek(ld->file,ld->data_offset,SEEK_SET)== -1)
		return IMG_EFILE;
	
	scl_size=img->width*(img->bpp/8);
	buffer=malloc(scl_size);
	
	if(ld->hdr.im_type == TGA_IMT_RLE_PAL ||
	 ld->hdr.im_type == TGA_IMT_RLE_BW){
		unsigned char *ptr;
		size_t total, run;
		int cur;

		for(iscl=0; iscl<img->height; iscl++){
			ptr=buffer;
			total=0;
			while(total<scl_size){
				cur=fgetc(ld->file);
				run=(cur&0x7f)+1;
				total+=run;
				if(total>scl_size){
					free(buffer);
					return IMG_EFILE;
				}
				if(cur&0x80){
					cur=fgetc(ld->file);
					memset(ptr,cur,run);
				}else{
					read=fread(ptr,1,run,ld->file);
					if(read<run){
						free(buffer);
						return IMG_EFILE;
					}
				}
				ptr+=run;
				if(feof(ld->file)||ferror(ld->file)){
					free(buffer);
					return IMG_EFILE;
				}
			}
			if((*cb)(iscl,buffer,cdata)==IMG_READ_CANCEL) break;
		}
	}else{
		for(iscl=0; iscl<img->height; iscl++){
			read=fread(buffer,1,scl_size,ld->file);
			if(read<scl_size){
				free(buffer);
				return IMG_EFILE;
			}
			if((*cb)(iscl,buffer,cdata)==IMG_READ_CANCEL) break;
		}
	}
	return 0;
}


static int build_grs_cmap(struct img_file *img, void *buffer)
{
	char *ptr = (char*) buffer;
	unsigned int i;
	
	for(i = 0; i < 256; i++) {
		ptr[i * 3] = ptr[i * 3 + 1] = ptr[i * 3 + 2] = i;
	}
	
	return 0;
}

/* Parse targa header and initialize struct img_file fields */
int img_open_tga(char *file_name, struct img_file *img, int flags)
{
	FILE *file;
	struct stat st;
	struct tga_header hdr;
	struct tga_footer ftr;
	struct tga_ld *ld;
	size_t read;
	char signature[18];
	int has_footer=0;
	
	memset(img,0,sizeof(struct img_file));	
	if(stat(file_name,&st)== -1) return IMG_EIO;
	img->cr_time=st.st_mtime;

	file=fopen(file_name,"r");
	if(!file) return IMG_EIO;
	
	/* read header fields */
	hdr.id_len=fgetc(file);
	hdr.cm_type=fgetc(file);
	hdr.im_type=fgetc(file);
	fread(&hdr.cm_first,2,1,file);
	fread(&hdr.cm_length,2,1,file);
	hdr.cm_esize=fgetc(file);
	fread(&hdr.im_origin_x,2,1,file);
	fread(&hdr.im_origin_y,2,1,file);	
	fread(&hdr.im_width,2,1,file);
	fread(&hdr.im_height,2,1,file);
	hdr.im_bpp=fgetc(file);
	hdr.im_desc=fgetc(file);
	
	/* check for errors */
	if(ferror(file) || feof(file) || !hdr.im_bpp ||
		 !hdr.im_width || !hdr.im_height){
		fclose(file);
		return IMG_EFILE;
	}

	if(is_big_endian()) bswap_header(&hdr);

	/* check if footer is present and read it if so */
	fseek(file,-(strlen(TGA_SIGNATURE)+1),SEEK_END);
	read=fread(signature,strlen(TGA_SIGNATURE)+1,1,file);

	if(read && !strncmp(signature,TGA_SIGNATURE,strlen(TGA_SIGNATURE))){
		has_footer=1;
		fseek(file,-((TGA_FOOTER_SIZE)+
			(strlen(TGA_SIGNATURE)+1)),SEEK_CUR);
		fread(&ftr.ext_area_offset,1,4,file);
		fread(&ftr.dev_dir_offset,1,4,file);
		if(ferror(file) || feof(file)){
			fclose(file);
			return IMG_EFILE;
		}
		if(is_big_endian()) bswap_footer(&ftr);
	}
	
	/* check if we can read this type */
	if(hdr.im_type==0 || hdr.im_type>11 || (hdr.cm_type && hdr.im_bpp>8)){
		fclose(file);
		return IMG_EUNSUP;
	}

	ld=malloc(sizeof(struct tga_ld));
	if(!ld){
		fclose(file);
		return IMG_ENOMEM;
	}
	
	memset(ld,0,sizeof(struct tga_ld));
	
	ld->file=file;
	ld->data_offset=TGA_HEADER_SIZE+hdr.id_len;
	memcpy(&ld->hdr,&hdr,sizeof(struct tga_header));
		
	img->loader_data=(void*)ld;
	img->close_fnc=&close_image;
	img->width=hdr.im_width;
	img->height=hdr.im_height;
	img->bpp=hdr.im_bpp;
	img->orig_bpp=hdr.im_bpp;

	switch(hdr.im_type){
		case TGA_IMT_RAW_PAL:
		case TGA_IMT_RLE_PAL:
		/* color mapped images */
		ld->cmap_offset=ld->data_offset;
		ld->data_offset+=hdr.cm_length*(hdr.cm_esize/8);
		img->format=IMG_PSEUDO;
		img->read_cmap_fnc=&read_cmap;
		img->read_scanlines_fnc=&read_cm_scanlines;
		break;
		
		case TGA_IMT_RAW_RGB:
		case TGA_IMT_RLE_RGB:
		/* high/true-color images */
		img->format=IMG_DIRECT;
		img->read_scanlines_fnc=&read_rgb_scanlines;
		break;
		
		case TGA_IMT_RAW_BW:
		case TGA_IMT_RLE_BW:
		/* grayscale */
		img->format = IMG_PSEUDO;
		img->read_cmap_fnc = &build_grs_cmap;
		img->read_scanlines_fnc = &read_cm_scanlines;
		break;
		
		default:
		fclose(file);
		return IMG_EUNSUP;
		break;
	}

	switch(hdr.im_bpp){
		case 16:
		img->red_mask=TGA_16RMASK;
		img->green_mask=TGA_16GMASK;
		img->blue_mask=TGA_16BMASK;
		break;
		case 32:
		if(TGA_IMD_ALPHABITS(hdr.im_desc))
			img->alpha_mask=TGA_32AMASK;
		case 24:
		img->red_mask=TGA_24RMASK;
		img->green_mask=TGA_24GMASK;
		img->blue_mask=TGA_24BMASK;
		break;
	}

	/* set transform flags */
	switch(TGA_IMD_ORIGIN(hdr.im_desc)){
		case TGA_ORIGIN_BL:
		img->tform=IMGT_VFLIP;
		break;
		case TGA_ORIGIN_BR:
		img->tform=IMGT_VFLIP|IMGT_HFLIP;
		break;
		case TGA_ORIGIN_TR:
		img->tform=IMGT_HFLIP;
		break;
	}
	
	/* get extended attributes if presend */
	if(has_footer && ftr.ext_area_offset){
		uint8_t attrib;
		union {	uint8_t argb[4]; uint32_t pixel; } bg;
		
		/* attributes */
		fseek(file,ftr.ext_area_offset+494,SEEK_SET);
		attrib=fgetc(file);

		if(attrib<3){
			/* undefined data in the alpha channel */
			img->alpha_mask=0;
		}else if(attrib==4){
			/* pre-multiplied alpha */
			img->flags|=IMGF_PMALPHA;
		}
		/* Key Color (background) */
		fseek(file,ftr.ext_area_offset+470,SEEK_SET);
		fread(&bg.argb,4,1,file);
		if(bg.pixel){
			img->flags|=IMGF_BGCOLOR;
			img->bg_pixel=(((uint32_t)bg.argb[0]<<24)|
				((uint32_t)bg.argb[1]<<16)|
				((uint32_t)bg.argb[2]<<8)|bg.argb[3]);
		}
	}
	return 0;
}

static void close_image(struct img_file *img)
{
	struct tga_ld *ld=(struct tga_ld*)img->loader_data;
	fclose(ld->file);
	free(img->loader_data);
}

static void bswap_header(struct tga_header *hdr)
{
	hdr->cm_first=bswap_word(hdr->cm_first);
	hdr->cm_length=bswap_word(hdr->cm_length);
	hdr->im_origin_x=bswap_word(hdr->im_origin_x);
	hdr->im_origin_y=bswap_word(hdr->im_origin_y);
	hdr->im_width=bswap_word(hdr->im_width);
	hdr->im_height=bswap_word(hdr->im_height);
}

static void bswap_footer(struct tga_footer *ftr)
{
	ftr->ext_area_offset=bswap_dword(ftr->ext_area_offset);
	ftr->dev_dir_offset=bswap_dword(ftr->dev_dir_offset);
}
