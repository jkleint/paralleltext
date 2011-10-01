/****************************************************************************
 Parallel Text Processing -- Common Routines

 Copyright 2011 John Kleint

 This is free software, licensed under the GNU General Public License v3,
 available in the accompanying file LICENSE.txt.
*****************************************************************************/

#ifndef PTP_H_
#define PTP_H_

#include <stdlib.h>

#define READ_SIZE_BYTES (64*1024)
#define PTP_EOF         (-1)
#define PTP_ERR_ALLOC   (1)
#define PTP_ERR_READ    (2)


typedef struct {
    char * buf;
    size_t bufsize;
    size_t bufpos;
    size_t readsize;
    void (*process)(char * buf, size_t buflen, void * info);
    void * info;
    int fd;
} process_lines_context;

/**
 * Initialize a process_lines_context struct to process lines from the given (open) file
 * descriptor fd with the process function.  info can be any information the function needs
 * to do its job.  Returns nonzero on error.
 */
int process_lines_init(process_lines_context * ctx, int fd, void (*process)(char * buf, size_t buflen, void * info), void * info);


/**
 * Generic, efficient line-oriented file processing.
 *
 * First, write your custom processing function with the signature
 *
 *     void process(char * buf, size_t buflen, void * info)
 *
 * It gets passed a buffer containing one or more whole lines (including newlines)
 * and the buffer's length. The buffer is guaranteed to contain at least one byte,
 * and will either end with a newline or the last byte of the file.  info can be a
 * pointer to anything an instance of your function needs.

 * Then, call process_lines_init() with the (open) file descriptor you'd like to
 * read from, your line processing function, and a pointer to any information
 * your function may need.
 *
 * Loop calling process_lines() with the context struct until it returns nonzero.
 * -1 means EOF and positive means error (see the PTP_ERR_* codes).
 *
 * Each call to process_lines() results in exactly one call to read(2); this
 * makes it useful in conjunction with poll(2), select(2), and the like.  If
 * any newlines were read, it will call your process() function exactly once;
 * otherwise, process() will not be called.
 *
 * Finally, call process_lines_cleanup() (note that this does not close the file
 * descriptor).
 */
int process_lines(process_lines_context * ctx);


/**
 * Free the internal process_lines buffer.  Does not close the file descriptor
 * it was initialized with.
 */
int process_lines_cleanup(process_lines_context * ctx);


// There may be a more clever way to do this.
#define debug(level, ...) _DEBUG##level(level, __VA_ARGS__)

#if DEBUG >= 1
#define _DEBUG1(level, ...) _debug(level, __VA_ARGS__)
#else
#define _DEBUG1(level, ...)
#endif

#if DEBUG >= 2
#define _DEBUG2(level, ...) _debug(level, __VA_ARGS__)
#else
#define _DEBUG2(level, ...)
#endif

#if DEBUG >= 3
#define _DEBUG3(level, ...) _debug(level, __VA_ARGS__)
#else
#define _DEBUG3(level, ...)
#endif


/* Print a formatted debugging message to stderr. */
void _debug(unsigned int level, const char * msg, ...);


#endif /* PTP_H_ */
