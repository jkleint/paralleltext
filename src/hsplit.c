/****************************************************************************
 hsplit - Hash Split

 Copyright 2011 John Kleint
 This is free software, licensed under the GNU General Public License v3,
 available in the accompanying file LICENSE.txt.

 Hash lines from stdin to multiple files, such that the same line always
 goes to the same file.
 ****************************************************************************/

#define _XOPEN_SOURCE 700        // For getline(3)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include "murmurhash3.h"


#define HASH_SEED    (0x5ca1ab1e)


void printusage()
{
    fputs(
        "Usage: hsplit [OPTION]... [FILE]...\n"
        "Distribute lines of standard input evenly among FILE(s), such that identical\n"
        "lines end up in the same FILE.\n\n"

        "With no FILE(s), print the 32-bit unsigned integer hash code for each input\n"
        "line to standard output.\n\n"

        "  -h,  --help                display this help and exit\n"
        "  -a,  --append              append to FILE(s) rather than overwrite\n"

        "\n",
        stderr);
}


/* Hash a buffer of length `len` to a 32-bit integer.
 * MurmurHash3 is used because it's fast, simple, public domain, and provides a 32-bit variant.
 */
uint32_t hash(const void * buf, size_t len)
{
    uint32_t hashcode;

    if (len > INT_MAX)
    {
        fprintf(stderr, "Maximum line length (%d) exceeded: %zu.\n", INT_MAX, len);
        exit(1);
    }
    MurmurHash3_x86_32(buf, (int) len, HASH_SEED, &hashcode);

    return hashcode;
}


/** Convert a 32-bit integer hashcode to a file number in the range [0, numfiles). */
unsigned int hash2filenum(uint32_t hashcode, unsigned int numfiles)
{
    return (unsigned int) (((double) hashcode) / (UINT32_MAX + 1.0) * numfiles);
}


/**
 * Hash lines from stdin to files given on command line.
 */
int main(int argc, char * argv[])
{
    int append = 0;
    int first_filename_arg = 1;
    size_t linebuf_size = -1;
    ssize_t bytesread;
    char * linebuf = NULL;

    if (argc >= 2 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0))
    {
        printusage();
        exit(0);
    }

    if (argc > 1 && ((strcmp(argv[1], "-a") == 0) || (strcmp(argv[1], "--append") == 0)))
    {
        append = 1;
        first_filename_arg = 2;
    }

    unsigned int numfiles = argc - first_filename_arg;
    if (numfiles == 0 && append)
    {
        fprintf(stderr, "Can only use --append with files.");
        exit(1);
    }

    FILE ** files = calloc(numfiles, sizeof (FILE *));
    if (numfiles > 0 && files == NULL)
    {
        perror("hsplit: Error allocating memory");
        exit(1);
    }

    /* Open files. */
    for (unsigned int f = 0 ; f < numfiles ; f++)
    {
        const char * filename = argv[first_filename_arg + f];
        files[f] = fopen(filename, append ? "a" : "w");
        if (files[f] == NULL)
        {
            fprintf(stderr, "hsplit: error opening \"%s\"", filename);
            perror("");
            exit(1);
        }
    }

    // TODO: If murmurhash had an incremental interface, we could probably do this faster, with less memory.
    /* Loop: read a line, hash to get file number, write. */
    while ((bytesread = getline(&linebuf, &linebuf_size, stdin)) >= 0)
    {

        uint32_t hashcode = hash(linebuf, bytesread);

        if (numfiles > 0)
        {
            unsigned int filenum = hash2filenum(hashcode, numfiles);
            if (fwrite(linebuf, 1, bytesread, files[filenum]) < bytesread)
            {
                fprintf(stderr, "hsplit: Error writing to file %d", filenum);
                perror("");
                exit(1);
            }
        }
        else
        {
            printf("%u\n", hashcode);
        }
    }

    /* Close files. */
    for (int f = 0 ; f < numfiles ; f++)
    {
        if (fclose(files[f]) != 0)
        {
            perror("hsplit: Error closing file");
            exit(1);
        }
    }
    free(linebuf);
    free(files);

    return 0;
}
