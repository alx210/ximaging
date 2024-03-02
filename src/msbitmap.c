/*
 * Copyright (C) 2012-2024 alx@fastestcode.org
 * This software is distributed under the terms of the MIT license.
 * See the included LICENSE file for further information.
 */
 
/*
 * MS-Windows Bitmap reader. Supports V3+ raw pseudo and true color images.
 */

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "imgfile.h"
#include "bswap.h"
#include "debug.h"


struct bmp_file_hdr{
	uint16_t type;	/* must be 0x4D42 (BM) */
	uint32_t size;	/* size of the whole file */
	uint32_t resrv;
	uint32_t off_bits; /* image data offset */
};

#define BMP_FILE_HDR_SIZE 14 /* struct size in BMP file */

struct bmp_info_hdr{
	uint32_t hdr_size;	/* size of this structure */
	int32_t width;
	int32_t height;
	
	int16_t planes;		/* must be 1 */
	
	int16_t bpp;
	uint32_t comp;
	
	uint32_t size_image;	/* size of image data */
	int32_t x_ppm;
	int32_t y_ppm;
	uint32_t clr_used;
	uint32_t clr_imp;
};

#define BMP_INFO_HDR_SIZE 40

struct bmp_bitmasks {
	uint32_t r;
	uint32_t g;
	uint32_t b;
	uint32_t a;
};

#define BMP_BITMASKS_SIZE 16 /* struct size in BMP file */

struct bmp_ld {
	FILE *file;
	size_t data_offset;
	size_t clut_offset;
	size_t pixel_size;
	size_t pad_size;
	size_t scl_size;
};

/* Local prototypes */
static int read_scanlines(struct img_file *img,
	img_scanline_cbt cb, void *cdata);
static int read_cmap(struct img_file *img, void *buffer);
static void bswap_dstruct(struct bmp_info_hdr *ih, struct bmp_bitmasks *bm);
static void close_image(struct img_file *img);
static size_t fread_be(void *ptr, size_t size, size_t nmemb, FILE *file);

static int read_cmap(struct img_file *img, void *buffer)
{
	struct bmp_ld *ld=(struct bmp_ld*)img->loader_data;
	unsigned char clut[1024]; /* BMP clut is BGRX */
	unsigned char *ptr=buffer;
	size_t read;
	int i;

	if(fseek(ld->file,ld->clut_offset,SEEK_SET)== -1){
		return IMG_EFILE;
	}
	read=fread(clut,1,1024,ld->file);
	if(read<1024) return IMG_EFILE;

	/* translate BGRX table to RGB */
	for(i=0; i<256; i++){
		ptr[i*3]=clut[i*4+2];
		ptr[i*3+1]=clut[i*4+1];
		ptr[i*3+2]=clut[i*4];
	}
	return 0;
}

static int read_scanlines(struct img_file *img,
	img_scanline_cbt cb, void *cdata)
{
	struct bmp_ld *ld=(struct bmp_ld*)img->loader_data;
	size_t (*read_proc)(void*,size_t,size_t,FILE*)=&fread;
	size_t read;
	unsigned char *buffer;
	unsigned int i;
	int res=0;
	
	if(is_big_endian())	read_proc=&fread_be;
	
	if(fseek(ld->file,ld->data_offset,SEEK_SET)== -1)
		return IMG_EFILE;
	
	buffer=malloc(ld->scl_size);
	if(!buffer) return IMG_ENOMEM;
	
	for(i=0; i<img->height; i++){
		read=read_proc(buffer,1,ld->scl_size,ld->file);
		fseek(ld->file,ld->pad_size,SEEK_CUR);
		if(read<ld->scl_size){
			res=IMG_EFILE;
			break;
		}
		if((*cb)(i,buffer,cdata)==IMG_READ_CANCEL) break;
	}

	return res;
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


int img_open_bmp(char *file_name, struct img_file *img)
{
	FILE *file;
	struct stat st;
	uint16_t magic;
	uint32_t data_offset;
	struct bmp_info_hdr ih;
	struct bmp_bitmasks bm;
	struct bmp_ld *ld;

	memset(img,0,sizeof(struct img_file));	
	
	if(stat(file_name,&st)== -1) return IMG_EIO;
	img->cr_time=st.st_mtime;
	
	file=fopen(file_name,"r");
	if(!file) return IMG_EIO;
	
	/* read file header */
	fread(&magic,2,1,file);
	fseek(file,10,SEEK_SET);
	fread(&data_offset,4,1,file);
	if(is_big_endian()){
		magic=bswap_word(magic);
		data_offset=bswap_dword(data_offset);
	}
	if(feof(file) || ferror(file) || magic!=0x4D42){
		fclose(file);
		return IMG_EUNSUP;
	}
	
	/* read info header */
	fseek(file,BMP_FILE_HDR_SIZE,SEEK_SET);
	fread(&ih.hdr_size,4,1,file);
	fread(&ih.width,4,1,file);
	fread(&ih.height,4,1,file);
	fseek(file,2,SEEK_CUR);
	fread(&ih.bpp,2,1,file);
	fread(&ih.comp,4,1,file);
	if(feof(file)||ferror(file)){
		fclose(file);
		return IMG_EFILE;
	}
	if(is_big_endian()) bswap_dstruct(&ih,NULL);
	
	/* only raw 8-32 bit bitmaps */
	if((ih.comp && ih.comp!=3) || ih.bpp<8 || !ih.width || !ih.height ){
		fclose(file);
		return IMG_EUNSUP;
	}
	
	/* read the masks if any */
	if(ih.comp==3){
		fseek(file,BMP_FILE_HDR_SIZE+BMP_INFO_HDR_SIZE,SEEK_SET);
		fread(&bm.r,4,1,file);
		fread(&bm.g,4,1,file);
		fread(&bm.b,4,1,file);
		fread(&bm.a,4,1,file);
		if(feof(file)||ferror(file)){
			fclose(file);
			return IMG_EFILE;
		}
		if(is_big_endian()) bswap_dstruct(NULL,&bm);
		img->red_mask=bm.r;
		img->green_mask=bm.g;
		img->blue_mask=bm.b;
		img->alpha_mask=bm.a;

		if(is_big_endian()){
			img->red_mask=bswap_dword(img->red_mask);
			img->green_mask=bswap_dword(img->green_mask);
			img->blue_mask=bswap_dword(img->blue_mask);
			img->alpha_mask=bswap_dword(img->alpha_mask);
		}
	}else{
		/* or set to default (BGR) */
		if(ih.bpp==16){
			img->red_mask=(0x1F<<10);
			img->green_mask=(0x1F<<5);
			img->blue_mask=(0x1F);
		}else{
			img->red_mask=(0x00FF0000);
			img->green_mask=(0x0000FF00);
			img->blue_mask=(0x000000FF);
		}
	}
		
	ld=calloc(1,sizeof(struct bmp_ld));
	if(!ld){
		fclose(file);
		return IMG_ENOMEM;
	}
	
	ld->file=file;
	ld->data_offset=data_offset;
	ld->clut_offset=BMP_FILE_HDR_SIZE+ih.hdr_size;
	ld->pixel_size=ih.bpp/8;
	ld->scl_size=ih.width*ld->pixel_size;
	/* scanline padding */
	if(ld->pixel_size<4 && ld->scl_size%4){
		ld->pad_size=((((ld->scl_size+4)>>2)<<2)-ld->scl_size);
	}
	img->bpp=ih.bpp;
	img->width=ih.width;
	img->height=abs(ih.height);

	/* positive height means the image is upside down */
	img->tform=(ih.height>0)?IMGT_VFLIP:0;
	img->loader_data=(void*)ld;
	img->read_scanlines_fnc=&read_scanlines;
	img->read_cmap_fnc=(ih.bpp==8)?(&read_cmap):NULL;
	img->format=(ih.bpp==8)?IMG_PSEUDO:IMG_DIRECT;
	img->close_fnc=&close_image;
	img->orig_bpp=ih.bpp;

	return 0;
}

static void close_image(struct img_file *img)
{
	struct bmp_ld *ld=(struct bmp_ld*)img->loader_data;
	fclose(ld->file);
	free(ld);
	memset(img,0,sizeof(struct img_file));
}

static void bswap_dstruct(struct bmp_info_hdr *ih, struct bmp_bitmasks *bm)
{
	if(ih){
		ih->hdr_size=bswap_dword(ih->hdr_size);
		ih->width=bswap_dword(ih->width);
		ih->height=bswap_dword(ih->height);
		ih->bpp=bswap_word(ih->bpp);
	}
	if(bm){
		bm->r=bswap_dword(bm->r);
		bm->g=bswap_dword(bm->g);
		bm->b=bswap_dword(bm->b);
		bm->a=bswap_dword(bm->a);
	}
}
