/*
 * Copyright (C) 2012-2025 alx@fastestcode.org
 * This software is distributed under the terms of the MIT license.
 * See the included LICENSE file for further information.
 */

/*
 * Hash table type definitions and prototypes.
 */

#ifndef HASHTBL_H
#define HASHTBL_H

typedef unsigned long hashkey_t;


/* Entry comparison function type */
typedef int (*ht_cmp_ft)(const void*,const void*);

/* Entry hashing function type */
typedef hashkey_t (*ht_hash_ft)(const void*);

/* Hash table container */
typedef struct {
	size_t tbl_size;	/* size of the hash table */
	size_t grow_by;
	long item_count;	/* number of items in the table */
	size_t entry_size;	/* size of a single item in data_vec */
	char *stat_vec;	/* slot state vector */
	char *data_vec;		/* stores the entries */
	ht_cmp_ft cmp_fnc;
	ht_hash_ft hash_fnc;
} hashtbl_t;

/*
 * Allocate a hash table and initial storage.
 * Returns a valid pointer, or NULL on failure.
 */
hashtbl_t* ht_alloc(size_t vec_size, size_t grow_by,
	size_t entry_size, ht_hash_ft hash_fnc, ht_cmp_ft cmp_fnc);

/* 
 * Delete all entries; storage size remains unchanged.
 */
void ht_empty(hashtbl_t *tbl);

/*
 * Free internal vectors and the container.
 */
void ht_free(hashtbl_t *hv);

/*
 * Insert a new entry. The table may grow if necessary.
 * Returns 0 on success.
 */
int ht_insert(hashtbl_t *tbl, const void *entry);

/*
 * Lookup an entry and store it in res. 'ref' and 'res' may point to the same
 * memory block. 'res' also may be NULL if entry data is not desired.
 * Returns 0 on success, ENOENT otherwise.
 */
int ht_lookup(const hashtbl_t *tbl, const void *ref, void *res);

/*
 * Replace an entry. Returns 0 on success.
 */
int ht_replace(hashtbl_t *tbl, const void *entry);

/*
 * Delete an entry. Returns 0 on success.
 */
int ht_delete(hashtbl_t *tbl, const void *entry);

/*
 * Return number of entries in tbl
 */
long ht_count(hashtbl_t *tbl);

/* Hash a zero terminated string */
hashkey_t hash_string(const char *str);

/* Hash a zero terminated string disregadring case */
hashkey_t hash_string_nocase(const char *str);

#endif /* HASHTBL_H */
