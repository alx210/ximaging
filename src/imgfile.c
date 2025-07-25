/*
 * Copyright (C) 2012-2025 alx@fastestcode.org
 * This software is distributed under the terms of the MIT license.
 * See the included LICENSE file for further information.
 */
 
/*
 * Image file identification and loading helper routines.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include "common.h"
#include "strings.h"
#include "imgfile.h"
#include "hashtbl.h"
#include "ldrproto.h"
#include "extres.h"
#include "debug.h"

static char* guess_suffix(const char *file_name);

/* Static image file formats */
struct img_format_info {
	char *desc;		/* description */
	char *suffixes;	/* space separated list of file suffixes */
	img_open_proc_t open_fnc;
};

static struct img_format_info file_formats[]={
	{ "PC-Paintbrush", "pcx", LDRPROC(pcx) },
	{ "Truevision TGA", "tga tpic", LDRPROC(tga) },
	{ "SGI Image", "rgb rgba bw int inta sgi", LDRPROC(sgi) },
	{ "Sun Raster Image", "ras sun", LDRPROC(ras) },
	{ "X Bitmap", "xbm bm", LDRPROC(xbm) },
	{ "X Pixmap", "xpm pm", LDRPROC(xpm) },
	{ "Netpbm Image", "pbm pgm ppm pam pnm", LDRPROC(pam) },
	{ "MS-Windows Bitmap", "bmp", LDRPROC(bmp) },
	#ifdef ENABLE_JPEG
	{ "JPEG Image", "jpg jpeg jpe jif jfif jfi", LDRPROC(jpeg) },
	#endif
	#ifdef ENABLE_PNG
	{ "Portable Network Graphics", "png", LDRPROC(png) },
	#endif
	#ifdef ENABLE_TIFF
	{ "Tagged Image File", "tif tiff", LDRPROC(tiff) },
	#endif
};
static const int num_file_formats=sizeof(file_formats)/
	sizeof(struct img_format_info);

/* Dynamic file type table, one record per suffix */
static hashtbl_t *type_table = NULL;

/* Local prototypes */
static int img_type_rec_compare(const struct img_type_rec*,
	const struct img_type_rec*);
static hashkey_t type_hash_fnc(struct img_type_rec*);
static void build_type_table(void);

/*
 * Builds global file type table, if not done yet
 * Calls fatal error handler on memory allocation failure
 */
static void build_type_table(void)
{
	int i, res;
	const char sep[]=" ";
	unsigned int nfilters = 0;
	struct filter_rec *filters;

	if(type_table) return;
	
	nfilters = fetch_filters(&filters);

	type_table = ht_alloc((num_file_formats + nfilters) * 4, 0,
		sizeof(struct img_type_rec), (ht_hash_ft)type_hash_fnc,
		(ht_cmp_ft)img_type_rec_compare);
	
	if(!type_table) fatal_error(ENOMEM, NULL, NULL);
	
	for(i = 0; i < num_file_formats; i++){
		struct img_type_rec rec = { 0 };
		char *last = NULL;
		char *token = NULL;
		char *cur = NULL;

		cur = strdup(file_formats[i].suffixes);
		token = strtok_r(cur, sep, &last);
		
		dbg_assert(token);
		
		do {
			rec.suffix = strdup(token);
			rec.desc = file_formats[i].desc;
			rec.open_fnc = file_formats[i].open_fnc;
			res = ht_insert(type_table, &rec);
			if(res == ENOMEM) fatal_error(ENOMEM, NULL, NULL);
			token = strtok_r(NULL, sep, &last);
		} while(token);
		free(cur);
	}
	
	for(i = 0; i < nfilters; i++) {
		struct img_type_rec rec = { 0 };
		char *last = NULL;
		char *token = NULL;
		char *cur = NULL;
		
		if(!filters[i].suffixes || !filters[i].command)  {
			warning_msg("Insufficient data specified for filter \'%s\'\n",
				filters[i].name);
			continue;
		}

		cur = strdup(filters[i].suffixes);
		token = strtok_r(cur, sep, &last);
		
		dbg_assert(token);
		
		do {
			rec.suffix = strdup(token);
			rec.desc = filters[i].description;
			rec.filter_cmd = filters[i].command;
			rec.open_fnc = NULL;
			
			if( (res = ht_replace(type_table, &rec)) == ENOENT) {
				res = ht_insert(type_table, &rec);
			}

			if(res == ENOMEM) fatal_error(ENOMEM, NULL, NULL);
			token = strtok_r(NULL, sep, &last);
		} while(token);
		free(cur);
	}
}

/* Open the image file (type_suffix may be NULL) */
int img_open(const char *file_name, const char *type_suffix,
	struct img_file *img, int flags)
{
	int res;
	struct img_type_rec type;
	
	if(img_ident(file_name, type_suffix, &type))
		return IMG_EUNSUP;
	
	if(type.open_fnc) {
		res = type.open_fnc(file_name, img, 0);
		if(!img->type_str) img->type_str = type.desc;
	}  else if(type.filter_cmd) {
		res = img_filter_pnm(type.filter_cmd, file_name, img, 0);
		if(type.desc) img->type_str = type.desc;
	} else {
		return IMG_EUNSUP;
	}
	
	
	dbg_assert(img->close_fnc);
	
	return res;
}

/* 
 * Retrieves file type/handler info for suffix or fname (in that order of
 * preference). Returns zero if a handler exists. If no type info desired,
 * rec may be NULL.
 */
int img_ident(const char *fname, const char *suffix,
	struct img_type_rec *info_ret)
{
	struct img_type_rec rec;
	int res;

	build_type_table();
	
	if(suffix) {
		rec.suffix = (char*)suffix;
	} else if(fname) {
		char *token;
	
		token = strrchr(fname,'.');
		if(token && token[1] != '\0') {
			token++;
			rec.suffix = token;
		} else {
			rec.suffix = guess_suffix(fname);
			if(!rec.suffix) return IMG_EUNSUP;
		}
	}
	res = ht_lookup(type_table, &rec, &rec);
	if(!res && info_ret) *info_ret = rec;
	
	return (res == 0) ? 0 : IMG_EUNSUP;
}

/* Retrieve the list of supported image formats */
int img_get_formats(struct img_format_info const **ptr)
{
	*ptr = file_formats;
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
		case IMG_EFILTER:
		str = nlstr(APP_MSGSET, SID_EFILTER,
			"Failed to execute filter.");
		break;
		case IMG_EDATA:
		str = nlstr(APP_MSGSET, SID_EDATA,
			"Insufficient data.");
		break;
		default:
		str=nlstr(APP_MSGSET,SID_IMGERROR,
			"An error occured while reading the file.");
		break;
	}
	return str;
}

static int img_type_rec_compare(const struct img_type_rec *a,
	const struct img_type_rec *b)
{
	return strcasecmp(a->suffix, b->suffix);
}

static hashkey_t type_hash_fnc(struct img_type_rec *rec)
{
	return hash_string_nocase(rec->suffix);
}

/*
 * Tries to identify a file by reading its header.
 * Returns a corresponding suffix if match is found.
 */
static char* guess_suffix(const char *file_name)
{
	int i;
	FILE * file;
	size_t rc;
	char read_buf[12];
	static Boolean init = True;
	const char *fn_tail;

	struct magic_rec {
		char value[12];
		char *suffix;
		size_t len;
	};
	
	static struct magic_rec magic[] = {
		{ "", "xbm" },
		{ "\xff\xd8\xff\xe0", "jpg"},
		{ ".PNG", "png" },
		{ "\x59\xa6\x6a\x95", "ras" },
		{ "\x4d\x4d\x00\x2a", "tif" },
		{ "\x49\x49\x2a\x00", "tif" },
		{ "\x0a\x05", "pcx" },
		{ "\x01\xda", "rgb" },
		{ "/* XPM */", "xpm" },
		{ "P4", "pnm" },
		{ "P5", "pnm" },
		{ "P6", "pnm" },
		{ "P7", "pam" },
		{ "GIF8", "gif" },
		{ "BM", "bmp" }
	};
	
	size_t nmagic = (sizeof(magic) / sizeof(struct magic_rec));

	if(init) {
		init = False;
		for( i = 0; i < nmagic; i++)
			magic[i].len = strlen(magic[i].value);
	}

	fn_tail = strrchr(file_name, '/');
	if(fn_tail && fn_tail[1] != '\0')
		fn_tail++;
	else
		fn_tail = file_name;
	
	snprintf(magic[0].value, 12, "#define %s", fn_tail);
	magic[0].len = strlen(magic[0].value);

	file = fopen(file_name, "r");
	if(!file) return NULL;

	rc = fread(read_buf, 1, 12, file);
	fclose(file);
	if(!rc) return NULL;

	read_buf[rc] = '\0';
	
	for(i = 0; i < nmagic; i++) {
		if(!strncmp(magic[i].value, read_buf, magic[i].len))
			return magic[i].suffix;
	}
	
	return NULL;
}
