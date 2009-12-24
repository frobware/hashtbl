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

#include <stddef.h>
#include <stdlib.h>		/* size_t, offsetof, NULL */
#include <string.h>		/* strcmp */
#if defined(linux)
#include <inttypes.h>		/* intptr_t */
#endif
#include <assert.h>
#include "hashtbl.h"

#ifndef HASHTBL_MAX_LOAD_FACTOR
#define HASHTBL_MAX_LOAD_FACTOR	0.75
#endif

#if defined(_MSC_VER)
  #define INLINE __inline
#else
  #define INLINE inline
#endif

#define DLLIST_CONTAINER_OF(PTR, TYPE, FIELD) \
	((TYPE *)((char *)PTR - offsetof(TYPE, FIELD)))

#define DLLIST_ENTRY(PTR, TYPE, FIELD) \
	DLLIST_CONTAINER_OF(PTR, TYPE, FIELD)

struct dllist {
	struct dllist *next, *prev;
};

struct hashtbl_entry {
	struct dllist		 list; /* runs through all entries */
	struct hashtbl_entry	*next; /* [single] linked list head */
	void			*key;
	void			*val;
	unsigned int		 hash; /* hash of key */
};

struct hashtbl {
	struct dllist		  all_entries;	/* list head */
	HASHTBL_HASH_FUNC	  hashfun;
	HASHTBL_EQUALS_FUNC	  equalsfun;
	unsigned int		  count;	/* number of entries */
	int			  table_size;	/* absolute table capacity */
	int			  resize_threshold;
	hashtbl_resize_policy	  resize_policy;
	hashtbl_iteration_order	  iteration_order;
	HASHTBL_KEY_FREE_FUNC	  kfreefunc;
	HASHTBL_VAL_FREE_FUNC	  vfreefunc;
	HASHTBL_MALLOC_FUNC	  mallocfunc;
	HASHTBL_FREE_FUNC	  freefunc;
	struct hashtbl_entry	**table;
};

static INLINE int dllist_is_empty(const struct dllist *list)
{
	return list->next == list;
}

static INLINE void dllist_init(struct dllist *list)
{
	list->next = list->prev = list;
}

static INLINE void dllist_add_between(struct dllist *node,
				      struct dllist *prev,
				      struct dllist *next)
{
	node->next = next;
	node->prev = prev;
	prev->next = node;
	next->prev = node;
}

/* Add node after head. */

static INLINE void dllist_add(struct dllist *node, struct dllist *head)
{
	dllist_add_between(node, head, head->next);
}

/* Add node before head. */

static INLINE void dllist_add_after(struct dllist *node, struct dllist *head)
{
	dllist_add_between(node, head->prev, head);
}

static INLINE void dllist_remove(struct dllist *node)
{
	node->prev->next = node->next;
	node->next->prev = node->prev;
}

#ifndef NDEBUG
static int is_power_of_2(unsigned int x)
{
	return ((x & (x - 1)) == 0);
}
#endif

static INLINE int resize_threshold(int capacity)
{
	return (int)((capacity * HASHTBL_MAX_LOAD_FACTOR) + 0.5);
}

/*
 * This function computes the next highest power of 2 for a 32-bit
 * integer (X), greater than or equal to X.
 *
 * This works by copying the highest set bit to all of the lower bits,
 * and then adding one, which results in carries that set all of the
 * lower bits to 0 and one bit beyond the highest set bit to 1. If the
 * original number was a power of 2, then the decrement will reduce it
 * to one less, so that we round up to the same original value.
 */
static unsigned int roundup_to_next_power_of_2(unsigned int x)
{
	x--;
	x |= x >> 1;  /* handle	 2 bit numbers */
	x |= x >> 2;  /* handle	 4 bit numbers */
	x |= x >> 4;  /* handle	 8 bit numbers */
	x |= x >> 8;  /* handle 16 bit numbers */
	x |= x >> 16; /* handle 32 bit numbers */
	x++;
	return x;
}

/*
 * hash helper - Spread the lower order bits.
 * Magic numbers from Java 1.4.
 */
static INLINE unsigned int hash_helper(unsigned int k)
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
static INLINE unsigned long djb2_hash(const unsigned char *str)
{
	unsigned long hash = 5381;

	if (str != NULL) {
		int c;
		while ((c = *str++) != '\0') {
			hash = ((hash << 5) + hash) + c;
		}
	}
	return hash;
}

/* Robert Jenkins' 32 bit integer hash function. */
#if 0
static INLINE unsigned int int32hash(unsigned int a)
{
	a = (a + 0x7ed55d16) + (a << 12);
	a = (a ^ 0xc761c23c) ^ (a >> 19);
	a = (a + 0x165667b1) + (a << 5);
	a = (a + 0xd3a2646c) ^ (a << 9);
	a = (a + 0xfd7046c5) + (a << 3);
	a = (a ^ 0xb55a4f09) ^ (a >> 16);
	return a;
}
#endif

/* Create a new table with NEW_CAPACITY slots.	The capacity is
 * automatically rounded up to the next power of 2, but not beyond the
 * maximum table size (HASHTBL_MAX_TABLE_SIZE).	 *NEW_CAPACITY is set
 * to the rounded value.
 */
static INLINE struct hashtbl_entry ** create_table(const struct hashtbl *h,
						   int *new_capacity)
{
	struct hashtbl_entry **table;
	int capacity = *new_capacity;

	if (capacity < 1) {
		capacity = 1;
	} else if (capacity > HASHTBL_MAX_TABLE_SIZE) {
		capacity = HASHTBL_MAX_TABLE_SIZE;
	}

	capacity = roundup_to_next_power_of_2(capacity);
	assert(is_power_of_2(capacity));
	assert(capacity > 0 && capacity <= HASHTBL_MAX_TABLE_SIZE);

	if ((table = h->mallocfunc(capacity * sizeof(*table))) == NULL)
		return NULL;

	*new_capacity = capacity; /* output value is normalized capacity */
	memset(table, 0, capacity * sizeof(*table));
	return table;
}

static INLINE void record_access(struct hashtbl *h, struct hashtbl_entry *entry)
{
	if (h->iteration_order == HASHTBL_MRU_ORDER) {
		dllist_remove(&entry->list);
		dllist_add(&entry->list, &h->all_entries);
	}
}

static INLINE int slot_num(unsigned int hashval, int table_size)
{
	return hashval & (table_size - 1);
}

static INLINE struct hashtbl_entry *hashtbl_entry_new(const struct hashtbl *h,
						      unsigned int hashval,
						      void *k, void *v)
{
	struct hashtbl_entry *e;

	if ((e = h->mallocfunc(sizeof(*e))) != NULL) {
		e->key = k;
		e->val = v;
		e->hash = hashval;
		e->next = NULL;
		dllist_init(&e->list);
	}
	return e;
}

static INLINE struct hashtbl_entry *find_entry(struct hashtbl *h, const void *k)
{
	unsigned int hv = h->hashfun(k);
	struct hashtbl_entry *entry = h->table[slot_num(hv, h->table_size)];

	while (entry != NULL) {
		if (entry->hash == hv && h->equalsfun(entry->key, k))
			break;
		entry = entry->next;
	}

	return entry;
}

static struct hashtbl_entry * remove_key(struct hashtbl *h, const void *k)
{
	struct hashtbl_entry **chain_ref, *entry = NULL;
	unsigned int hash, slot;

	if (k == NULL)
		return entry;

	hash = h->hashfun(k);
	slot = slot_num(hash, h->table_size);
	entry = h->table[slot];
	chain_ref = &h->table[slot];

	while (entry != NULL) {
		if (entry->hash == hash && h->equalsfun(entry->key, k)) {
			/* advance previous node to next entry. */
			*chain_ref = entry->next;
			h->count--;
			dllist_remove(&entry->list);
			entry->next = NULL;
			dllist_init(&entry->list);
			break;
		}
		chain_ref = &entry->next;
		entry = entry->next;
	}
	return entry;
}

int hashtbl_insert(struct hashtbl *h, void *k, void *v)
{
	struct hashtbl_entry *entry, **slot_ref;

	if (k == NULL)
		return 1;

	if ((entry = find_entry(h, k)) != NULL) {
		if (entry->val != NULL && h->vfreefunc != NULL)
			h->vfreefunc(entry->val);
		entry->val = v;	/* replace current value */
		record_access(h, entry);
		return 0;
	}

	if ((entry = hashtbl_entry_new(h, h->hashfun(k), k, v)) == NULL)
		return 1;

	/* Find head of list and link new entry at the head. */
	slot_ref = &h->table[entry->hash & (h->table_size - 1)];
	entry->next = *slot_ref;
	*slot_ref = entry;
	h->count++;

	/* Move entry to the head of all entries. */
	dllist_add(&entry->list, &h->all_entries);

	if (h->resize_policy == HASHTBL_AUTO_RESIZE) {
		if (h->count > (unsigned int)h->resize_threshold) {
			/* auto resize failures are benign. */
			(void) hashtbl_resize(h, 2 * h->table_size);
		}
	}

	return 0;
}

void * hashtbl_lookup(struct hashtbl *h, const void *k)
{
	if (k != NULL) {
		struct hashtbl_entry *entry = find_entry(h, k);

		if (entry != NULL) {
			record_access(h, entry);
			return entry->val;
		}
	}
	return NULL;
}

int hashtbl_remove(struct hashtbl *h, const void *k)
{
	struct hashtbl_entry *entry = remove_key(h, k);

	if (entry != NULL) {
		if (h->kfreefunc != NULL)
			h->kfreefunc(entry->key);
		if (h->vfreefunc != NULL)
			h->vfreefunc(entry->val);
		h->freefunc(entry);
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
		if (h->kfreefunc != NULL)
			h->kfreefunc(entry->key);
		if (h->vfreefunc != NULL)
			h->vfreefunc(entry->val);
		h->table[entry->hash & (h->table_size -1)] = NULL;
		dllist_remove(&entry->list);
		h->freefunc(entry);
		--h->count;
	}

	dllist_init(&h->all_entries);
	assert(h->count == 0);
	assert(dllist_is_empty(&h->all_entries));
}

void hashtbl_delete(struct hashtbl *h)
{
	if (h != NULL) {
		hashtbl_clear(h);
		h->freefunc(h->table);
		h->freefunc(h);
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

struct hashtbl *hashtbl_new(int capacity,
			    hashtbl_resize_policy resize_policy,
			    hashtbl_iteration_order iteration_order,
			    HASHTBL_HASH_FUNC hashfun,
			    HASHTBL_EQUALS_FUNC equalsfun,
			    HASHTBL_KEY_FREE_FUNC kfreefunc,
			    HASHTBL_VAL_FREE_FUNC vfreefunc,
			    HASHTBL_MALLOC_FUNC mallocfunc,
			    HASHTBL_FREE_FUNC freefunc)
{
	struct hashtbl *h;

	mallocfunc = mallocfunc ? mallocfunc : malloc;
	freefunc = freefunc ? freefunc : free;

	if ((h = (*mallocfunc)(sizeof(*h))) == NULL)
		return NULL;

	h->hashfun = hashfun ? hashfun : hashtbl_direct_hash;
	h->equalsfun = equalsfun ? equalsfun : hashtbl_direct_equals;
	h->count = 0;
	h->table_size = capacity;
	h->resize_threshold = resize_threshold(h->table_size);
	h->resize_policy = resize_policy;
	h->iteration_order = iteration_order;
	h->kfreefunc = kfreefunc;
	h->vfreefunc = vfreefunc;
	h->mallocfunc = mallocfunc ? mallocfunc : malloc;
	h->freefunc = freefunc ? freefunc : free;

	dllist_init(&h->all_entries);

	if ((h->table = create_table(h, &h->table_size)) == NULL) {
		h->freefunc(h);
		return NULL;
	}

	return h;
}

int hashtbl_resize(struct hashtbl *h, int capacity)
{
	struct dllist *node, *head = &h->all_entries;
	struct hashtbl_entry *entry, **new_table;

	/* Don't grow if there's no increase in size. */
	if (capacity < h->table_size || capacity == h->table_size)
		return 0;

	/* Don't grow beyond our maximum. */
	if (h->table_size == HASHTBL_MAX_TABLE_SIZE)
		return 0;

	if ((new_table = create_table(h, &capacity)) == NULL)
		return 1;

	/* Transfer all entries from old table to new_table. */

	for (node = head->next; node != head; node = node->next) {
		struct hashtbl_entry **slot_ref;
		entry = DLLIST_ENTRY(node, struct hashtbl_entry, list);
		slot_ref = &new_table[slot_num(entry->hash, capacity)];
		entry->next = *slot_ref;
		*slot_ref = entry;
	}

	h->freefunc(h->table);
	h->table_size = capacity;
	h->table = new_table;
	h->resize_threshold = resize_threshold(capacity);

	return 0;
}

unsigned int hashtbl_apply(const struct hashtbl *h,
			   HASHTBL_APPLY_FUNC apply, void *user_arg)
{
	unsigned int count = 0;
	struct dllist *node;
	const struct dllist *head = &h->all_entries;

	for (node = head->next; node != head; node = node->next) {
		struct hashtbl_entry *entry;
		entry = DLLIST_ENTRY(node, struct hashtbl_entry, list);
		count++;
		if (apply(entry->key, entry->val, user_arg) != 1)
			return count;
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
	return hash_helper(*(unsigned int *)k);
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
	return (int)(((h->count / (float)h->table_size) * 100.0) + 0.5);
}

void hashtbl_iter_init(struct hashtbl *h, struct hashtbl_iter *iter)
{
	iter->key = iter->val = NULL;
	iter->private = h->all_entries.next;
}

int hashtbl_iter_next(struct hashtbl *h, struct hashtbl_iter *iter)
{
	struct dllist *node;
	struct hashtbl_entry *entry;
	assert(iter->private);
	node = (struct dllist *)iter->private;
	if (node == &h->all_entries) return 0;
	entry = DLLIST_ENTRY(node, struct hashtbl_entry, list);
	node = entry->list.next;
	iter->private = node;
	iter->key = entry->key;
	iter->val = entry->val;
	return 1;
}
