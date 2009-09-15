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

#ifndef HASHTBL_H
#define HASHTBL_H

/**
 * A hash table: efficiently maps keys to values.
 *
 * Note: neither the keys or the values are copied so their lifetime
 * must match that of the hash table.  NULL keys are not permitted
 * either.
 *
 * SYNOPSIS
 *
 * 1. A hash table is created with hashtbl_new().
 * 2. To insert an entry use hashtbl_insert().
 * 3. To lookup a key use hashtbl_lookup().
 * 4. To remove a key use hashtbl_remove().
 * 5. To iterate over all keys use hashtbl_map().
 * 5. To clear all keys use hashtbl_clear().
 * 6. To delete a hash table instance use hashtbl_delete().
 */

/* Opaque structure. */
struct hashtbl;

/* Hash function. */
typedef unsigned int (*HASHTBL_HASH_FUNC) (const void *k);

/* Equals function. */
typedef int (*HASHTBL_EQUALS_FUNC) (const void *a, const void *b);

/* Map/Apply function. */
typedef int (*HASHTBL_APPLY_FUNC) (const void *k, const void *v, const void *u);

/* Functions for deleting keys and values. */
typedef void (*HASHTBL_KEY_FREE_FUNC) (void *k);
typedef void (*HASHTBL_VAL_FREE_FUNC) (void *v);

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
 * @param auto_resize	   - boolean: whether table should auto resize
 * @param hash_fun	   - function that creates a hash value from a key
 * @param equals_fun	   - function that checks keys for equality
 * @param kfreefunc	   - function to delete "key"
 * @param kfreefunc	   - function to delete "value"
 *
 * Returns non-null if the table was created succesfully.
 */
struct hashtbl *hashtbl_new(int initial_capacity,
			    int auto_resize,
			    HASHTBL_HASH_FUNC hash_fun,
			    HASHTBL_EQUALS_FUNC equals_fun,
			    HASHTBL_KEY_FREE_FUNC kfreefunc,
			    HASHTBL_VAL_FREE_FUNC vfreefunc);

/*
 * Deletes the hash table instance.
 *
 * All the keys are removed via hashtbl_clear().
 *
 * @param h	    - hash table
 */
void hashtbl_delete(struct hashtbl *h);

/*
 * Removes a key from the table, returning the associated value.
 *
 * @param h	    - hash table instance
 * @param k	    - key to remove
 * @param kfreefunc - key free function (maybe NULL)
 *
 * Returns the value if the key was found, otherwise NULL.
 */
void *hashtbl_remove(struct hashtbl *h, const void *k,
		     HASHTBL_KEY_FREE_FUNC kfreefunc);

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
 * Returns the value associated with key, or NULL if not found.
 */
void *hashtbl_lookup(const struct hashtbl *h, const void *k);

/*
 * Replace value for an existing key.
 *
 * @param h - hashtable instance
 * @param k - the search key
 * @param v - new value for key
 *
 * Returns the old value associated with key, or NULL if key not
 * found.
 */
void *hashtbl_replace(const struct hashtbl *h, void *k, void *v);

/*
 * Returns the number of entries in the table.
 */
unsigned int hashtbl_count(const struct hashtbl *h);

/*
 * Returns the table's capacity.
 */
int hashtbl_capacity(const struct hashtbl *h);

/*
 * Apply a function to all keys in the table.
 *
 * The apply function should return 0 to terminate the enumeration
 * early.
 *
 * Returns the number of keys the function was applied to.
 */
unsigned int hashtbl_map(const struct hashtbl *h,
			 HASHTBL_APPLY_FUNC f, void *user_arg);

/*
 * Returns the load factor of the hash table as a percentage.
 *
 * @param h - hash table instance
 *
 * The load factor is the ratio of:
 *
 *   hashtbl_count() / hashtbl_capacity()
 *
 * expressed as a percentage.
 */
int hashtbl_load_factor(const struct hashtbl *h);

/*
 * Resize the hash table.
 *
 * Returns 0 on succees, or 1 if no memory could be allocated.
 */
int hashtbl_resize(struct hashtbl *h, unsigned int new_capacity);

#endif /* HASHTBL_H */
