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
#include <assert.h>

#include "ptp.h"
#include "murmurhash3.h"


#define HASH_SEED    (0x5ca1ab1e)


/* Holds the output FILE pointers.  */
struct fileinfo {
    unsigned int numfiles;
    FILE ** files;
};


void printusage()
{
    fputs(
        "Usage: hsplit [OPTION]... [FILE]...\n"
        "Distribute lines of standard input evenly among FILE(s), such that identical\n"
        "lines end up in the same FILE.\n\n"

        "Lines in any particular output FILE will have the same order they did in the\n"
        "input.  hsplit does not add a final newline if the input lacks one.\n\n"

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


/** Given a buffer of one or more lines, hash each line and write it to the appropriate file.
 * info is actually a struct fileinfo.  If info.numfiles == 0, write the hashcode to
 * stdout instead.  buflen is guaranteed to be at least 1, and buf will end with
 * a newline or the last byte of the file.
 */
void split_lines_to_files(char * buf, size_t buflen, void * info)
{
    const char * line = buf;
    const char * bufend = buf + buflen;
    const char * newline;
    struct fileinfo fileinfo = *(struct fileinfo *) info;

    assert(buflen > 0);
    while (line < bufend)
    {
        newline = memchr(line, '\n', bufend - line);
        if (newline == NULL)
            newline = bufend - 1;       // We add one later when writing to include the newline in the usual case.
        uint32_t hashcode = hash(line, newline - line);

        if (fileinfo.numfiles > 0)
        {
            unsigned int filenum = hash2filenum(hashcode, fileinfo.numfiles);
            // fwrite(3) is faster than write(2) here because we're only writing a line at a time, so fwrite's buffering helps.
            if (fwrite(line, 1, newline - line + 1, fileinfo.files[filenum]) < newline - line + 1)
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
        line = newline + 1;
    }
}


/**
 * Hash lines from stdin to files given on command line.
 */
int main(int argc, char * argv[])
{
    int append = 0;
    int first_filename_arg = 1;
    process_lines_context ctx;
    struct fileinfo fileinfo;

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

    fileinfo.numfiles = argc - first_filename_arg;
    if (fileinfo.numfiles == 0 && append)
    {
        fprintf(stderr, "Can only use --append with files.");
        exit(1);
    }

    fileinfo.files = calloc(fileinfo.numfiles, sizeof (FILE *));
    if (fileinfo.numfiles > 0 && fileinfo.files == NULL)
    {
        perror("hsplit: Error allocating memory");
        exit(1);
    }

    /* Open files. */
    for (unsigned int f = 0 ; f < fileinfo.numfiles ; f++)
    {
        const char * filename = argv[first_filename_arg + f];
        fileinfo.files[f] = fopen(filename, append ? "a" : "w");
        if (fileinfo.files[f] == NULL)
        {
            fprintf(stderr, "hsplit: error opening \"%s\"", filename);
            perror("");
            exit(1);
        }
    }

    if (process_lines_init(&ctx, fileno(stdin), split_lines_to_files, &fileinfo) != 0)
    {
        perror("hsplit");
        exit(1);
    }

    /* Loop: read a line, hash to get file number, write. */
    // TODO: If our hash function had an incremental interface, we could probably do this faster, without buffering.
    int result;
    do {
        result = process_lines(&ctx);
    } while (result == 0);
    if (result > 0)
    {
        perror("hsplit");
        exit(1);
    }

    /* Close files and clean up. */
    process_lines_cleanup(&ctx);
    for (int f = 0 ; f < fileinfo.numfiles ; f++)
    {
        if (fclose(fileinfo.files[f]) != 0)
        {
            perror("hsplit: Error closing file");
            exit(1);
        }
    }
    free(fileinfo.files);

    return 0;
}
