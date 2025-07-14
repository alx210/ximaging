/*
 * Copyright (C) 2012-2025 alx@fastestcode.org
 * This software is distributed under the terms of the MIT license.
 * See the included LICENSE file for further information.
 */
 
/*
 * Loader prototypes
 */

#ifndef LDRPROTO_H
#define LDRPROTO_H

#include "imgfile.h"

#define LDRPROC(type) img_open_ ## type
#define PROTODEF(type)int img_open_ ## type(const char*,struct img_file*,int);

PROTODEF(tga)
PROTODEF(pcx)
PROTODEF(sgi)
PROTODEF(ras)
PROTODEF(xpm)
PROTODEF(xbm)
PROTODEF(pam)
PROTODEF(bmp)
#ifdef ENABLE_JPEG
PROTODEF(jpeg)
#endif
#ifdef ENABLE_PNG
PROTODEF(png)
#endif
#ifdef ENABLE_TIFF
PROTODEF(tiff)
#endif

#undef PROTODEF

/* in netpbm.c */
int img_filter_pnm(const char *cmd_spec,
	const char *file_name, struct img_file *img, int flags);

#endif /* LDRPROTO_H */

