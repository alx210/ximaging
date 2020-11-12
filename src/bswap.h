/*
 * Copyright (C) 2012-2017 alx@fastestcode.org
 * This software is distributed under the terms of the MIT license.
 * See the included LICENSE file for further information.
 */
 
/*
 * Endianness swapping routines
 */

#ifndef BSWAP_H
#define BSWAP_H

#include <inttypes.h>

static inline uint16_t bswap_word(uint16_t v)
{
	uint16_t r;
	
	r=((v<<8)&0xFF00)|((v>>8)&0x00FF);
	return r;
}

static inline uint32_t bswap_dword(uint32_t v)
{
	uint32_t r;
	
	r=((v<<24)&0xFF000000)|((v<<8)&0x00FF0000)|
		((v>>24)&0x000000FF)|((v>>8)&0x0000FF00);
	return r;
}

static inline uint64_t bswap_qword(uint64_t v)
{
	uint64_t r;
	
	r=((uint64_t)bswap_dword(v)<<32) & 0xFFFFFFFF00000000;
	r|=((uint64_t)bswap_dword((v>>32))) & 0x00000000FFFFFFFF;
		
	return r;
}

static inline double bswap_double(double v)
{
	union { uint64_t i; double d; } uv;
	uv.d=v;
	uv.i=bswap_qword(uv.i);
	return uv.d;
}

static inline float bswap_float(float v)
{
	union { uint32_t i; float f; } uv;
	uv.f=v;
	uv.i=bswap_dword(uv.i);
	return uv.f;
}

static inline int is_big_endian(void)
{
	uint16_t i=1;
	char *c=(char*)&i;
	return (int)c[1];
}

#endif /* BSWAP_H */
