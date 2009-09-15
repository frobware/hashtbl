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

/* hashtbl_test.c - unit tests for hashtbl */

#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>
#include "CUnitTest.h"
#include "hashtbl.h"

#define UNUSED_PARAMETER(x) (void)(x)

#define AUTO_RESIZE 1
#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

static int ht_size = 1;

struct value {
	int x[12];
	char c;
	float f;
	int v;
};

struct key {
	char *s[10];
	float fval;
	char c;
	int k;
};

static unsigned int struct_key_hash(const void *k)
{
	return ((struct key *)k)->k;
}

static int struct_key_equals(const void *a, const void *b)
{
	return ((struct key *)a)->k == ((struct key *)b)->k;
}

static int test6_apply_fn1(const void *k, const void *v, const void *u)
{
	UNUSED_PARAMETER(k);
	*(unsigned int *)u += ((struct value *)v)->v;
	return 1;
}

static int test6_apply_fn2(const void *k, const void *v, const void *u)
{
	UNUSED_PARAMETER(k);
	UNUSED_PARAMETER(v);
	*(int *)u *= 2;
	return 0;
}

static int test12_apply_fn1(const void *k, const void *v, const void *u)
{
	struct key *k1 = (struct key *)k;
	struct value *v1 = (struct value *)v;
	int test12_max = *(int *) u;
	CUT_ASSERT_EQUAL(v1->v - test12_max, k1->k);
	return 1;
}

/* Test basic hash table creation/clear/size. */

static int test1(void)
{
	struct hashtbl *h = NULL;
	h = hashtbl_new(ht_size, AUTO_RESIZE,
			struct_key_hash, struct_key_equals,
			NULL, NULL);
	CUT_ASSERT_NOT_NULL(h);
	CUT_ASSERT_EQUAL(0, hashtbl_count(h));
	hashtbl_clear(h);
	CUT_ASSERT_EQUAL(0, hashtbl_count(h));
	hashtbl_delete(h);
	return 0;
}

/* Test lookup of non-existent key. */

static int test2(void)
{
	struct key k;
	struct hashtbl *h = NULL;
	h = hashtbl_new(ht_size, AUTO_RESIZE,
			struct_key_hash, struct_key_equals,
			NULL, NULL);
	CUT_ASSERT_NOT_NULL(h);
	memset(&k, 0, sizeof(k));
	k.k = 2;
	CUT_ASSERT_EQUAL(0, hashtbl_count(h));
	CUT_ASSERT_NULL(hashtbl_lookup(h, &k));
	hashtbl_clear(h);
	CUT_ASSERT_EQUAL(0, hashtbl_count(h));
	hashtbl_delete(h);
	return 0;
}

/* Test lookup of key insert. */

static int test3(void)
{
	struct key k;
	struct value v;
	struct hashtbl *h = NULL;
	h = hashtbl_new(ht_size, AUTO_RESIZE,
			struct_key_hash, struct_key_equals,
			NULL, NULL);
	CUT_ASSERT_NOT_NULL(h);
	memset(&k, 0, sizeof(k));
	memset(&v, 0, sizeof(v));
	k.k = 3;
	v.v = 300;
	CUT_ASSERT_EQUAL(0, hashtbl_count(h));
	CUT_ASSERT_EQUAL(0, hashtbl_insert(h, &k, &v));
	CUT_ASSERT_EQUAL(1, hashtbl_count(h));
	CUT_ASSERT_EQUAL(&v, hashtbl_lookup(h, &k));
	CUT_ASSERT_EQUAL(300, ((struct value *)hashtbl_lookup(h, &k))->v);
	hashtbl_clear(h);
	CUT_ASSERT_EQUAL(0, hashtbl_count(h));
	hashtbl_delete(h);
	return 0;
}

/* Test lookup of multiple key insert. */

static int test4(void)
{
	struct key k1, k2;
	struct value v1, v2;
	struct hashtbl *h = NULL;
	h = hashtbl_new(ht_size, AUTO_RESIZE,
			struct_key_hash, struct_key_equals,
			NULL, NULL);
	CUT_ASSERT_NOT_NULL(h);
	memset(&k1, 0, sizeof(k1));
	memset(&k2, 0, sizeof(k2));
	memset(&k1, 0, sizeof(k1));
	memset(&k2, 0, sizeof(k2));
	k1.k = 3;
	v1.v = 300;
	CUT_ASSERT_EQUAL(0, hashtbl_count(h));
	CUT_ASSERT_EQUAL(0, hashtbl_insert(h, &k1, &v1));
	CUT_ASSERT_EQUAL(1, hashtbl_count(h));
	CUT_ASSERT_EQUAL(&v1, hashtbl_lookup(h, &k1));
	CUT_ASSERT_EQUAL(300, ((struct value *)hashtbl_lookup(h, &k1))->v);

	hashtbl_clear(h);
	CUT_ASSERT_EQUAL(0, hashtbl_count(h));

	k2.k = 4;
	v2.v = 400;
	CUT_ASSERT_NOT_NULL(h);
	CUT_ASSERT_EQUAL(0, hashtbl_count(h));
	CUT_ASSERT_EQUAL(0, hashtbl_insert(h, &k2, &v2));
	CUT_ASSERT_EQUAL(1, hashtbl_count(h));
	CUT_ASSERT_EQUAL(&v2, hashtbl_lookup(h, &k2));
	CUT_ASSERT_EQUAL(400, ((struct value *)hashtbl_lookup(h, &k2))->v);

	hashtbl_clear(h);
	CUT_ASSERT_EQUAL(0, hashtbl_count(h));

	hashtbl_delete(h);
	return 0;
}

/* Test multiple key lookups. */

static int test5(void)
{
	struct key k1, k2;
	struct value v1, v2;
	struct hashtbl *h = NULL;
	h = hashtbl_new(ht_size, AUTO_RESIZE,
			struct_key_hash, struct_key_equals,
			NULL, NULL);
	CUT_ASSERT_NOT_NULL(h);

	memset(&k1, 0, sizeof(k1));
	memset(&k2, 0, sizeof(k2));
	memset(&k1, 0, sizeof(k1));
	memset(&k2, 0, sizeof(k2));

	k1.k = 3;
	v1.v = 300;
	CUT_ASSERT_EQUAL(0, hashtbl_count(h));
	CUT_ASSERT_EQUAL(0, hashtbl_insert(h, &k1, &v1));
	CUT_ASSERT_EQUAL(1, hashtbl_count(h));
	CUT_ASSERT_EQUAL(&v1, hashtbl_lookup(h, &k1));
	CUT_ASSERT_EQUAL(300, ((struct value *)hashtbl_lookup(h, &k1))->v);
	CUT_ASSERT_EQUAL(1, hashtbl_count(h));

	k2.k = 4;
	v2.v = 400;
	CUT_ASSERT_EQUAL(0, hashtbl_insert(h, &k2, &v2));
	CUT_ASSERT_EQUAL(2, hashtbl_count(h));
	CUT_ASSERT_EQUAL(&v2, hashtbl_lookup(h, &k2));
	CUT_ASSERT_EQUAL(400, ((struct value *)hashtbl_lookup(h, &k2))->v);

	CUT_ASSERT_EQUAL(&v1, hashtbl_lookup(h, &k1));
	CUT_ASSERT_EQUAL(300, ((struct value *)hashtbl_lookup(h, &k1))->v);

	hashtbl_clear(h);
	CUT_ASSERT_EQUAL(0, hashtbl_count(h));

	hashtbl_delete(h);
	return 0;
}

/* Test hashtable apply function. */

static int test6(void)
{
	int accumulator = 0;
	struct key k1, k2;
	struct value v1, v2;
	struct hashtbl *h = NULL;

	h = hashtbl_new(ht_size, AUTO_RESIZE,
			struct_key_hash, struct_key_equals,
			NULL, NULL);

	CUT_ASSERT_NOT_NULL(h);

	memset(&k1, 0, sizeof(k1));
	memset(&k2, 0, sizeof(k2));
	memset(&v1, 0, sizeof(v1));
	memset(&v2, 0, sizeof(v2));

	k1.k = 3;
	v1.v = 300;
	CUT_ASSERT_EQUAL(0, hashtbl_count(h));
	CUT_ASSERT_EQUAL(0, hashtbl_insert(h, &k1, &v1));
	CUT_ASSERT_EQUAL(1, hashtbl_count(h));
	CUT_ASSERT_EQUAL(&v1, hashtbl_lookup(h, &k1));
	CUT_ASSERT_EQUAL(300, ((struct value *)hashtbl_lookup(h, &k1))->v);
	CUT_ASSERT_EQUAL(1, hashtbl_count(h));

	k2.k = 4;
	v2.v = 400;
	CUT_ASSERT_NOT_NULL(h);
	CUT_ASSERT_EQUAL(0, hashtbl_insert(h, &k2, &v2));
	CUT_ASSERT_EQUAL(2, hashtbl_count(h));
	CUT_ASSERT_EQUAL(&v2, hashtbl_lookup(h, &k2));
	CUT_ASSERT_EQUAL(400, ((struct value *)hashtbl_lookup(h, &k2))->v);

	CUT_ASSERT_EQUAL(2, hashtbl_map(h, test6_apply_fn1, &accumulator));
	CUT_ASSERT_EQUAL(700, accumulator);

	CUT_ASSERT_EQUAL(1, hashtbl_map(h, test6_apply_fn2, &accumulator));
	CUT_ASSERT_EQUAL(1400, accumulator);

	hashtbl_clear(h);
	CUT_ASSERT_EQUAL(0, hashtbl_count(h));

	hashtbl_delete(h);
	return 0;
}

/* Test key/value remove(). */

static int test7(void)
{
	struct key k;
	struct value v;
	struct hashtbl *h = NULL;

	h = hashtbl_new(ht_size, AUTO_RESIZE,
			struct_key_hash, struct_key_equals,
			NULL, NULL);

	CUT_ASSERT_NOT_NULL(h);

	memset(&k, 0, sizeof(k));
	memset(&v, 0, sizeof(v));

	k.k = 3;
	v.v = 300;
	CUT_ASSERT_EQUAL(0, hashtbl_count(h));
	CUT_ASSERT_EQUAL(0, hashtbl_insert(h, &k, &v));
	CUT_ASSERT_EQUAL(1, hashtbl_count(h));
	CUT_ASSERT_EQUAL(&v, hashtbl_lookup(h, &k));
	CUT_ASSERT_EQUAL(300, ((struct value *)hashtbl_lookup(h, &k))->v);
	CUT_ASSERT_EQUAL(&v, hashtbl_remove(h, &k, NULL));
	CUT_ASSERT_EQUAL(0, hashtbl_count(h));
	CUT_ASSERT_NULL(hashtbl_remove(h, &k, NULL));
	CUT_ASSERT_EQUAL(0, hashtbl_count(h));
	hashtbl_clear(h);
	CUT_ASSERT_EQUAL(0, hashtbl_count(h));
	hashtbl_delete(h);

	return 0;
}

/* Test key insert, remove with dynamic keys and values. */

static int test8(void)
{
	struct hashtbl *h = NULL;
	struct key *k = malloc(sizeof(struct key));
	struct value *v = malloc(sizeof(struct value));

	h = hashtbl_new(ht_size, AUTO_RESIZE,
			struct_key_hash, struct_key_equals,
			free, free);
	CUT_ASSERT_NOT_NULL(h);
	CUT_ASSERT_NOT_NULL(k);
	CUT_ASSERT_NOT_NULL(v);

	memset(k, 0, sizeof(*k));
	memset(v, 0, sizeof(*v));

	k->k = 3;
	v->v = 300;
	CUT_ASSERT_EQUAL(0, hashtbl_count(h));
	CUT_ASSERT_EQUAL(0, hashtbl_insert(h, k, v));
	CUT_ASSERT_EQUAL(1, hashtbl_count(h));
	CUT_ASSERT_EQUAL(v, hashtbl_lookup(h, k));
	CUT_ASSERT_EQUAL(300, ((struct value *)hashtbl_lookup(h, k))->v);
	CUT_ASSERT_EQUAL(v, hashtbl_remove(h, k, NULL));
	CUT_ASSERT_EQUAL(0, hashtbl_count(h));
	CUT_ASSERT_NULL(hashtbl_remove(h, k, NULL));
	CUT_ASSERT_EQUAL(0, hashtbl_count(h));
	free(k);
	free(v);
	hashtbl_clear(h);
	CUT_ASSERT_EQUAL(0, hashtbl_count(h));
	hashtbl_delete(h);
	return 0;
}

/* Test null key insertion. */

static int test9(void)
{
	struct hashtbl *h = NULL;
	h = hashtbl_new(ht_size, AUTO_RESIZE,
			struct_key_hash, struct_key_equals,
			NULL, NULL);
	CUT_ASSERT_NOT_NULL(h);
	CUT_ASSERT_EQUAL(0, !hashtbl_insert(h, NULL, NULL));
	hashtbl_delete(h);
	return 0;
}

/* Test null key lookup. */

static int test10(void)
{
	struct hashtbl *h = NULL;
	h = hashtbl_new(ht_size, AUTO_RESIZE,
			struct_key_hash, struct_key_equals,
			NULL, NULL);
	CUT_ASSERT_NOT_NULL(h);
	CUT_ASSERT_NULL(hashtbl_lookup(h, NULL));
	hashtbl_delete(h);
	return 0;
}

/* Test null key removal. */

static int test11(void)
{
	struct hashtbl *h = NULL;
	h = hashtbl_new(ht_size, AUTO_RESIZE,
			struct_key_hash, struct_key_equals,
			free, free);
	CUT_ASSERT_NOT_NULL(h);
	CUT_ASSERT_NULL(hashtbl_remove(h, NULL, free));
	hashtbl_delete(h);
	return 0;
}

static int test12(void)
{
	int test12_max = 100;
	struct hashtbl *h = NULL;
	int i;
	h = hashtbl_new(ht_size, AUTO_RESIZE,
			struct_key_hash, struct_key_equals,
			free, free);
	CUT_ASSERT_NOT_NULL(h);
	CUT_ASSERT_EQUAL(0, hashtbl_count(h));

	for (i = 0; i < test12_max; i++) {
		struct key *k = malloc(sizeof(struct key));
		struct value *v = malloc(sizeof(struct value));
		CUT_ASSERT_NOT_NULL(k);
		CUT_ASSERT_NOT_NULL(v);
		memset(k, 0, sizeof(*k));
		memset(v, 0, sizeof(*v));
		k->k = i;
		v->v = i + test12_max;
		CUT_ASSERT_EQUAL(0, hashtbl_insert(h, k, v));
		CUT_ASSERT_EQUAL(i + 1, (int) hashtbl_count(h));
		CUT_ASSERT_EQUAL(i + test12_max,
				 ((struct value *)hashtbl_lookup(h, k))->v);
	}

	hashtbl_map(h, test12_apply_fn1, &test12_max);

	for (i = 0; i < test12_max; i++) {
		struct key k;
		struct value *v;
		memset(&k, 0, sizeof(k));
		k.k = i;
		v = hashtbl_lookup(h, &k);
		CUT_ASSERT_NOT_NULL(v);
		CUT_ASSERT_EQUAL(i + test12_max, v->v);
	}

	for (i = 99; i >= 0; i--) {
		struct key k;
		struct value *v;
		memset(&k, 0, sizeof(k));
		k.k = i;
		v = hashtbl_lookup(h, &k);
		CUT_ASSERT_NOT_NULL(v);
		CUT_ASSERT_EQUAL(v->v - test12_max, i);
	}

	hashtbl_clear(h);
	hashtbl_delete(h);

	return 0;
}

/* Test direct hash/equals functions. */

static int test13(void)
{
	int i;
	int keys[] = { 100, 200, 300 };
	int values[] = { 1000, 2000, 3000 };
	struct hashtbl *h = NULL;
	h = hashtbl_new(ht_size, AUTO_RESIZE,
			hashtbl_direct_hash, hashtbl_direct_equals,
			NULL, NULL);
	CUT_ASSERT_NOT_NULL(h);
	CUT_ASSERT_EQUAL(0, hashtbl_count(h));
	for (i = 0; i < (int)ARRAY_SIZE(keys); i++) {
		CUT_ASSERT_EQUAL(0, hashtbl_insert(h, &keys[i], &values[i]));
		CUT_ASSERT_NOT_NULL(hashtbl_lookup(h, &keys[i]));
		CUT_ASSERT_EQUAL(values[i],
				 *(int *)hashtbl_lookup(h, &keys[i]));
	}
	CUT_ASSERT_EQUAL((int)ARRAY_SIZE(keys), hashtbl_count(h));
	for (i = 0; i < (int)ARRAY_SIZE(keys); i++) {
		CUT_ASSERT_EQUAL(values[i],
				 *(int *)hashtbl_remove(h, &keys[i], NULL));
	}
	CUT_ASSERT_EQUAL(0, hashtbl_count(h));
	hashtbl_clear(h);
	hashtbl_delete(h);
	return 0;
}

/* Test int hash/equals functions. */

static int test14(void)
{
	unsigned int i;
	int keys[] = { 100, 200, 300 };
	int values[] = { 1000, 2000, 3000 };
	struct hashtbl *h = NULL;
	h = hashtbl_new(ht_size, AUTO_RESIZE,
			hashtbl_int_hash, hashtbl_int_equals,
			NULL, NULL);
	CUT_ASSERT_NOT_NULL(h);
	CUT_ASSERT_EQUAL(0, hashtbl_count(h));
	for (i = 0; i < ARRAY_SIZE(keys); i++) {
		int x = keys[i];
		CUT_ASSERT_EQUAL(0, hashtbl_insert(h, &keys[i], &values[i]));
		CUT_ASSERT_NOT_NULL(hashtbl_lookup(h, &x));
		CUT_ASSERT_EQUAL(values[i], *(int *)hashtbl_lookup(h, &x));
	}
	CUT_ASSERT_EQUAL(ARRAY_SIZE(keys), hashtbl_count(h));
	for (i = 0; i < ARRAY_SIZE(keys); i++) {
		int x = keys[i];
		CUT_ASSERT_EQUAL(values[i],
				 *(int *)hashtbl_remove(h, &x, NULL));
	}
	CUT_ASSERT_EQUAL(0, hashtbl_count(h));
	hashtbl_clear(h);
	hashtbl_delete(h);
	return 0;
}

/* Test string hash/equals functions. */

static int test15(void)
{
	unsigned int i;
	const char *keys[] = { "100", "200", "300" };
	const char *values[] = { "1000", "2000", "3000" };
	struct hashtbl *h = NULL;
	h = hashtbl_new(ht_size, AUTO_RESIZE,
			hashtbl_string_hash, hashtbl_string_equals,
			NULL, NULL);
	CUT_ASSERT_NOT_NULL(h);
	CUT_ASSERT_EQUAL(0, hashtbl_count(h));
	for (i = 0; i < ARRAY_SIZE(keys); i++) {
		int ss;		/* same string */
		CUT_ASSERT_EQUAL(0, hashtbl_insert(h, &keys[i], &values[i]));
		CUT_ASSERT_NOT_NULL(hashtbl_lookup(h, &keys[i]));
		ss = strcmp(values[i],
			    (char *)hashtbl_lookup(h, &keys[i])) == 0;
		CUT_ASSERT_EQUAL(0, ss);
	}
	hashtbl_clear(h);
	hashtbl_delete(h);
	return 0;
}

/* Test hashtbl_replace */

static int test16(void)
{
	unsigned int i;
	char *replace_str = "hello world";
	const char *keys[] = { "100", "200", "300" };
	const char *vals[] = { "1000", "2000", "3000" };
	struct hashtbl *h = NULL;
	h = hashtbl_new(ht_size, AUTO_RESIZE,
			hashtbl_string_hash, hashtbl_string_equals,
			NULL, NULL);
	CUT_ASSERT_NOT_NULL(h);
	CUT_ASSERT_EQUAL(0, hashtbl_count(h));
	for (i = 0; i < ARRAY_SIZE(keys); i++) {
		CUT_ASSERT_EQUAL(0, hashtbl_insert(h, &keys[i], &vals[0]));
		CUT_ASSERT_NOT_NULL(hashtbl_lookup(h, &keys[i]));
		CUT_ASSERT_EQUAL(&vals[0], hashtbl_lookup(h, &keys[i]));
		CUT_ASSERT_EQUAL(&vals[0], hashtbl_replace(h, &keys[i], replace_str));
		CUT_ASSERT_EQUAL(replace_str, hashtbl_lookup(h, &keys[i]));
	}
	hashtbl_clear(h);
	hashtbl_delete(h);
	return 0;
  
}

CUT_BEGIN_TEST_HARNESS CUT_RUN_TEST(test1);
CUT_RUN_TEST(test2);
CUT_RUN_TEST(test3);
CUT_RUN_TEST(test4);
CUT_RUN_TEST(test5);
CUT_RUN_TEST(test6);
CUT_RUN_TEST(test7);
CUT_RUN_TEST(test8);
CUT_RUN_TEST(test9);
CUT_RUN_TEST(test10);
CUT_RUN_TEST(test11);
CUT_RUN_TEST(test12);
CUT_RUN_TEST(test13);
CUT_RUN_TEST(test14);
CUT_RUN_TEST(test15);
CUT_RUN_TEST(test16);
CUT_END_TEST_HARNESS
