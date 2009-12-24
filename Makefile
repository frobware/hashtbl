# Copyright (c) 2009 <Andrew Iain McDermott @ gmail.com>
#
# Source can be cloned from:
#
#   git://github.com/aim-stuff/hashtbl.git
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
COMMON_CFLAGS += -Wall -Wextra
COMMON_CFLAGS += -Wformat -Wmissing-prototypes -Wpointer-arith
COMMON_CFLAGS += -Wuninitialized -O
COMMON_CFLAGS += -Wsign-compare -Wshadow
COMMON_CFLAGS += -pedantic
CFLAGS        += $(COMMON_CFLAGS)
CFLAGS        += -g -fno-inline
#CFLAGS        += -O3 -DNDEBUG
VALGRIND       = valgrind --quiet --leak-check=full

all : hashtbl_test
ifeq ($(shell uname -s),Linux)
	@echo $(VALGRIND) ./hashtbl_test
	@$(VALGRIND) ./hashtbl_test
else
	./hashtbl_test
endif

hashtbl_test: hashtbl_test.c hashtbl.c
	$(CC) $(CFLAGS) -DHASHTBL_MAX_TABLE_SIZE='((1<<8))' -o $@ hashtbl.c hashtbl_test.c

.PHONY: hashtbl_test.gcov

hashtbl_test.gcov: hashtbl_test.c hashtbl.c
	$(CC) $(CFLAGS) $(PROFILE_FLAGS) -g -o $@ hashtbl_test.c hashtbl.c
	./$@
	gcov $^

.PHONY : hashtbl_test.pg

hashtbl_test.pg: hashtbl_test.c hashtbl.c
	$(CC) $(CFLAGS) $(PROFILE_FLAGS) \
		-fwhole-program -combine -pg \
		-O3 -g \
		 -o $@ hashtbl_test.c hashtbl.c

clean:
	$(RM) -r *.o *.a *.d hashtbl_test.pg hashtbl_test.gcov hashtbl_test *.gcda *.gcov *.pg *.gcno

*.o : Makefile
hashtbl_test : Makefile

hashtbl.c : hashtbl.h
hashtbl_test.c : hashtbl.c
hashtbl_test.c : hashtbl.h

hashtbl.o : hashtbl.c hashtbl.h
hashtbl_test.o: hashtbl_test.c CUnitTest.h hashtbl.h
