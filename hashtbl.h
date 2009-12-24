/* Copyright (c) 2009 <Andrew McDermott>
 *
 * Source can be cloned from:
 *
 *     git://github.com/aim-stuff/hashtbl.git
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/**
 * A hash table: efficiently maps keys to values.
 *
 * SYNOPSIS
 *
 * 1. A hash table is created with hashtbl_new().
 * 2. To insert an entry use hashtbl_insert().
 * 3. To lookup a key use hashtbl_lookup().
 * 4. To remove a key use hashtbl_remove().
 * 5. To apply a function over all entries use hashtbl_apply().
 * 5. To clear all keys use hashtbl_clear().
 * 6. To delete a hash table instance use hashtbl_delete().
 * 7. To iterate over all entries use hashtbl_first(), hashtbl_next().
 *
 * Note: neither the keys or the values are copied so their lifetime
 * must match that of the hash table.  NULL keys are not permitted.
 * Inserting, removing or lookup up NULL keys is therefore undefined.
 *
 *
 */

#ifndef HASHTBL_H
#define HASHTBL_H

#ifdef	__cplusplus
# define __BEGIN_DECLS	extern "C" {
# define __END_DECLS	}
#else
# define __BEGIN_DECLS
# define __END_DECLS
#endif

__BEGIN_DECLS

#include <stddef.h>		/* size_t */

/* Opaque types. */

struct hashtbl;

typedef unsigned long hashtbl_hashval_t;

/* Hash function. */
typedef unsigned int (*HASHTBL_HASH_FUNC)(const void *k);

/* Equals function. */
typedef int (*HASHTBL_EQUALS_FUNC)(const void *a, const void *b);

/* Apply function. */
typedef int (*HASHTBL_APPLY_FUNC)(const void *k, const void *v, const void *u);

/* Functions for deleting keys and values. */
typedef void (*HASHTBL_KEY_FREE_FUNC)(void *k);
typedef void (*HASHTBL_VAL_FREE_FUNC)(void *v);

/* Functions for allocting an freeing memory. */
typedef void * (*HASHTBL_MALLOC_FUNC)(size_t n);
typedef void (*HASHTBL_FREE_FUNC)(void *ptr);

typedef enum {
	HASHTBL_LRU_ORDER = 1,
	HASHTBL_MRU_ORDER = 2
} hashtbl_iteration_order;

typedef enum {
	HASHTBL_AUTO_RESIZE = 1,
	HASHTBL_NO_RESIZE   = 2
} hashtbl_resize_policy;

struct hashtbl_iter {
	void *key, *val;
	const void *private; /* clients shouldn't modify this */
};

/*
 * [Default] Hash function.
 */
unsigned int hashtbl_direct_hash(const void *k);

/*
 * [Default] Key equals function.
 *
 * Returns 1 if key "a" equals key "b".
 */
int hashtbl_direct_equals(const void *a, const void *b);

/* Hash functions for integer keys/values. */
unsigned int hashtbl_int_hash(const void *k);
int hashtbl_int_equals(const void *a, const void *b);

/* Hash functions for nul-terminated string keys/values. */
int hashtbl_string_equals(const void *a, const void *b);
unsigned int hashtbl_string_hash(const void *k);

/*
 * Creates a new hash table.
 *
 * @param initial_capacity - initial size of the table
 * @param resize_policy	   - HASHTBL_{AUTO_RESIZE, NO_RESIZE}
 * @param iteration_order  - either MRU or LRU
 * @param hash_fun	   - function that computes a hash value from a key
 * @param equals_fun	   - function that checks keys for equality
 * @param kfreefunc	   - function to delete "key" instances
 * @param kfreefunc	   - function to delete "value" instances
 * @param mallocfunc	   - function to allocate memory
 * @param freefunc	   - function to free memory
 *
 * Returns non-null if the table was created succesfully.
 */
struct hashtbl *hashtbl_new(int initial_capacity,
			    float max_load_factor,
			    hashtbl_resize_policy resize_policy,
			    hashtbl_iteration_order iteration_order,
			    HASHTBL_HASH_FUNC hash_fun,
			    HASHTBL_EQUALS_FUNC equals_fun,
			    HASHTBL_KEY_FREE_FUNC kfreefunc,
			    HASHTBL_VAL_FREE_FUNC vfreefunc,
			    HASHTBL_MALLOC_FUNC mallocfunc,
			    HASHTBL_FREE_FUNC freefunc);

/*
 * Deletes the hash table instance.
 *
 * All the keys are removed via hashtbl_clear().
 *
 * @param h - hash table
 */
void hashtbl_delete(struct hashtbl *h);

/*
 * Removes a key and value from the table.
 *
 * @param h - hash table instance
 * @param k - key to remove
 *
 * Returns 0 if key was found, otherwise 1.
 */
int hashtbl_remove(struct hashtbl *h, const void *k);

/*
 * Clears all entries and reclaims memory used by each entry.
 */
void hashtbl_clear(struct hashtbl *h);

/*
 * Inserts a new key with associated value.
 *
 * @param h - hashtable instance
 * @param k - key to insert
 * @param v - value associated with key
 *
 * Returns 0 on succees, or 1 if a new entry cannot be created.
 */
int hashtbl_insert(struct hashtbl *h, void *k, void *v);

/*
 * Lookup an existing key.
 *
 * @param h - hashtable instance
 * @param k - the search key
 *
 * Returns the value associated with key, or NULL if key is not present.
 */
void * hashtbl_lookup(struct hashtbl *h, const void *k);

/*
 * Returns the number of entries in the table.
 */
unsigned int hashtbl_count(const struct hashtbl *h);

/*
 * Returns the table's capacity.
 */
int hashtbl_capacity(const struct hashtbl *h);

/*
 * Apply a function to all entries in the table.
 *
 * The apply function should return 0 to terminate the enumeration
 * early.
 *
 * Returns the number of entries the function was applied to.
 */
unsigned int hashtbl_apply(const struct hashtbl *h,
			   HASHTBL_APPLY_FUNC f, void *user_arg);

/*
 * Returns the load factor of the hash table (as a percentage).
 *
 * @param h - hash table instance
 *
 * The load factor is a ratio and is calculated as:
 *
 *   hashtbl_count() / hashtbl_capacity()
 *
 * and expressed as a percentage.
 */
int hashtbl_load_factor(const struct hashtbl *h);

/*
 * Resize the hash table.
 *
 * Returns 0 on succees, or 1 if no memory could be allocated.
 */
int hashtbl_resize(struct hashtbl *h, int new_capacity);

/*
 * Initialize an iterator.
 */
void hashtbl_iter_init(struct hashtbl *h, struct hashtbl_iter *iter);

/*
 * Advances the iterator.
 *
 * Returns 1 while there more entries, otherwise 0.
 */
int hashtbl_iter_next(struct hashtbl *h, struct hashtbl_iter *iter);

__END_DECLS

#endif	/* HASHTBL_H */
