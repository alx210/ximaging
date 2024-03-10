/*
 * Copyright (C) 2012-2024 alx@fastestcode.org
 * This software is distributed under the terms of the MIT license.
 * See the included LICENSE file for further information.
 */

/*
 * Implements dynamic hash table.
 */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include "hashtbl.h"
#include "debug.h"

/* Local prototypes */
static long modexp(long b, long e, long m);
static int test_prime(long n);
static unsigned long qrand(unsigned long m);
static int expand_table(hashtbl_t *tbl);
static int insert_item(hashtbl_t *tbl, const void *entry);
static int find_index(const hashtbl_t *tbl,	const void *entry, long *res);

/* Fullness threshold in percent at which storage should be expanded */
#define FULL_THRESHOLD	80

/* Item state values */
#define ST_EMPTY	0x00
#define ST_SET		0x03
#define ST_LOCKED	0x01

/* Prime test accuracy */
#define PRIM_TESTS	30

/* b ^ e (mod m) */
static long modexp(long b, long e, long m)
{
	long c=1;
	while(e){
		if(e & 1) c=(c*b)%m;
		e>>=1;
		b=(b*b)%m;
	}
	return c;
}

/* Returns 1 if 'p' appears to be a prime number. */
static int test_prime(long p)
{
	long a, i, r;
	
	if(p<2) return 0;

	/* do a complete test run for p < PRIM_TESTS^2,
	 * or a limited one for larger numbers */
	if(p < (PRIM_TESTS*PRIM_TESTS)){
		if(modexp(2,p,p)==2%p){
			for(i=2; i<(p-1); i*=i){
				if(!(p % i)) return 0;
			}
		}else return 0;

	}else{
		for(i=0; i<PRIM_TESTS; i++){
			a=qrand(p-1);
			r=modexp(a,p-1,p);
			if(r!=1%p) return 0;
		}
	}
	return 1;
}

/* 
 * A goofy pseudo random number generator. m > x > 0.
 */
static unsigned long qrand(unsigned long m)
{
	static unsigned long rnd=3;
	unsigned long r;
	do {
		rnd=(1664525L*rnd+1013904223L);
	} while(!(r=rnd%m));
	return r;
}

/* 
 * Expand storage space; this requires duplicating and rehashing
 * the whole table. Returns 0 on success.
 */
static int expand_table(hashtbl_t *tbl)
{
	long i;
	int err;
	long count;
	hashtbl_t *tmp;

	/* check if the table is worth expanding, if there are too many
	 * bounce slots then compacting may suffice */
	for(count=0, i=0; i<tbl->item_count; i++)
		if(tbl->stat_vec[i]==ST_SET) count++;
	if(((float)count/tbl->tbl_size)*100 < FULL_THRESHOLD){
		count=tbl->tbl_size;
	}else{
		count=tbl->tbl_size+tbl->grow_by;
		while(!test_prime(count)) count++;
	}
	tmp=ht_alloc(count,tbl->grow_by,tbl->entry_size,
		tbl->hash_fnc,tbl->cmp_fnc);
	if(!tmp) return ENOMEM;
	
	for(i=0; i<tbl->tbl_size; i++){
		if(tbl->stat_vec[i]!=ST_SET) continue;
		err=insert_item(tmp,&tbl->data_vec[tbl->entry_size*i]);
		if(err) break;
	}
	if(err){
		ht_free(tmp);
		return err;
	}
	/* free tbl's storage and swap them with tmp's */
	free(tbl->stat_vec);
	free(tbl->data_vec);
	memcpy(tbl,tmp,sizeof(hashtbl_t));
	free(tmp);
	return 0;
}

/*
 * Insert an item into the table
 */
static int insert_item(hashtbl_t *tbl, const void *entry)
{
	hashkey_t key;
	long index;
	long rehash=0;

	key=tbl->hash_fnc(entry);
	index=key % tbl->tbl_size;
	while(tbl->stat_vec[index]!=ST_EMPTY){
		if(!tbl->cmp_fnc(&tbl->data_vec[tbl->entry_size*index],entry)){
			return EEXIST;
		}
		rehash++;
		index=(key+rehash) % tbl->tbl_size;
	}
	memcpy(&tbl->data_vec[tbl->entry_size*index],entry,tbl->entry_size);
	tbl->stat_vec[index]=ST_SET;
	tbl->item_count++;

	return 0;
}

/* Find entry index. */
static int find_index(const hashtbl_t *tbl,	const void *entry, long *index)
{
	hashkey_t key;
	long rehash=0;
	int state;
	
	key=tbl->hash_fnc(entry);
	do{
		*index=(key+rehash) % tbl->tbl_size;
		state=tbl->stat_vec[*index];
		if(state==ST_EMPTY)	return ENOENT;
		rehash++;
	}while(tbl->cmp_fnc(&tbl->data_vec[tbl->entry_size*(*index)],entry) ||
		state==ST_LOCKED);

	return 0;
}

/* 
 * Create a hash table and initialize internal storage.
 * Returns a valid pointer on success, NULL otherwise.
 */
hashtbl_t* ht_alloc(size_t tbl_size, size_t grow_by,
	size_t esize, ht_hash_ft hash_fnc, ht_cmp_ft cmp_fnc)
{
	hashtbl_t *tbl;
	
	if(!grow_by) grow_by=tbl_size;
	/* adjust size according to the fullness threshold */
	tbl_size+=((float)(120-(FULL_THRESHOLD))/100)*tbl_size;
	while(!test_prime(tbl_size)) tbl_size++;
	dbg_assert(tbl_size>0);

	tbl=malloc(sizeof(hashtbl_t));
	if(!tbl) return NULL;
	tbl->stat_vec=calloc(tbl_size,sizeof(char));
	tbl->data_vec=calloc(tbl_size,esize);
	if(!tbl->stat_vec || !tbl->data_vec){
		if(tbl->stat_vec) free(tbl->stat_vec);
		if(tbl->data_vec) free(tbl->data_vec);
		free(tbl);
		return NULL;
	}
	tbl->entry_size=esize;
	tbl->tbl_size=tbl_size;
	tbl->item_count=0;
	tbl->cmp_fnc=cmp_fnc;
	tbl->hash_fnc=hash_fnc;
	return tbl;
}

/* Delete all entries. Storage size remains unchanged. */
void ht_empty(hashtbl_t *tbl)
{
	memset(tbl->stat_vec,0,tbl->tbl_size);
	tbl->item_count=0;
}

/* Release internal storage and container memory */
void ht_free(hashtbl_t *tbl)
{
	free(tbl->stat_vec);
	free(tbl->data_vec);
	free(tbl);
}

/* 
 * Insert a new item. Returns 0 on success.
 */
int ht_insert(hashtbl_t *tbl, const void *entry)
{
	if(((float)tbl->item_count/tbl->tbl_size)*100 > FULL_THRESHOLD){
		int res;
		res=expand_table(tbl);
		if(res) return res;
	}
	return insert_item(tbl,entry);
}

/* Lookup an item and store the result in 'res'*/
int ht_lookup(const hashtbl_t *tbl, const void *entry, void *res)
{
	long index;
	if(find_index(tbl,entry,&index)) return ENOENT;
	if(res)	memcpy(res,&tbl->data_vec[tbl->entry_size*index],tbl->entry_size);
	return 0;
}

/* Lookup an item and store the pointer to it in 'res' */
int ht_lookup_ptr(const hashtbl_t *tbl, const void *entry, void **res)
{
	long index;
	if(find_index(tbl,entry,&index)) return ENOENT;
	if(res)	*res=&tbl->data_vec[tbl->entry_size*index];
	return 0;
}

/* Remove a key */
int ht_delete(hashtbl_t *tbl, const void *entry)
{
	long index;
	if(find_index(tbl,entry,&index)) return ENOENT;
	tbl->stat_vec[index]=ST_LOCKED;
	return 0;
}

/* Replace an entry. */
int ht_replace(hashtbl_t *tbl, const void *entry)
{
	long index;
	if(find_index(tbl,entry,&index)) return ENOENT;
	memcpy(&tbl->data_vec[tbl->entry_size*index],entry,tbl->entry_size);
	return 0;
}

long ht_count(hashtbl_t *tbl)
{
	return tbl->item_count;
}

/* Hash a zero terminated string */
hashkey_t hash_string(const char *str)
{
	unsigned long h=0;
	unsigned int i=0;
	
	for(i=0; str[i]; i++)
		h=((h<<5)^((h&0xf8L)>>27))^((unsigned)str[i]);

	return h;
}

/* Hash a zero terminated string disregadring case */
hashkey_t hash_string_nocase(const char *str)
{
	unsigned long h=0;
	unsigned int i=0;
	
	for(i=0; str[i]; i++)
		h=((h<<5)^((h&0xf8L)>>27))^((unsigned)tolower(str[i]));

	return h;
}
