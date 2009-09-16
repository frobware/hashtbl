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

/*
 * A hash table implementation based on chaining.
 *
 * Each slot in the bucket array is a pointer to a doubly linked list
 * that contains the key-value pairs that are hashed to the same
 * location.  Lookup requires scanning the hashed slot's list for a
 * match with the given key.  Insertion requires appending a new entry
 * to the end of the list in the hashed slot.  Removal requires
 * searching the list and unlinking the element in the doubly linked
 * list.
 */

#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <stddef.h>
#include <assert.h>
#include "hashtbl.h"

#ifndef HASHTBL_MALLOC
#define HASHTBL_MALLOC		malloc
#endif

#ifndef HASHTBL_FREE
#define HASHTBL_FREE		free
#endif

#ifndef MAX_LOAD_FACTOR
#define MAX_LOAD_FACTOR		0.75
#endif

#ifndef MAX_TABLE_SIZE
#define MAX_TABLE_SIZE		(1 << 30)
#endif

#ifndef offsetof
#define list2type(ptr, type, field) \
	((type *) ((char *)(ptr) - (size_t)(&((type *)0)->field)))
#else
#define list2type(ptr, type, field) \
	((type *) ((char *)(ptr) - offsetof(type, field)))
#endif

struct hashtbl_entry {
	void		     *key;
	void		     *val;
	unsigned int	      hash;
	struct hashtbl_entry *next;
};

struct hashtbl {
	HASHTBL_HASH_FUNC	hashfun;
	HASHTBL_EQUALS_FUNC	equalsfun;
	unsigned int		count;
	unsigned int		table_size;
	unsigned int		resize_threshold;
	int			auto_resize;
	HASHTBL_KEY_FREE_FUNC	kfreefunc;
	HASHTBL_VAL_FREE_FUNC	vfreefunc;
	struct hashtbl_entry  **table;
};

struct list_head {
	struct list_head *next;
	struct list_head *prev;
};

static inline int list_is_empty(struct list_head *list)
{
	return (list->next == list && list->prev == list);
}

static inline void list_init(struct list_head *list)
{
	list->next = list->prev = list;
}

/* Add node between next and prev. */
static inline void list_add_between(struct list_head *prev,
				    struct list_head *next, 
				    struct list_head *node)
{
	prev->next = node;
	node->prev = prev;
	node->next = next;
	next->prev = node;
}

/* Add node after head. */

static inline void list_add(struct list_head *head,
			    struct list_head *node)
{
	list_add_between(head, head->next, node);
}

static inline void list_remove(struct list_head *node)
{
	struct list_head *prev = node->prev;
	struct list_head *next = node->next;
	prev->next = next;
	next->prev = prev;
}

/*
 * hash helper - Spread the lower order bits.
 * Magic numbers from Java 1.4.
 */
static inline unsigned int hash_helper(unsigned int k)
{
	unsigned int h = (unsigned int)k;
	h ^= (h >> 20) ^ (h >> 12);
	return h ^ (h >> 7) ^ (h >> 4);
}

/* djb2
 *
 * This algorithm was first reported by Dan Bernstein many years ago
 * in comp.lang.c.
 */
static inline unsigned long djb2_hash(const unsigned char *str)
{
	unsigned long hash = 5381;
	assert(str);
	if (str != NULL) {
		int c;
		while ((c = *str++) != '\0')
			hash = ((hash << 5) + hash) + c;
	}
	return hash;
}

/* Robert Jenkins' 32 bit integer hash function. */

static inline unsigned int int32hash(unsigned int a)
{
	a = (a + 0x7ed55d16) + (a << 12);
	a = (a ^ 0xc761c23c) ^ (a >> 19);
	a = (a + 0x165667b1) + (a << 5);
	a = (a + 0xd3a2646c) ^ (a << 9);
	a = (a + 0xfd7046c5) + (a << 3);
	a = (a ^ 0xb55a4f09) ^ (a >> 16);
	return a;
}

static int hashtbl_init(struct hashtbl *h,
			int initial_capacity,
			int auto_resize,
			HASHTBL_HASH_FUNC hashfun,
			HASHTBL_EQUALS_FUNC equalsfun,
			HASHTBL_KEY_FREE_FUNC kfreefunc,
			HASHTBL_VAL_FREE_FUNC vfreefunc)

{
	int capacity = 1;
	unsigned int i;

	if (initial_capacity < 1) {
		initial_capacity = 1;
	} else if (initial_capacity > MAX_TABLE_SIZE) {
		initial_capacity = MAX_TABLE_SIZE;
	}

	/* Find a power of 2 >= initial_capacity */
	while (capacity < initial_capacity) {
		capacity <<= 1;
	}
	
	h->hashfun	    = hashfun ? hashfun : hashtbl_direct_hash;
	h->equalsfun	    = equalsfun ? equalsfun : hashtbl_direct_equals;
	h->count	    = 0;
	h->table_size	    = capacity;
	h->resize_threshold = (h->table_size * MAX_LOAD_FACTOR) + 0.5;
	h->auto_resize	    = auto_resize;
	h->kfreefunc	    = kfreefunc;
	h->vfreefunc	    = vfreefunc;
	h->table	    = HASHTBL_MALLOC(capacity * sizeof(*(h->table)));

	if (h->table == NULL) {
		return 1;
	}

	for (i = 0; i < h->table_size; i++) {
		h->table[i] = NULL;
	}

	return 0;
}

/* Return pointer to hash table head for an existing entry. */
static inline struct hashtbl_entry **entry2head(const struct hashtbl *h,
						struct hashtbl_entry *e)
{
	return &h->table[e->hash & (h->table_size-1)];
}

/* Return pointer to hash table head for key. */
static inline struct hashtbl_entry **key2head(const struct hashtbl *h,
					      const void *k)
{
	/* Sentinel value. */
	static struct hashtbl_entry *empty_entry[1];

	if (k != NULL) {
		return &h->table[h->hashfun(k) & (h->table_size-1)];
	} else {
		return &empty_entry[0];
	}
}

static inline struct hashtbl_entry *hashtbl_entry_new(struct hashtbl *h,
						      void *k, void *v)
{
	struct hashtbl_entry *e;

	if ((e = HASHTBL_MALLOC(sizeof(*e))) != NULL) {
		e->key	= k;
		e->val	= v;
		e->hash = h->hashfun(e->key);
		e->next = NULL;
	}
	return e;
}

static inline struct hashtbl_entry *lookup_internal(const struct hashtbl *h,
						    const void *k)
{
	struct hashtbl_entry *entry = *key2head(h,k);

	while (entry != NULL) {
		if (h->equalsfun(entry->key, k))
			break;
		entry = entry->next;
	}

	return entry;
}

static inline void unlink_entry(struct hashtbl *h,
				struct hashtbl_entry **head,
				struct hashtbl_entry *entry)
{
	*head = entry->next;
	h->count--;
}

static inline struct hashtbl_entry * remove_key(struct hashtbl *h,
						const void *k)
{
	struct hashtbl_entry **head = key2head(h, k);
	struct hashtbl_entry *entry = *head;

	while (entry != NULL) {
		if (h->equalsfun(entry->key, k)) {
			unlink_entry(h, head, entry);
			break;
		}
		head = &entry->next;
		entry = entry->next;
	}

	return entry;
}

static inline void link_entry(struct hashtbl *h,
			      struct hashtbl_entry *entry)
{
	struct hashtbl_entry **head = entry2head(h, entry);
	entry->next = *head;
	*head = entry;
	h->count++;
	assert(h->count != 0);
}

int hashtbl_insert(struct hashtbl *h, void *k, void *v)
{
	struct hashtbl_entry *entry;

	if (k == NULL)
		return 1;

	if ((entry = hashtbl_entry_new(h, k, v)) == NULL)
		return 1;

	if (h->auto_resize && h->count > h->resize_threshold) {
		/* auto resize failures are benign. */
		(void) hashtbl_resize(h, h->table_size * 2);
	}

	link_entry(h, entry);

	return 0;
}

void * hashtbl_lookup(const struct hashtbl *h, const void *k)
{
	struct hashtbl_entry *entry = lookup_internal(h, k);
	return (entry != NULL) ? entry->val : NULL;
}

void *hashtbl_replace(const struct hashtbl *h, void *k, void *v)
{
	void *old_val = NULL;
	struct hashtbl_entry *entry = lookup_internal(h, k);
	if (entry != NULL) {
		old_val = entry->val;
		entry->val = v;
	}
	return old_val;
}

void *hashtbl_remove(struct hashtbl *h, const void *k,
		     HASHTBL_KEY_FREE_FUNC kfreefunc)
{
	void *v = NULL;
	struct hashtbl_entry *entry = remove_key(h, k);

	if (entry != NULL) {
		v = entry->val;
		if (kfreefunc)
			kfreefunc(entry->key);
		HASHTBL_FREE(entry);
	}
	return v;
}

void hashtbl_clear(struct hashtbl *h)
{
	unsigned int i;
	struct hashtbl_entry *entry;
	struct hashtbl_entry *next;

	for (i = 0; i < h->table_size; i++) {
		next = h->table[i];
		while ((entry = next) != NULL) {
			if (h->kfreefunc != NULL)
				h->kfreefunc(entry->key);
			if (h->vfreefunc != NULL)
				h->vfreefunc(entry->val);
			next = entry->next;
			entry->next = NULL;
			HASHTBL_FREE(entry);
			--h->count;
		}
		assert(next == NULL);
		h->table[i] = next;
	}
	assert(h->count == 0);
}

void hashtbl_delete(struct hashtbl *h)
{
	if (h != NULL) {
		hashtbl_clear(h);
		memset(h->table, 0, sizeof(*h->table) * h->table_size);
		HASHTBL_FREE(h->table);
		memset(h, 0, sizeof(*h));
		HASHTBL_FREE(h);
	}
}

unsigned int hashtbl_count(const struct hashtbl *h)
{
	return (h != NULL) ? h->count : 0;
}

int hashtbl_capacity(const struct hashtbl *h)
{
	return (h != NULL) ? h->table_size : 0;
}

struct hashtbl *hashtbl_new(int initial_capacity,
			    int auto_resize,
			    HASHTBL_HASH_FUNC hash,
			    HASHTBL_EQUALS_FUNC eql,
			    HASHTBL_KEY_FREE_FUNC kfreefunc,
			    HASHTBL_VAL_FREE_FUNC vfreefunc)
{
	struct hashtbl *h;

	if ((h = HASHTBL_MALLOC(sizeof(*h))) == NULL)
		return NULL;

	if (hashtbl_init(h, initial_capacity, auto_resize,
			 hash, eql, kfreefunc, vfreefunc) != 0) {
		HASHTBL_FREE(h);
		return NULL;
	}

	return h;
}

int hashtbl_resize(struct hashtbl *h, unsigned int new_capacity)
{
	unsigned int i;
	struct hashtbl tmp_h;

	if (new_capacity < h->table_size)
		return 0;

	if (new_capacity > MAX_TABLE_SIZE)
		return 1;

	if (hashtbl_init(&tmp_h,
			 new_capacity,
			 0,	/* auto_resize = No */
			 h->hashfun,
			 h->equalsfun,
			 h->kfreefunc, h->vfreefunc) != 0)
		return 1;

	/* Move all key/value entries from h to t. */
	
	for (i = 0; i < h->table_size; i++) {
		struct hashtbl_entry **head = &h->table[i];
		struct hashtbl_entry *entry;
		while ((entry = *head) != NULL) {
			unlink_entry(h, head, entry);
			link_entry(&tmp_h, entry);
			/* Look for other chained keys in this slot */
			head = &h->table[i];
		}
	}

	HASHTBL_FREE(h->table);
	h->table_size = new_capacity;
	h->table = tmp_h.table;
	h->count = tmp_h.count;
	h->resize_threshold = (h->table_size * MAX_LOAD_FACTOR) + 0.5;
	return 0;
}

unsigned int hashtbl_map(const struct hashtbl *h,
			 HASHTBL_APPLY_FUNC apply, void *user_arg)
{
	unsigned int count = 0;
	unsigned int i;

	for (i = 0; i < h->table_size; i++) {
		struct hashtbl_entry *entry = h->table[i];
		while (entry != NULL) {
			count++;
			if (apply(entry->key, entry->val, user_arg) != 1)
				return count;
			entry = entry->next;
		}
	}

	return count;
}

unsigned int hashtbl_string_hash(const void *k)
{
	return djb2_hash((const unsigned char *)k);
}

int hashtbl_string_equals(const void *a, const void *b)
{
	return strcmp(((const char *)a), (const char *)b) == 0;
}

unsigned int hashtbl_int_hash(const void *k)
{
	return hash_helper(*(const unsigned int *)k);
}

int hashtbl_int_equals(const void *a, const void *b)
{
	return *((const int *)a) == *((const int *)b);
}

unsigned int hashtbl_direct_hash(const void *k)
{
	return hash_helper((intptr_t)k);
}

int hashtbl_direct_equals(const void *a, const void *b)
{
	return a == b;
}

int hashtbl_load_factor(const struct hashtbl *h)
{
	return (h->count / (float)h->table_size) * 100.0 + 0.5;
}
