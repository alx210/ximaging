/*
 * Copyright (C) 2012-2017 alx@fastestcode.org
 * This software is distributed under the terms of the MIT license.
 * See the included LICENSE file for further information.
 */
 
/*
 * Silicon Graphics IRIS image reader.
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

/* IRIS magic */
#define SGI_IMAGE_ID 0x01DA

/* Storage format */
#define SGI_SF_VERBATIM 0
#define SGI_SF_RLE 1

/* Color mapping */
#define SGI_CM_NORMAL 0 /* B/W for 1 channel, RGB for 3 channel, RGBA for 4 */
#define SGI_CM_DITHER 1 /* (Obsolete) RGB packed in 1 byte as [BBGGGRRR] */
#define SGI_CM_SCREEN 2 /* (Obsolete) 8 BPP indexed */
#define SGI_CM_CLRMAP 3 /* Contains no image data but a color map */

struct sgi_header{
	uint16_t id;		/* must be SGI_IMAGE_ID */
	uint8_t stor_fmt;	/* storage format SGI_SF* */
	uint8_t bpc;		/* bytes per pixel channel (1 or 2) */
	uint16_t dims;		/* number of dimensions (1,2 or 3) */
	uint16_t xres;		/* resolution */
	uint16_t yres;
	uint16_t zsize;		/* number of channels */
	int32_t pxmin;		/* min/max pixel value (intensity) */
	int32_t pxmax;
	
	#if 0
	int32_t _p1;		/* padding */
	int8_t name[80];	/* image name */
	#endif
	
	int32_t clrmap;		/* colormap SGI_CM* */
	
	#if 0
	uint8_t _p2[404];	/* padding */
	#endif
};
#define HEADER_SIZE	512	/* sgi_header struct size in file */


struct sgi_ld {
	FILE *file;
	struct sgi_header hdr;
	
	/* scanline tables for encoded files */
	uint32_t tab_len;
	uint32_t *start_tab;
};

/* Local prototypes */
static void close_image(struct img_file *img);
static void bswap_header_struct(struct sgi_header* h);
static int read_scanlines(struct img_file *img,
	img_scanline_cbt cb, void *cdata);
static int read_rle(uint8_t *buffer, size_t len, size_t bpc, FILE *file);
static void planes_to_row(uint8_t *planes, short nplanes,
	unsigned int xres, uint8_t *row, short bpc);

/*
 * Decode RLE data from the file
 */
static int read_rle(uint8_t *buffer, size_t size, size_t bpc, FILE *file)
{
	unsigned short c;
	unsigned short run;
	size_t i,j;
	
	for(i=0; i<size; c=0){
		if(fread(&c,1,bpc,file)<bpc) break;
		run=c&0x007F;
		if(i+run>size){
			return IMG_EFILE;
		}else if(!run){
			return 0;
		}
		
		if(!(c&0x0080)){
			if(fread(&c,1,bpc,file)<bpc) break;
			if(bpc>1){
				unsigned short *p=(unsigned short*)buffer;
				for(j=0; j<run; j++, i++) p[i]=c;
			}else{
				memset(&buffer[i],(int)c,run);
				i+=run;
			}
		}else{
			if(fread(&buffer[i],1,run*bpc,file)<run*bpc) break;
			i+=run*bpc;
		}
	}
	
	if(ferror(file)) return IMG_EIO;
	if(feof(file)) return IMG_EFILE;
	return 0;
}

/*
 * Read uncompressed scanlines
 */
static int read_scanlines(struct img_file *img,
	img_scanline_cbt cb, void *cdata)
{
	struct sgi_ld *ld=(struct sgi_ld*)img->loader_data;
	uint8_t *planes;
	uint8_t *row;
	unsigned int psize;
	unsigned int ip, iscl;
	int res=0;

	row=malloc(ld->hdr.xres*ld->hdr.zsize);
	if(!row) return IMG_ENOMEM;
	
	psize=ld->hdr.xres*ld->hdr.bpc;
	planes=malloc(psize*ld->hdr.zsize);
	if(!planes){
		free(row);
		return IMG_ENOMEM;
	}
	memset(planes,0xFF,psize*ld->hdr.zsize);
	if(ld->hdr.stor_fmt==SGI_SF_VERBATIM){
		for(iscl=0; iscl<ld->hdr.yres; iscl++){
			for(ip=0; ip<ld->hdr.zsize; ip++){
				fseek(ld->file,HEADER_SIZE+
					(iscl*psize)+(ip*(ld->hdr.yres*psize)),SEEK_SET);
				fread(&planes[ip*psize],1,psize,ld->file);
			}
			if(feof(ld->file) || ferror(ld->file)) break;
			planes_to_row(planes,ld->hdr.zsize,ld->hdr.xres,row,ld->hdr.bpc);
			if((*cb)(iscl,row,cdata)==IMG_READ_CANCEL) break;
		}
	}else{
		for(iscl=0; iscl<ld->hdr.yres; iscl++){
			for(ip=0; ip<ld->hdr.zsize; ip++){
				fseek(ld->file,ld->start_tab[(ip*ld->hdr.yres)+iscl],SEEK_SET);
				res=read_rle(&planes[ip*psize],psize,ld->hdr.bpc,ld->file);
				if(res) break;
			}
			if(res)break;
			planes_to_row(planes,ld->hdr.zsize,ld->hdr.xres,row,ld->hdr.bpc);
			if((*cb)(iscl,row,cdata)==IMG_READ_CANCEL) break;
		}
	}
	free(row);
	free(planes);
	if(!res){
		if(feof(ld->file)) return IMG_EFILE;
		if(ferror(ld->file)) return IMG_EIO;
	}
	return res;
}

/*
 * Convert 'nplanes' planes to a row
 */
static void planes_to_row(uint8_t *planes, short nplanes,
	unsigned int xres, uint8_t *row, short bpc)
{
	unsigned int i,j,k;
	if(bpc<2){
		for(i=0, k=0; i<xres; i++, k+=nplanes){
			for(j=0; j<nplanes; j++)
				row[k+j]=planes[i+xres*j];
		}
	}else{
		uint16_t *ptr=(uint16_t*)planes;
		if(is_big_endian()){
			for(i=0, k=0; i<xres*bpc; i++, k+=nplanes){
				for(j=0; j<nplanes; j++)
					row[k+j]=(ptr[i+xres*j]/256);
			}
		}else{
			for(i=0, k=0; i<xres*bpc; i++, k+=nplanes){
				for(j=0; j<nplanes; j++)
					row[k+j]=(bswap_word(ptr[i+xres*j])/256);
			}
		}
	}
}

int img_open_sgi(const char *file_name, struct img_file *img, int flags)
{
	FILE *file;
	struct stat st;
	struct sgi_header hdr;
	struct sgi_ld *ld;
	
	memset(img,0,sizeof(struct img_file));	
	if(stat(file_name,&st)== -1) return IMG_EIO;
	img->cr_time=st.st_mtime;
		
	file=fopen(file_name,"r");
	if(!file) return IMG_EFILE;
	
	fread(&hdr.id,2,1,file);
	hdr.stor_fmt=fgetc(file);
	hdr.bpc=fgetc(file);
	fread(&hdr.dims,2,1,file);
	fread(&hdr.xres,2,1,file);
	fread(&hdr.yres,2,1,file);
	fread(&hdr.zsize,2,1,file);
	fread(&hdr.pxmin,4,1,file);
	fread(&hdr.pxmax,4,1,file);
	fseek(file,84,SEEK_CUR);
	fread(&hdr.clrmap,4,1,file);
	if(!is_big_endian()) bswap_header_struct(&hdr);
	
	/* single scanline, greyscale */
	if(hdr.dims==1) hdr.yres=1;
		
	if(feof(file) || ferror(file) || hdr.id!=SGI_IMAGE_ID || 
		!hdr.xres || !hdr.yres || !hdr.zsize){
		fclose(file);
		return IMG_EFILE;
	}	
		
	if( hdr.bpc>2 || hdr.clrmap==SGI_CM_SCREEN || hdr.clrmap==SGI_CM_CLRMAP){
		fclose(file);
		return IMG_EUNSUP;
	}
	
	ld=malloc(sizeof(struct sgi_ld));
	if(!ld){
		fclose(file);
		return IMG_ENOMEM;
	}	
	memcpy(&ld->hdr,&hdr,sizeof(struct sgi_header));
	
	/* load the scanline tables if RLE */
	if(hdr.stor_fmt==SGI_SF_RLE){
		ld->tab_len=(hdr.yres*hdr.zsize);
		ld->start_tab=calloc(sizeof(uint32_t),ld->tab_len);

		if(!ld->start_tab){
			free(ld);
			fclose(file);
			return IMG_ENOMEM;
		}

		fseek(file,HEADER_SIZE,SEEK_SET);
		fread(ld->start_tab,sizeof(uint32_t),ld->tab_len,file);
		if(ferror(file) || feof(file)){
			free(ld->start_tab);
			free(ld);
			fclose(file);
			return IMG_EFILE;
		}

		/* ensure proper endianness in the table */
		if(!is_big_endian()){
			int i;
			for(i=0; i<ld->tab_len; i++){
				ld->start_tab[i]=bswap_dword(ld->start_tab[i]);
			}
		}
	}else{
		/* verbatim */
		ld->tab_len=0;
	}
	ld->file=file;
	img->width=hdr.xres;
	img->height=hdr.yres;
	img->bpp=hdr.zsize*8;
	img->loader_data=ld;
	img->read_scanlines_fnc=&read_scanlines;
	img->close_fnc=&close_image;
	img->orig_bpp=img->bpp*hdr.bpc;
	img->tform=IMGT_VFLIP;

	switch(hdr.zsize){
		case 1:
		img->green_mask=img->blue_mask=img->red_mask=0x000000FF;
		break;
		case 2:
		img->green_mask=img->blue_mask=img->red_mask=0x000000FF;
		img->alpha_mask=0x0000FF00;
		break;
		case 4:
		img->alpha_mask=0xFF000000;
		case 3:
		img->red_mask=0x000000FF;
		img->green_mask=0x0000FF00;
		img->blue_mask=0x00FF0000;
		break;
	}
	if(is_big_endian()){
		img->red_mask=bswap_dword(img->red_mask);
		img->green_mask=bswap_dword(img->green_mask);
		img->blue_mask=bswap_dword(img->blue_mask);
		img->alpha_mask=bswap_dword(img->alpha_mask);
	}
	return 0;
}

static void close_image(struct img_file *img)
{
	struct sgi_ld *ld=(struct sgi_ld*)img->loader_data;
	
	fclose(ld->file);
	if(ld->tab_len)	free(ld->start_tab);
	free(ld);
}

static void bswap_header_struct(struct sgi_header* h)
{
	h->id=bswap_word(h->id);
	h->dims=bswap_word(h->dims);
	h->xres=bswap_word(h->xres);
	h->yres=bswap_word(h->yres);
	h->zsize=bswap_word(h->zsize);
	h->pxmin=bswap_dword(h->pxmin);
	h->pxmax=bswap_dword(h->pxmax);
	h->clrmap=bswap_dword(h->clrmap);
}
