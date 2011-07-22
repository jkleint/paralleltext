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
#include <stdarg.h>


#define INFINITE_TIMEOUT (-1)
#define MAX(a,b)    ((a) > (b) ? (a) : (b))


void printusage()
{
    fputs(
        "Usage: pcat [OPTION]... [FILE]...\n"
        "Read FILE(s) in parallel and write whole lines to standard output.\n\n"

        "pcat allows you to combine the output from multiple concurrent processes\n"
        "while keeping individual lines intact: each output line is guaranteed to be\n"
        "an entire line from exactly one input.  The ordering of output lines is\n"
        "arbitrary; in general, lines from all FILEs will be mixed together.\n\n"

        "  -h,  --help                display this help and exit\n"
      //"  -c,  --continue-on-error   continue processing other FILEs if one has an error\n\n"

        "With no FILE, or when FILE is -, read standard input (like cat(1)).\n\n",
        stderr);
}


/* Print a formatted debugging message to stderr. */
void debug(int level, const char * msg, ...)
{
    #ifdef DEBUG
    va_list args;
    va_start(args, msg);
    vfprintf(stderr, msg, args);
    #endif
}


/*
 * Read data from file descriptor fd, print whole lines to stdout, save any
 * trailing partial line in buf.
 *
 * We want to print only whole lines to stdout.  Lines can be arbitrarily long,
 * and we may not get a whole line in one read().  So, we have to manage a
 * growable buffer.
 *
 * buf will always hold the last partial line read, starting at offset 0.  The last
 * partial line is bufpos bytes long (it can be zero, i.e., no partial line).  We
 * try to read readbufsize bytes into buf starting at bufpos; if it won't fit,
 * we double the size of buf until it does.  Once we've read in the data,
 * we scan it in reverse, looking for the last newline.  We print everything up to
 * and including the last newline.  We then copy any trailing partial line to the
 * beginning of the buffer.
 *
 * print_lines() returns zero if everything went OK, -1 on EOF, and 1 on error.
 * We pass pointers to buf, bufsize, and bufpos because this routine modifies
 * them.
 */
int print_lines(int fd, size_t readbufsize, char ** buf, size_t * bufsize, size_t * bufpos)
{
    if (*bufsize - *bufpos < readbufsize)
    {
        while (*bufsize - *bufpos < readbufsize)
            *bufsize *= 2;

        *buf = realloc(*buf, *bufsize);
        if (*buf == NULL)
        {
            perror("pcat: realloc");
            exit(1);
        }
    }

    ssize_t bytesread = read(fd, *buf + *bufpos, *bufsize - *bufpos);
    debug(1, "read on fd %d returned %d\n", fd, bytesread);
    if (bytesread < 0)        // Error
        return 1;
    if (bytesread == 0)       // EOF
    {
        if (*bufpos > 0 && write(fileno(stdout), *buf, *bufpos) < 0)
        {
            perror("pcat: write()");
            exit(1);
        }
        *bufpos = 0;
        return -1;
    }

    char * last_newline = memrchr(*buf + *bufpos, '\n', (size_t) bytesread);
    if (last_newline != NULL)
    {
        char * partial_line = last_newline + 1;
        if (write(fileno(stdout), *buf, partial_line - *buf) < 0)
        {
            perror("pcat: write()");
            exit(1);
        }
        size_t bytes_remaining = *buf + *bufpos + bytesread - partial_line;
        memmove(*buf, partial_line, bytes_remaining);
        *bufpos = bytes_remaining;
    }
    else        // No newline found
    {
        *bufpos += bytesread;
    }

    return 0;
}


/* Basic idea: open each given file for reading, then loop poll()ing,
 * writing each file's lines to stdout.  Since we may not get a complete
 * line at a time, we have to buffer until we see a newline.
 */
int main(int argc, char* argv[])
{
    int continue_on_errors = 0;
    unsigned int first_filename_arg = 1;
    unsigned int numfiles = MAX(argc - first_filename_arg, 1);        	// If no filenames given, still have stdin
    unsigned int initial_buffer_size = 4096;
    struct pollfd * fds = calloc(numfiles, sizeof(struct pollfd));
    char ** buffers = calloc(numfiles, sizeof(char *));                	// Holds trailing partial line for each input
    size_t * buffer_sizes = calloc(numfiles, sizeof(size_t));        	// Size of each input buffer
    size_t * buffer_positions = calloc(numfiles, sizeof(size_t));    	// Length of partial line in each buffer


    if (argc == 2 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0))
    {
        printusage();
        exit(0);
    }

    if (fds == NULL || buffers == NULL || buffer_sizes == NULL || buffer_positions == NULL)
    {
        perror("Error allocating memory");
        exit(1);
    }

    /* Open files; set up poll structs and buffers. */
    debug(1, "opening %d file(s)\n", numfiles);
    for (unsigned int f = 0 ; f < numfiles; f++)
    {
        const char * filename = argc > 1 ? argv[first_filename_arg + f] : "-";
        if (strcmp(filename, "-") == 0)
            fds[f].fd = fileno(stdin);
        else
            fds[f].fd = open(filename, O_RDONLY);
        debug(1, "open(\"%s\") as fd %d\n", filename, fds[f].fd);
        fds[f].events = POLLIN;
        if (fds[f].fd < 0)
        {
            fprintf(stderr, "Error opening '%s'", filename);
            perror("");
            exit(1);
        }

        buffer_sizes[f] = initial_buffer_size;
        buffers[f] = calloc(initial_buffer_size, sizeof(char));
        if (buffers[f] == NULL)
        {
            perror("Error allocating buffer");
            exit(1);
        }
    }

    /* Loop polling files until we've finished reading them all. */
    unsigned int numfiles_remaining = numfiles;
    while (numfiles_remaining > 0)
    {
        debug(2, "polling %d file(s)\n", numfiles_remaining);
        unsigned int numready = poll(fds, numfiles, INFINITE_TIMEOUT);
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

        for (unsigned int f = 0 ; f < numfiles ; f++)
        {
            if (!fds[f].events)        // If we're not listening for this file, skip it.
                continue;
            if (fds[f].revents & (POLLERR | POLLNVAL))        // Error
            {
                fds[f].events = 0;
                numfiles_remaining--;
                fprintf(stderr, "pcat: %s%s%spolling fd %d.\n",
                		fds[f].revents & POLLERR ? "POLLERR " : "",
                		fds[f].revents & POLLHUP ? "POLLHUP " : "",
                		fds[f].revents & POLLNVAL ? "POLLNVAL " : "",
            			fds[f].fd);
                free(buffers[f]);
                if (!continue_on_errors)
                    exit(1);
            }
            // On Linux and Solaris, pipes give POLLHUP instead of POLLIN on EOF, so we have to check for both.
            else if (fds[f].revents & (POLLIN | POLLHUP))        // Data or EOF available
            {
                int result = print_lines(fds[f].fd, initial_buffer_size, &buffers[f], &buffer_sizes[f], &buffer_positions[f]);
                if (result != 0)        	// EOF or Error
                {
                    fds[f].events = 0;
                    numfiles_remaining--;
                    free(buffers[f]);
                    if (result > 0)        	// Error
                    {
                        fprintf(stderr, "Error reading from fd %d.\n", fds[f].fd);
                    }
                    if (close(fds[f].fd) != 0)
                    {
                        perror("pcat: close()");
                        if (!continue_on_errors)
                            exit(1);
                    }
                }
            }
            else if (fds[f].revents != 0)
            {
                fprintf(stderr, "Unknown result from poll on fd %d: %d\n", f, fds[f].revents);
                exit(1);
            }
        }
    }

    debug(1, "Success.\n");
    return 0;
}
