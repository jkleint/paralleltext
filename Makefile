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

bin/pcat: bin src/pcat.c
	$(CC) $(CFLAGS) src/pcat.c -o bin/pcat

bin/hsplit: bin src/hsplit.c src/murmurhash3.c src/murmurhash3.h
	$(CC) $(CFLAGS) src/hsplit.c src/murmurhash3.c -o bin/hsplit

test: bin/pcat bin/hsplit
	test/test-pcat.sh
	test/test-hsplit.sh

clean:
	rm -rf bin
