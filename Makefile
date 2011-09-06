# Parallel text processing tools Makefile

# Copyright 2011 John Kleint 
# This is free software, licensed under the GNU General Public License v3,
# available in the accompanying file LICENSE.txt.

CC=gcc
CFLAGS=-Wall -std=c99 -O3

.PHONY: clean all test

all: bin/pcat bin/hsplit

bin:
	mkdir bin

bin/pcat: bin src/pcat.c bin/ptp.o
	$(CC) $(CFLAGS) src/pcat.c bin/ptp.o -o bin/pcat

bin/hsplit: bin src/hsplit.c bin/ptp.o bin/murmurhash3.o
	$(CC) $(CFLAGS) src/hsplit.c bin/murmurhash3.o bin/ptp.o -o bin/hsplit

bin/ptp.o: bin src/ptp.[ch]
	$(CC) $(CFLAGS) -c src/ptp.c -o bin/ptp.o

bin/murmurhash3.o: bin src/murmurhash3.[ch]
	$(CC) $(CFLAGS) -c src/murmurhash3.c -o bin/murmurhash3.o

test: bin/pcat bin/hsplit
	test/test-pcat.sh
	test/test-hsplit.sh

clean:
	rm -rf bin
