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
 * Each slot in the bucket array is a pointer to a linked list that
 * contains the key-value pairs that are hashed to the same location.
 * Lookup requires scanning the hashed slot's list for a match with
 * the given key.  Insertion requires adding a new entry to the head
 * of the list in the hashed slot.  Removal requires searching the
 * list and unlinking the element in the list.	The hash table also
 * maintains a doubly-linked list running through all of the entries;
 * this list is used for iteration, which is normally the order in
 * which keys are inserted into the table.  Alternatively, iteration
 * can be based on access.
 */

#include <stddef.h>		/* size_t, offsetof, NULL */
#include <stdlib.h>		/* malloc, free */
#include <string.h>		/* strcmp */
#if !defined(_MSC_VER)
#include <stdint.h>		/* intptr_t */
#endif
#include "hashtbl.h"

#define UNUSED_PARAMETER(X)		(void) (X)

#ifndef HASHTBL_MAX_TABLE_SIZE
#define HASHTBL_MAX_TABLE_SIZE		(1 << 30)
#endif

#ifndef HASHTBL_DEFAULT_LOAD_FACTOR
#define HASHTBL_DEFAULT_LOAD_FACTOR	0.75f
#endif

#if defined(_MSC_VER)
  #define INLINE __inline
#else
  #define INLINE inline
#endif

#define DLLIST_CONTAINER_OF(PTR, TYPE, FIELD) \
	(TYPE *)(((char *)PTR) - offsetof(TYPE, FIELD))

#define DLLIST_ENTRY(PTR, TYPE, FIELD) \
	DLLIST_CONTAINER_OF(PTR, TYPE, FIELD)

struct dllist {
	struct dllist *next, *prev;
};

struct hashtbl {
	struct dllist		 all_entries;
	double			 max_load_factor;
	HASHTBL_HASH_FN		 hash_fn;
	HASHTBL_EQUALS_FN	 equals_fn;
	unsigned long		 nentries;
	int			 table_size;
	int			 resize_threshold;
	int			 auto_resize;
	int			 access_order;
	HASHTBL_KEY_FREE_FN	 key_free;
	HASHTBL_VAL_FREE_FN	 val_free;
	HASHTBL_MALLOC_FN	 malloc_fn;
	HASHTBL_FREE_FN		 free_fn;
	HASHTBL_EVICTOR_FN	 evictor_fn;
	struct hashtbl_entry   **table;
};

struct hashtbl_entry {
	struct dllist		 list; /* runs through all entries */
	struct hashtbl_entry	*next; /* [single] linked list head */
	void			*key;
	void			*val;
	unsigned int		 hash; /* hash of key */
};

static INLINE void dllist_init(struct dllist *list)
{
	list->next = list;
	list->prev = list;
}

static INLINE void dllist_add_node(struct dllist *node,
				   struct dllist *prev,
				   struct dllist *next)
{
	node->next = next;
	node->prev = prev;
	prev->next = node;
	next->prev = node;
}

/* Add node after head. */

static INLINE void dllist_add_before(struct dllist *node, struct dllist *head)
{
	dllist_add_node(node, head, head->next);
}

/* Add node before head. */

static INLINE void dllist_add_after(struct dllist *node, struct dllist *head)
{
	dllist_add_node(node, head->prev, head);
}

static INLINE void dllist_remove(struct dllist *node)
{
	node->prev->next = node->next;
	node->next->prev = node->prev;
}

static INLINE int resize_threshold(int capacity, double max_load_factor)
{
	return (int)(((double)capacity * max_load_factor) + 0.5);
}

static int roundup_to_next_power_of_2(int x)
{
	int n = 1;
	while (n < x) n <<=1;
	return n;
}

static int is_power_of_2(int x)
{
	return ((x & (x - 1)) == 0);
}

/*
 * hash helper - Spread the lower order bits.
 * Magic numbers from Java 1.4.
 */
static INLINE unsigned int hash_spreader(unsigned int k)
{
	unsigned int h = k;
	h ^= (h >> 20) ^ (h >> 12);
	return h ^ (h >> 7) ^ (h >> 4);
}

/* djb2
 *
 * This algorithm was first reported by Dan Bernstein many years ago
 * in comp.lang.c.
 */
static INLINE unsigned int djb_hash(const unsigned char *str)
{
	const unsigned char *p = str;
	unsigned int hash = 0;

	if (str != NULL) {
		while (*p++ != '\0') {
			hash = 33 * hash ^ *p;
		}
	}
	return hash;
}

static INLINE unsigned int djb2_hash(const void *key, size_t len)
{
	const unsigned char *p = key;
	unsigned int hash = 0;
	size_t i;

	for (i = 0; i < len; i++) {
		hash = 33 * hash ^ p[i];
	}

	return hash;
}

static INLINE void record_access(struct hashtbl *h, struct hashtbl_entry *entry)
{
	if (h->access_order) {
		/* move to head of all_entries */
		dllist_remove(&entry->list);
		dllist_add_before(&entry->list, &h->all_entries);
	}
}

static INLINE int slot_n(unsigned int hashval, int table_size)
{
	/* The table size is represented as an int, so an int will do. */
	return (int) hashval & (table_size - 1);
}

static INLINE int remove_eldest(const struct hashtbl *h, unsigned long nentries)
{
	UNUSED_PARAMETER(h);
	UNUSED_PARAMETER(nentries);
	return 0;
}

static INLINE struct hashtbl_entry *find_entry(struct hashtbl *h,
					       unsigned int hv,
					       const void *k)
{
	struct hashtbl_entry *entry = h->table[slot_n(hv, h->table_size)];

	while (entry != NULL) {
		if (entry->hash == hv && h->equals_fn(entry->key, k))
			break;
		entry = entry->next;
	}

	return entry;
}

/*
 * Remove an entry from the hash table without deleting the underlying
 * instance.  Returns the entry, or NULL if not found.
 */
static struct hashtbl_entry * remove_key(struct hashtbl *h, const void *k)
{
	unsigned int hv = h->hash_fn(k);
	struct hashtbl_entry **slot_ref = &h->table[slot_n(hv, h->table_size)];
	struct hashtbl_entry *entry = *slot_ref;

	while (entry != NULL) {
		if (entry->hash == hv && h->equals_fn(entry->key, k)) {
			/* advance previous node to next entry. */
			*slot_ref = entry->next;
			h->nentries--;
			dllist_remove(&entry->list);
			break;
		}
		slot_ref = &entry->next;
		entry = entry->next;
	}

	return entry;
}

int hashtbl_insert(struct hashtbl *h, void *k, void *v)
{
	struct hashtbl_entry *entry, **slot_ref;
	unsigned int hv = h->hash_fn(k);

	if ((entry = find_entry(h, hv, k)) != NULL) {
		/* Replace the current value. This should not affect
		 * the iteration order as the key already exists. */
		if (h->val_free != NULL)
			h->val_free(entry->val);
		entry->val = v;
		return 0;
	}

	if ((entry = h->malloc_fn(sizeof(*entry))) == NULL)
		return 1;

	/* Link new entry at the head. */
	slot_ref = &h->table[slot_n(hv, h->table_size)];
	entry->key = k;
	entry->val = v;
	entry->hash = hv;
	entry->next = *slot_ref;

	*slot_ref = entry;
	/* Move new entry to the head of all entries. */
	dllist_add_before(&entry->list, &h->all_entries);

	h->nentries++;

	if (h->evictor_fn(h, h->nentries)) {
		/* Evict oldest entry. */
		struct dllist *node = h->all_entries.prev;
		entry = DLLIST_ENTRY(node, struct hashtbl_entry, list);
		hashtbl_remove(h, entry->key);
	}

	if (h->auto_resize) {
		if (h->nentries >= (unsigned int)h->resize_threshold) {
			/* auto resize failures are benign. */
			(void) hashtbl_resize(h, 2 * h->table_size);
		}
	}

	return 0;
}

void * hashtbl_lookup(struct hashtbl *h, const void *k)
{
	unsigned int hv = h->hash_fn(k);
	struct hashtbl_entry *entry = find_entry(h, hv, k);

	if (entry != NULL) {
		record_access(h, entry);
		return entry->val;
	}

	return NULL;
}

int hashtbl_remove(struct hashtbl *h, const void *k)
{
	struct hashtbl_entry *entry = remove_key(h, k);

	if (entry != NULL) {
		if (h->key_free != NULL)
			h->key_free(entry->key);
		if (h->val_free != NULL && entry->val != NULL)
			h->val_free(entry->val);
		h->free_fn(entry);
		return 0;
	}

	return 1;
}

void hashtbl_clear(struct hashtbl *h)
{
	struct dllist *node, *tmp, *head = &h->all_entries;
	struct hashtbl_entry *entry;

	for (node = head->next, tmp = node->next;
	     node != head;
	     node = tmp, tmp = node->next) {
		entry = DLLIST_ENTRY(node, struct hashtbl_entry, list);
		if (h->key_free != NULL)
			h->key_free(entry->key);
		if (h->val_free != NULL)
			h->val_free(entry->val);
		dllist_remove(&entry->list);
		h->free_fn(entry);
		h->nentries--;
	}

	memset(h->table, 0, (size_t)h->table_size * sizeof(*h->table));
	dllist_init(&h->all_entries);
}

void hashtbl_delete(struct hashtbl *h)
{
	hashtbl_clear(h);
	h->free_fn(h->table);
	h->free_fn(h);
}

unsigned long hashtbl_count(const struct hashtbl *h)
{
	return h->nentries;
}

int hashtbl_capacity(const struct hashtbl *h)
{
	return h->table_size;
}

struct hashtbl *hashtbl_create(int capacity,
			       double max_load_factor,
			       int auto_resize,
			       int access_order,
			       HASHTBL_HASH_FN hash_fn,
			       HASHTBL_EQUALS_FN equals_fn,
			       HASHTBL_KEY_FREE_FN key_free,
			       HASHTBL_VAL_FREE_FN val_free,
			       HASHTBL_MALLOC_FN malloc_fn,
			       HASHTBL_FREE_FN free_fn,
			       HASHTBL_EVICTOR_FN evictor_fn)
{
	struct hashtbl *h;

	malloc_fn  = (malloc_fn != NULL) ? malloc_fn : malloc;
	free_fn	   = (free_fn != NULL) ? free_fn : free;
	hash_fn	   = (hash_fn != NULL) ? hash_fn : hashtbl_direct_hash;
	equals_fn  = (equals_fn != NULL) ? equals_fn : hashtbl_direct_equals;
	evictor_fn = (evictor_fn != NULL) ? evictor_fn : remove_eldest;

	if ((h = malloc_fn(sizeof(*h))) == NULL)
		return NULL;

	if (max_load_factor < 0.0) {
		max_load_factor = HASHTBL_DEFAULT_LOAD_FACTOR;
	} else if (max_load_factor > 1.0) {
		max_load_factor = 1.0f;
	}

	h->max_load_factor = max_load_factor;
	h->hash_fn = hash_fn;
	h->equals_fn = equals_fn;
	h->nentries = 0;
	h->table_size = 0;	/* must be 0 for resize() to work */
	h->resize_threshold = 0;
	h->auto_resize = auto_resize;
	h->access_order = access_order;
	h->key_free = key_free;
	h->val_free = val_free;
	h->malloc_fn = malloc_fn;
	h->free_fn = free_fn;
	h->evictor_fn = evictor_fn;
	h->table = NULL;

	dllist_init(&h->all_entries);

	if (hashtbl_resize(h, capacity) != 0) {
		free_fn(h);
		h = NULL;
	}

	return h;
}

int hashtbl_resize(struct hashtbl *h, int capacity)
{
	struct dllist *node, *head = &h->all_entries;
	struct hashtbl_entry *entry, **new_table;
	
	if (capacity < 1) {
		capacity = 1;
	} else if (capacity >= HASHTBL_MAX_TABLE_SIZE) {
		capacity = HASHTBL_MAX_TABLE_SIZE;
	} else if (!is_power_of_2(capacity)) {
		capacity = roundup_to_next_power_of_2(capacity);
	}

	/* Don't grow if there is no change to the current size. */

	if (capacity < h->table_size || capacity == h->table_size)
		return 0;

	new_table = h->malloc_fn((size_t)capacity * sizeof(*new_table));

	if (new_table == NULL)
		return 1;

	memset(new_table, 0, (size_t)capacity * sizeof(*new_table));

	/* Transfer all entries from old table to new table. */

	for (node = head->next; node != head; node = node->next) {
		struct hashtbl_entry **slot_ref;
		entry = DLLIST_ENTRY(node, struct hashtbl_entry, list);
		slot_ref = &new_table[slot_n(entry->hash, capacity)];
		entry->next = *slot_ref;
		*slot_ref = entry;
	}

	if (h->table != NULL)
		h->free_fn(h->table);
	h->table_size = capacity;
	h->table = new_table;
	h->resize_threshold = resize_threshold(capacity, h->max_load_factor);

	return 0;
}

unsigned long hashtbl_apply(const struct hashtbl *h,
			    HASHTBL_APPLY_FN apply, void *client_data)
{
	unsigned long nentries = 0;
	struct dllist *node;
	const struct dllist *head = &h->all_entries;

	for (node = head->next; node != head; node = node->next) {
		struct hashtbl_entry *entry;
		entry = DLLIST_ENTRY(node, struct hashtbl_entry, list);
		nentries++;
		if (apply(entry->key, entry->val, client_data) != 1)
			return nentries;
	}

	return nentries;
}

unsigned int hashtbl_string_hash(const void *k)
{
	return djb_hash((const unsigned char *)k);
}

int hashtbl_string_equals(const void *a, const void *b)
{
	return strcmp(((const char *)a), (const char *)b) == 0;
}

unsigned int hashtbl_int_hash(const void *k)
{
	return *(unsigned int *)k;
}

int hashtbl_int_equals(const void *a, const void *b)
{
	return *((const int *)a) == *((const int *)b);
}

unsigned int hashtbl_int64_hash(const void *k)
{
	return (unsigned int) *(unsigned long long *)k;
}

int hashtbl_int64_equals(const void *a, const void *b)
{
	return *((const long long int *)a) == *((const long long int *)b);
}

unsigned int hashtbl_direct_hash(const void *k)
{
	return hash_spreader((unsigned int)(uintptr_t)k);
}

int hashtbl_direct_equals(const void *a, const void *b)
{
	return a == b;
}

double hashtbl_load_factor(const struct hashtbl *h)
{
	return (double)h->nentries / (double)h->table_size;
}

void hashtbl_iter_init(struct hashtbl *h, struct hashtbl_iter *iter,
		       hashtbl_iter_direction direction)
{
	iter->key = iter->val = NULL;
	iter->private.direction = direction;

	if (direction == HASHTBL_FORWARD_ITERATOR) {
		iter->private.p = h->all_entries.next;
	} else {
		iter->private.p = h->all_entries.prev;
	}
}

int hashtbl_iter_next(struct hashtbl *h, struct hashtbl_iter *iter)
{
	struct hashtbl_entry *entry;
	struct dllist *node = (struct dllist *)iter->private.p;

	if (node == &h->all_entries)
		return 0;

	if (iter->private.direction == HASHTBL_FORWARD_ITERATOR) {
		iter->private.p = node->next;
	} else {
		iter->private.p = node->prev;
	}

	entry = DLLIST_ENTRY(node, struct hashtbl_entry, list);
	iter->key = entry->key;
	iter->val = entry->val;

	return 1;
}
