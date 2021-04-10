/*
 * Copyright (C) 2012-2017 alx@fastestcode.org
 * This software is distributed under the terms of the MIT license.
 * See the included LICENSE file for further information.
 */
 
/*
 * X11 Pixmap (XPM3) file reader.
 */

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <ctype.h>
#include <memory.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include "imgfile.h"
#include "hashtbl.h"
#include "bswap.h"
#include "debug.h"

#include "xclrtab.h" /* X color name to RGB mappings */

#define XPM_MAX_LINE	255	/* for the header/color table entries */
#define XPM_MAX_BPC		3	/* max bytes per character */

struct color {
	char sym[XPM_MAX_BPC+1];
	uint8_t r;
	uint8_t g;
	uint8_t b;
	uint8_t a;
};

struct loader_data {
	unsigned int width;
	unsigned int height;
	FILE *file;
	fpos_t data_pos;
	struct color *clut;
	int bpc;
	uint8_t *ci;
	hashtbl_t *hci;
};

struct sym_rec {
	char *sym;
	unsigned long value;
};

/* Local prototypes */
static int read_color_table(FILE *file, struct color *table,
	int ncolors, int bpc);
int init_clut(struct loader_data *ld, struct color *table,
	int ncolors, int bpc);
static int build_rgb_table(void);
static int get_named_rgb(struct color *c, char *name);
static int seek_to_data(FILE *file);
static int parse_color(struct color *c, int ctype, char *cval);
static void close(struct img_file *img);
static int read_scanlines(struct img_file*,img_scanline_cbt,void*);
static int sym_compare_nocase(struct sym_rec*,struct sym_rec*);
static hashkey_t sym_hash_nocase(struct sym_rec*);
static int sym_compare(struct sym_rec*,struct sym_rec*);
static hashkey_t sym_hash(struct sym_rec*);

/* X symbolic color name table */
static hashtbl_t *rgb_table=NULL;

/*
 * One-time color name table init
 */
static int build_rgb_table(void)
{
	int i;
	struct sym_rec rec;

	dbg_assert(rgb_table==NULL);
	rgb_table=ht_alloc(NUM_XCOLORS,0,sizeof(struct sym_rec),
		(ht_hash_ft)sym_hash_nocase,(ht_cmp_ft)sym_compare_nocase);
	if(!rgb_table) return IMG_ENOMEM;
	
	for(i=0; xclrtab[i].name!=NULL; i++){
		rec.sym=xclrtab[i].name;
		rec.value=i;
		if(ht_insert(rgb_table,&rec)){
			ht_free(rgb_table);
			rgb_table=NULL;
			return IMG_ENOMEM;
		}
	}
	return 0;
}

/*
 * Retrieve RGB for X color name
 */
static int get_named_rgb(struct color *c, char *name)
{
	int res;
	struct sym_rec rec;	

	if(!rgb_table){
		res=build_rgb_table();
		if(res) return res;
	}
	rec.sym=name;
	res=ht_lookup(rgb_table,&rec,&rec);
	if(res) return res;

	c->r=xclrtab[rec.value].r;
	c->g=xclrtab[rec.value].g;
	c->b=xclrtab[rec.value].b;
	c->a=255;
	return 0;
}

/*
 * Parse XPM color value and place it in 'c'
 */
static int parse_color(struct color *c, int ctype, char *cval)
{
	int res;
	
	if(!strcasecmp(cval,"none")){
		c->r=c->g=c->b=c->a=0;
		return 0;
	}
	
	/* TBD: support for HSV */
	if(cval[0]=='%') return IMG_EUNSUP;
	c->a=255;
	
	if(ctype=='c' && cval[0]=='#'){
		char r[5], g[5], b[5];
		int len, csize;

		cval++;
		len=strlen(cval);
		
		if(len==6){
			csize=2;
		}else if(len==12){
			csize=4;
		}else{
			dbg_trace("illegal color value for %c: %s",ctype,cval);
			return IMG_EFILE;
		}
		strncpy(r,cval,csize); r[csize]='\0';
		strncpy(g,&cval[csize],csize); g[csize]='\0';
		strncpy(b,&cval[csize*2],csize); b[csize]='\0';

		if(csize==2){
			c->r=strtol(r,NULL,16);
			c->g=strtol(g,NULL,16);
			c->b=strtol(b,NULL,16);
		}else{
			c->r=(strtol(r,NULL,16) >> 8);
			c->g=(strtol(g,NULL,16) >> 8);
			c->b=(strtol(b,NULL,16) >> 8);
		}

	}else if(ctype=='c'){
		res=get_named_rgb(c,cval);
		if(res) return res;
	}else{
		dbg_trace("illegal color specifier: %c: %s\n",ctype,cval);
		return IMG_EFILE;
	}
	return 0;
}

/*
 * Try to parse the color table
 */
int read_color_table(FILE *file, struct color *table, int ncolors, int bpc)
{
	char buf[XPM_MAX_LINE+1];
	const char token_sep[]=" \t";

	while(ncolors--){
		char *bufptr;
		char *token;
		char ctype=0;
		int i, c;
		
		/* get next string literal into the buffer and remove the quotes */
		while(EOF!=(c=fgetc(file))){
			if(c=='\"') break;
		}
		if(c==EOF || !fgets(buf,XPM_MAX_LINE,file)){
			if(ferror(file)) return IMG_EIO;
			else if(feof(file)) return IMG_EFILE;
		}
		token=strrchr(buf,'\"');
		if(!token) return IMG_EFILE;
		*token='\0';
		
		/* store the symbol, advance the buffer... */
		strncpy(table[ncolors].sym,buf,bpc);
		bufptr=&buf[bpc];
		
		/* ...and look for 'c' specifier and its value */
		token=strtok(bufptr,token_sep);
		for(i=0; i<5 && token; i++){
			if(token[0]=='c'){
				char cval[XPM_MAX_LINE+1];
				ctype=token[0];

				cval[0]='\0';
				while((token=strtok(NULL,token_sep))){
					if(strlen(token)>1)
						strcat(cval,token);
					else
						break;
				}

				if(!ctype || !strlen(cval)) return IMG_EFILE;
				if(!parse_color(&table[ncolors],ctype,cval)) break;
			}
			token=strtok(NULL,token_sep);
		}
	}
	return 0;
}

/*
 * Initialize color lookup table
 */
int init_clut(struct loader_data *ld, struct color *table,
	int ncolors, int bpc)
{
	int i;
	int res;
	struct sym_rec rec;

	/* build a hash table for 2+ bpc pixmaps, or a 256 byte array
	 * for 1 bpc that can be indexed directly */
	if(bpc>1){
		hashtbl_t *clv;
		
		clv=ht_alloc(ncolors,0,sizeof(struct sym_rec),
			(ht_hash_ft)sym_hash,(ht_cmp_ft)sym_compare);
		if(!clv) return IMG_ENOMEM;

		for(i=0; i<ncolors; i++){
			rec.sym=table[i].sym;
			rec.value=table[i].r|((uint32_t)table[i].g<<8)|
				((uint32_t)table[i].b<<16)|((uint32_t)table[i].a<<24);;
			res=ht_insert(clv,&rec);
			if(res==ENOMEM){
				ht_free(clv);
				return IMG_ENOMEM;
			}
			#ifdef DEBUG
			else if(res=EEXIST){
				dbg_trace("%d: duplicate entry for %s\n",i,table[i].sym);
			}
			#endif
		}
		ld->hci=clv;
		ld->ci=NULL;
	}else{ 
		uint8_t *indices;
		indices=malloc(256);
		if(!indices) return IMG_ENOMEM;
		
		for(i=0; i<ncolors; i++){
			indices[table[i].sym[0]]=i;
		}
		ld->hci=NULL;
		ld->ci=indices;
	}
	return 0;
}

/*
 * Convert pixmap data string to an RGBA pixel
 */
static uint32_t lookup_pixel(struct loader_data *ld, const char *sym)
{
	uint32_t pixel=0;
	
	if(ld->bpc==1){
		struct color *c;
		c=&ld->clut[ld->ci[sym[0]]];
		pixel=c->r|((uint32_t)c->g<<8)|
			((uint32_t)c->b<<16)|((uint32_t)c->a<<24);
	}else{
		struct sym_rec rec={(char*)sym,0};
		if(!ht_lookup(ld->hci,&rec,&rec)){
			pixel=rec.value;
		}
		#ifdef DEBUG
		else{
			dbg_trace("no entry for sym: %s\n",rec.sym);
		}
		#endif
	}
	return pixel;
}

static int read_scanlines(struct img_file *img,
	img_scanline_cbt cb, void *cdata)
{
	struct loader_data *ld=(struct loader_data*)img->loader_data;
	unsigned int iscl;
	char sym[XPM_MAX_BPC+1];
	uint32_t *buf;

	buf=calloc(ld->width,4);
	if(!buf) return IMG_ENOMEM;
	
	fsetpos(ld->file,&ld->data_pos);
	
	for(iscl=0; iscl<ld->height; iscl++){
		int c;
		int x=0;
		
		while(EOF!=(c=fgetc(ld->file)))
			if(c=='\"') break;

		if(c==EOF){
			free(buf);
			return (ferror(ld->file))?IMG_EIO:IMG_EFILE;
		}
		
		for(x=0; x<ld->width; x++){
			size_t read;
			read=fread(sym,1,ld->bpc,ld->file);
			sym[ld->bpc]='\0';
			if(!read){
				free(buf);
				return (ferror(ld->file))?IMG_EIO:IMG_EFILE;
			}
			buf[x]=lookup_pixel(ld,sym);
		}
		while(EOF!=(c=fgetc(ld->file)))
			if(c=='\n') break;
		
		if((*cb)(iscl,(void*)buf,cdata)==IMG_READ_CANCEL) break;
	}
	free(buf);
	return 0;
}

int img_open_xpm(const char *file_name, struct img_file *img, int flags)
{
	FILE *file;
	struct stat st;
	char buf[XPM_MAX_LINE+1];
	int width=0;
	int height=0;
	int ncolors=0;
	struct color *clr_table;
	int bpc=0;
	int res=0;
	struct loader_data *ld;
	
	memset(img,0,sizeof(struct img_file));
	
	if((stat(file_name,&st)== -1) ||
		!(file=fopen(file_name,"r"))) return IMG_EFILE;
	
	if(!fgets(buf,XPM_MAX_LINE,file) ||
		!strstr(buf,"/* XPM */")){
			fclose(file);
			return IMG_EFILE;
	}

	while(fgets(buf,XPM_MAX_LINE,file)){
		if(sscanf(buf,"\"%d %d %d %d\"",&width,&height,&ncolors,&bpc) ||
			sscanf(buf,"\"%d %d %d %d %*d %*d\"",&width,&height,&ncolors,&bpc))
				break;
	}
		
	if(!width || !height || !ncolors || !bpc || bpc>XPM_MAX_BPC){
		res=(ferror(file))?IMG_EIO:IMG_EFILE;
		fclose(file);
		return res;
	}
	
	ld=calloc(1,sizeof(struct loader_data));
	if(!ld){
		fclose(file);
		return IMG_ENOMEM;
	}
	clr_table=calloc(ncolors,sizeof(struct color));
	if(!clr_table){
		free(ld);
		fclose(file);
		return IMG_ENOMEM;
	}

	if((res=read_color_table(file,clr_table,ncolors,bpc))||
		(res=init_clut(ld,clr_table,ncolors,bpc))){
		free(ld);
		free(clr_table);
		fclose(file);
		return res;
	}
	fgetpos(file,&ld->data_pos);
	ld->file=file;
	ld->clut=clr_table;
	ld->bpc=bpc;
	ld->width=width;
	ld->height=height;
	
	img->format=IMG_DIRECT;
	img->width=width;
	img->height=height;
	img->bpp=32;
	img->orig_bpp=32;
	if(!is_big_endian()){
		img->red_mask=0x000000FF;
		img->green_mask=0x0000FF00;
		img->blue_mask=0x00FF0000;
		img->alpha_mask=0xFF000000;
	}else{
		img->red_mask=0xFF000000;
		img->green_mask=0x00FF0000;
		img->blue_mask=0x0000FF00;
		img->alpha_mask=0x000000FF;
	}
	img->loader_data=(void*)ld;
	img->read_scanlines_fnc=&read_scanlines;
	img->close_fnc=&close;
	img->cr_time=st.st_mtime;
	
	return 0;
}

static void close(struct img_file *img)
{
	struct loader_data *ld=(struct loader_data*)img->loader_data;
	fclose(ld->file);
	free(ld->clut);
	if(ld->bpc>1)
		ht_free(ld->hci);
	else
		free(ld->ci);
	free(ld);
}

/*
 * sym_rec compare and hash functions for XPM symbols
 */
static int sym_compare(struct sym_rec *a, struct sym_rec *b)
{
	return strcmp(a->sym,b->sym);
}

static hashkey_t sym_hash(struct sym_rec *rec)
{
	hashkey_t h=31;
	unsigned int i=0;
	for(i=0; rec->sym[i]; i++)
		h=37*h+((unsigned long)rec->sym[i]<<(8*i));
	return h;
}

/*
 * sym_rec compare and hash function for X color names 
 */
static int sym_compare_nocase(struct sym_rec *a, struct sym_rec *b)
{
	return strcasecmp(a->sym,b->sym);
}

static hashkey_t sym_hash_nocase(struct sym_rec *rec)
{
	return hash_string_nocase(rec->sym);
}
