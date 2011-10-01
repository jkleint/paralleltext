/****************************************************************************
 pcat - Parallel Cat

 Copyright 2011 John Kleint
 This is free software, licensed under the GNU General Public License v3,
 available in the accompanying file LICENSE.txt.
 *****************************************************************************/

#define _XOPEN_SOURCE        	// For fileno(3)
#define _GNU_SOURCE            	// For memrchr(3)

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>
#include <limits.h>

#include "ptp.h"

#define INFINITE_TIMEOUT (-1)
#define MAX(a,b)    ((a) > (b) ? (a) : (b))


void printusage()
{
    fputs(
        "Usage: pcat [OPTION]... [FILE]...\n"
        "Read FILE(s) in parallel and write whole lines to standard output.\n\n"

        "pcat allows you to combine the output from multiple concurrent processes\n"
        "while keeping whole lines intact: each output line is guaranteed to be\n"
        "an entire line from exactly one input.  The relative order of lines from\n"
        "any particular file is preserved (i.e., lines from the same file stay in \n"
        "order), but lines from each FILE will be mixed together arbitrarily.\n"
        "To ensure each output line comes from only one input, pcat will add a\n"
        "final newline to any file that ends without one.\n"

        "  -h,  --help                display this help and exit\n"
        "  -c,  --continue-on-error   continue processing other FILEs if one has an error\n\n"

        "With no FILE, or when FILE is -, read standard input (like cat(1)).\n\n",
        stderr);
}


/** write() buf to stdout. */
void writelines(char * buf, size_t buflen, __attribute__ ((unused)) void * info)
{
    assert(buflen > 0);
    assert(buflen < SSIZE_MAX);
    if (write(fileno(stdout), buf, buflen) < (ssize_t) buflen)
    {
        perror("pcat: write()");
        exit(1);
    }

    // If last line has no newline, write one.
    if (buf[buflen - 1] != '\n' && write(fileno(stdout), "\n", 1) < 1)
    {
        perror("pcat: write()");
        exit(1);
    }
}


/* Basic idea: open each given file for reading, then loop poll()ing,
 * writing each file's lines to stdout.  Since we may not get a complete
 * line at a time, we have to buffer each file until we see a newline.  */
int main(int argc, char * argv[])
{
    int continue_on_errors = 0;
    unsigned int first_filename_arg = 1;

    if (argc == 2 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0))
    {
        printusage();
        exit(0);
    }

    if (argc > 1 && strcmp(argv[1], "-c") == 0)
    {
        continue_on_errors = 1;
        first_filename_arg = 2;
    }

    unsigned int numfiles = MAX(argc - first_filename_arg, 1);          // If no filenames given, still have stdin
    process_lines_context contexts[numfiles];                           // C99 variable-length arrays -- ooh-la-la!
    struct pollfd pollfds[numfiles];

    /* Open files; set up poll structs and buffers. */
    debug(1, "opening %d file(s)\n", numfiles);
    for (unsigned int f = 0 ; f < numfiles; f++)
    {
        const char * filename = argc > 1 ? argv[first_filename_arg + f] : "-";
        if (strcmp(filename, "-") == 0)
            pollfds[f].fd = fileno(stdin);
        else
            pollfds[f].fd = open(filename, O_RDONLY);
        debug(1, "open(\"%s\") as fd %d\n", filename, pollfds[f].fd);
        pollfds[f].events = POLLIN;
        if (pollfds[f].fd < 0)
        {
            fprintf(stderr, "pcat: Error opening '%s'", filename);
            perror("");
            exit(1);
        }

        if (process_lines_init(contexts + f, pollfds[f].fd, writelines, NULL) != 0)
        {
            perror("pcat: Error initializing processing context");
            exit(1);
        }
    }

    /* Loop polling files, writing data to stdout, until we've finished reading them all. */
    unsigned int numfiles_remaining = numfiles;
    while (numfiles_remaining > 0)
    {
        debug(2, "polling %d file(s)\n", numfiles_remaining);
        int numready = poll(pollfds, numfiles, INFINITE_TIMEOUT);
        debug(2, "poll gave %d ready file(s)\n", numready);

        if (numready == 0)
        {
            fprintf(stderr, "pcat: poll timed out.\n");
            exit(1);
        }
        else if (numready < 0)
        {
            perror("pcat: poll");
            exit(1);
        }

        // Loop reading ready files, writing input to stdout.
        for (unsigned int f = 0 ; f < numfiles ; f++)
        {
            if (!pollfds[f].events)        // If we're not listening for this file, skip it.
                continue;
            if (pollfds[f].revents & (POLLERR | POLLNVAL))        // Error
            {
                pollfds[f].events = 0;
                numfiles_remaining--;
                fprintf(stderr, "pcat: %s%s%spolling fd %d.\n",
                		pollfds[f].revents & POLLERR ? "POLLERR " : "",
                		pollfds[f].revents & POLLHUP ? "POLLHUP " : "",
                		pollfds[f].revents & POLLNVAL ? "POLLNVAL " : "",
            			pollfds[f].fd);
                process_lines_cleanup(contexts + f);
                if (!continue_on_errors)
                    exit(1);
            }
            // On Linux and Solaris, pipes give POLLHUP instead of POLLIN on EOF, so we have to check for both.
            else if (pollfds[f].revents & (POLLIN | POLLHUP))        // Data or EOF available
            {
                debug(3, "processing data from fd %d\n", pollfds[f].fd);
                int result = process_lines(contexts + f);
                if (result != 0)        	// EOF or Error
                {
                    pollfds[f].events = 0;
                    numfiles_remaining--;
                    debug(2, "cleaning up fd %d: got %d from process_lines\n", pollfds[f].fd, result);
                    process_lines_cleanup(contexts + f);
                    if (result > 0)        	// Error
                    {
                        fprintf(stderr, "pcat: Error reading from fd %d.\n", pollfds[f].fd);
                    }
                    debug(2, "closing fd %d\n", pollfds[f].fd);
                    if (close(pollfds[f].fd) != 0)
                    {
                        perror("pcat: close()");
                        if (!continue_on_errors)
                            exit(1);
                    }
                }
            }
            else if (pollfds[f].revents != 0)
            {
                fprintf(stderr, "pcat: Unknown result from poll on fd %d: %d\n", f, pollfds[f].revents);
                exit(1);
            }
        }
    }

    debug(1, "Success.\n");
    return 0;
}
