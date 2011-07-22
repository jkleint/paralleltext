# Parallel text processing tools Makefile

# Copyright 2011 John Kleint 
# This is free software, licensed under the GNU General Public License v3,
# available in the accompanying file LICENSE.txt.

CC=gcc
CFLAGS=-Wall -std=c99 -O2

.PHONY: clean all test

all: bin/pcat

bin:
	mkdir bin

bin/pcat: bin src/pcat.c
	$(CC) $(CFLAGS) src/pcat.c -o bin/pcat

test: bin/pcat
	test/test-pcat.sh

clean:
	rm -rf bin
