/*
 * Copyright (C) 2012-2024 alx@fastestcode.org
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

/* Iterator */
enum ht_it_dir { HT_IT_FORWARD, HT_IT_BACKWARD };

typedef struct {
	hashtbl_t *tbl;
	long index;
	enum ht_it_dir dir;
} ht_iterator_t;

#ifdef __cplusplus
extern "C"{
#endif

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
 * Lookup an item and store the pointer to it in 'res'.
 * Returns 0 on success, ENOENT otherwise.
 */
int ht_lookup_ptr(const hashtbl_t *tbl, const void *entry, void **res);

/*
 * Replace an entry. Returns 0 on success.
 */
int ht_set(hashtbl_t *tbl, const void *entry);

/*
 * Delete an entry. Returns 0 on success.
 */
int ht_delete(hashtbl_t *tbl, const void *entry);

/*
 * Return number of entries in tbl
 */
long ht_count(hashtbl_t *tbl);

/* 
 * Initialize an iterator. It is guaranteed to remain valid as long
 * as table contents aren't changed.
 */
void ht_init_iterator(const hashtbl_t*,enum ht_it_dir, ht_iterator_t*);

/*
 * Returns a volatile pointer to the next hash table record or
 * NULL if either end of the table is reached.
 */
void* ht_iterate(ht_iterator_t*);

/* Hash a zero terminated string */
hashkey_t hash_string(const char *str);

/* Hash a zero terminated string disregadring case */
hashkey_t hash_string_nocase(const char *str);

#ifdef __cplusplus
}
#endif

#endif /* HASHTBL_H */
