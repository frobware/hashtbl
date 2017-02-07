# Copyright (c) 2009, 2010 <Andrew McDermott>
#
# Source can be cloned from:
#
#   https://github.com/frobware/hashtbl.git
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:

# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

PROFILE_FLAGS  = -fprofile-arcs -ftest-coverage
COMMON_CFLAGS += -Wall
COMMON_CFLAGS += -Wformat -Wmissing-prototypes -Wpointer-arith -Wshadow
#COMMON_CFLAGS += -Wuninitialized -O1
ifeq ($(shell uname -s),Darwin)
COMMON_CFLAGS += -Wshorten-64-to-32
endif
COMMON_CFLAGS += -Wstrict-aliasing -fstrict-aliasing -Wconversion -Wcast-align
CFLAGS        += $(COMMON_CFLAGS)
CFLAGS        += -g -fno-inline
#CFLAGS        += -O3 -DNDEBUG
VALGRIND       =

ifeq ($(shell uname -s),Linux)
VALGRIND       = valgrind --quiet --leak-check=full
endif
ifeq ($(shell uname -sr),Darwin 9.8.0)
VALGRIND       = valgrind --quiet --leak-check=full
endif

all : hashtbl_test linked_hashtbl_test
	$(VALGRIND) ./hashtbl_test
	$(VALGRIND) ./linked_hashtbl_test

linked_hashtbl_test: linked_hashtbl_test.c linked_hashtbl.c linked_hashtbl.h hashtbl_funcs.h
	$(CC) $(CFLAGS) -DLINKED_HASHTBL_MAX_TABLE_SIZE='(1<<8)' -o $@ linked_hashtbl.c linked_hashtbl_test.c

hashtbl_test: hashtbl_test.c hashtbl.c hashtbl.h hashtbl_funcs.h
	$(CC) $(CFLAGS) -DHASHTBL_MAX_TABLE_SIZE='(1<<8)' -o $@ hashtbl.c hashtbl_test.c

.PHONY: linked_hashtbl_test.gcov

linked_hashtbl_test.gcov: linked_hashtbl_test.c linked_hashtbl.c
	$(CC) $(CFLAGS) $(PROFILE_FLAGS) -DLINKED_HASHTBL_MAX_TABLE_SIZE='(1<<8)' -g -o $@ linked_hashtbl_test.c linked_hashtbl.c
	./$@
	gcov -a $^

hashtbl_test.gcov: hashtbl_test.c hashtbl.c
	$(CC) $(CFLAGS) $(PROFILE_FLAGS) -DHASHTBL_MAX_TABLE_SIZE='(1<<8)' -g -o $@ hashtbl_test.c hashtbl.c
	./$@
	gcov -a $^

.PHONY : linked_hashtbl_test.pg

linked_hashtbl_test.pg: linked_hashtbl_test.c linked_hashtbl.c
	$(CC) $(CFLAGS) $(PROFILE_FLAGS) \
		-DLINKED_HASHTBL_MAX_TABLE_SIZE='(1<<8)' \
		-pg -g \
		 -o $@ linked_hashtbl_test.c linked_hashtbl.c
	./$@
	gprof -s

hashtbl_test.pg: hashtbl_test.c hashtbl.c
	$(CC) $(CFLAGS) $(PROFILE_FLAGS) \
		-DHASHTBL_MAX_TABLE_SIZE='(1<<8)' \
		-pg -g \
		 -o $@ hashtbl_test.c hashtbl.c
	./$@
	gprof -s

clean:
	$(RM) linked_hashtbl_test.pg linked_hashtbl_test.gcov linked_hashtbl_test
	$(RM) hashtbl_test.pg hashtbl_test.gcov hashtbl_test
	$(RM) -r *.o *.a *.d *.gcda *.gcov *.pg *.gcno

*.o : Makefile
*.h : Makefile

linked_hashtbl_test: linked_hashtbl_test.c CUnitTest.h linked_hashtbl.h Makefile

.PHONY: TAGS

TAGS:
	etags *.c *.h
