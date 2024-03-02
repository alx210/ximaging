/*
 * Copyright (C) 2012-2024 alx@fastestcode.org
 * This software is distributed under the terms of the MIT license.
 * See the included LICENSE file for further information.
 */
 
/*
 * Image file identification and loading helper routines.
 */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include "common.h"
#include "strings.h"
#include "imgfile.h"
#include "hashtbl.h"
#include "ldrproto.h"
#include "debug.h"

/* 
 * Image type definitions:
 * "Description","extension ...",loader function (see ldrproto.h)
 */
static struct img_format_info file_formats[]={
	{"PC-Paintbrush","pcx",LDRPROC(pcx)},
	{"Truevision TGA","tga tpic",LDRPROC(tga)},
	{"SGI Image","rgb rgba bw int inta sgi",LDRPROC(sgi)},
	{"Sun Raster Image","ras sun",LDRPROC(ras)},
	{"X Bitmap","xbm bm",LDRPROC(xbm)},
	{"X Pixmap","xpm pm",LDRPROC(xpm)},
	{"MS-Windows Bitmap","bmp",LDRPROC(bmp)},
	#ifdef ENABLE_JPEG
	{"JPEG Image","jpg jpeg jpe jif jfif jfi",LDRPROC(jpeg)},
	#endif
	#ifdef ENABLE_PNG
	{"Portable Network Graphics","png",LDRPROC(png)},
	#endif
	#ifdef ENABLE_TIFF
	{"Tagged Image File","tif tiff",LDRPROC(tiff)},
	#endif
};
static const int num_file_formats=sizeof(file_formats)/
	sizeof(struct img_format_info);

struct ext_hash_rec {
	char *ext;
	int index;
};

/* Local prototypes */
static int ext_hash_rec_compare(const struct ext_hash_rec*,
	const struct ext_hash_rec*);
static hashkey_t ext_hash_fnc(struct ext_hash_rec*);

/*
 * Open image by extension.
 */
int img_open(const char *file_name, struct img_file *img, int flags)
{
	int res;
	struct img_format_info *info;
	
	info=img_get_format_info(file_name);
	if(!info) return IMG_EUNSUP;
	res=info->open_proc(file_name,img,0);
	img->type_str=info->desc;
	return res;
}

/* 
 * Get image format info. Returns NULL if unable to identify the image.
 */
struct img_format_info* const img_get_format_info(const char *fname)
{
	char *token;
	static hashtbl_t *ext_table=NULL;
	struct ext_hash_rec rec;

	if(!ext_table){
		int i, res;
		const char sep[]=" ";
		ext_table=ht_alloc(num_file_formats*4,0,sizeof(struct ext_hash_rec),
			(ht_hash_ft)ext_hash_fnc,(ht_cmp_ft)ext_hash_rec_compare);
		if(!ext_table) fatal_error(ENOMEM,NULL,NULL);
		for(i=0; i<num_file_formats; i++){
			char *last=NULL;
			char *token=NULL;
			char *cur=NULL;
			
			cur=strdup(file_formats[i].ext);
			token=strtok_r(cur,sep,&last);
			dbg_assert(token);
			do {
				rec.ext=strdup(token);
				rec.index=i;
				res=ht_insert(ext_table,&rec);
				if(res==ENOMEM) fatal_error(ENOMEM,NULL,NULL);
				token=strtok_r(NULL,sep,&last);
			}while(token);
			free(cur);
		}
	}
		
	token=strrchr(fname,'.');
	if(!token || token[1]=='\0') return NULL;
	token++;
	rec.ext=token;
	if(ht_lookup(ext_table,&rec,&rec)) return NULL;
	return &file_formats[rec.index];
}

/* Retrieve the list of supported image formats */
int img_get_formats(struct img_format_info const **ptr)
{
	*ptr=file_formats;
	return num_file_formats;
}

/* Retrieve descriptive text for an IMG error code */
char * const img_strerror(int img_errno)
{
	char *str;
	switch(img_errno){
		case IMG_ENOMEM:
		str=nlstr(APP_MSGSET,SID_ENORES,
			"Not enough resources available for this task.");
		break;
		case IMG_EUNSUP:
		str=nlstr(APP_MSGSET,SID_IMGUNSUP,
			"Unsupported image file format.");
		break;
		case IMG_EFILE:
		str=nlstr(APP_MSGSET,SID_IMGINVAL,
			"Invalid or damaged image file.");
		break;
		case IMG_EPERM:
		str=nlstr(APP_MSGSET,SID_EFPERM,
			"No permission to access the specified file.");
		break;
		default:
		str=nlstr(APP_MSGSET,SID_IMGERROR,
			"An error occured while reading the file.");
		break;
	}
	return str;
}

static int ext_hash_rec_compare(const struct ext_hash_rec *a,
	const struct ext_hash_rec *b)
{
	return strcasecmp(a->ext,b->ext);
}

static hashkey_t ext_hash_fnc(struct ext_hash_rec *rec)
{
	return hash_string_nocase(rec->ext);
}
