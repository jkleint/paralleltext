/****************************************************************************
 Parallel Text Processing -- Common Routines

 Copyright 2011 John Kleint

 This is free software, licensed under the GNU General Public License v3,
 available in the accompanying file LICENSE.txt.

 See ptp.h for documentation.
****************************************************************************/

#define _GNU_SOURCE             // For memrchr(3)

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>

#include "ptp.h"


int process_lines_init(process_lines_context * ctx, int fd, void (*process)(char * buf, size_t buflen, void * info), void * info)
{
    ctx->bufsize = ctx->readsize = READ_SIZE_BYTES;
    ctx->buf = calloc(ctx->bufsize, 1);
    if (ctx->buf == NULL)
        return PTP_ERR_ALLOC;
    ctx->bufpos = 0;
    ctx->process = process;
    ctx->fd = fd;
    ctx->info = info;
    return 0;
}


/*
 * We want to process only whole lines.  Lines can be arbitrarily long,
 * and we may not get a whole line in one read().  So, we have to manage a
 * growable buffer.
 *
 * The basic idea is to read data from a file descriptor, process any whole
 * lines, and save any trailing partial line in a buffer.  If we don't yet
 * have a whole line, we don't call process() and the data is buffered.
 *
 * buf will always hold the last partial line read (at offset 0).
 * The last partial line is bufpos bytes long (it can be zero, i.e., no
 * partial line).  We try to read readsize bytes into buf starting at bufpos;
 * if it won't fit, we double the size of buf until it does.  Once we've read
 * in the data, we scan it in reverse, looking for the last newline.  We
 * process() everything up to and including the last newline.  We then copy
 * any trailing partial line to the beginning of the buffer.
 */
int process_lines(process_lines_context * ctx)
{
    // We copy these into local vars for readability
    char * buf = ctx->buf;
    size_t bufsize = ctx->bufsize;
    size_t bufpos = ctx->bufpos;
    const size_t readsize = ctx->readsize;

    assert(bufsize > 0);
    assert(bufpos <= bufsize);
    /* Ensure buf has space for readsize more bytes. */
    if (bufsize - bufpos < readsize)
    {
        while (bufsize - bufpos < readsize)
            bufsize *= 2;

        buf = realloc(buf, bufsize);
        if (buf == NULL)
        {
            return PTP_ERR_ALLOC;
        }
        ctx->buf = buf;                 // We update ctx here because this function has several exit points
        ctx->bufsize = bufsize;
    }


    /* Read; if EOF, call final process and return -1. */
    ssize_t bytesread = read(ctx->fd, buf + bufpos, bufsize - bufpos);
    debug(1, "read on fd %d returned %d\n", ctx->fd, bytesread);
    if (bytesread < 0)              // Error
    {
        return PTP_ERR_READ;
    }
    else if (bytesread == 0)       // EOF
    {
        if (bufpos > 0)
        {
            ctx->process(buf, bufpos, ctx->info);
        }
        return PTP_EOF;
    }

    /* bytesread > 0
    Find last newline, call process; copy any trailing partial line to start of buf.
    If no newline, just update bufpos. */
    char * last_newline = memrchr(buf + bufpos, '\n', (size_t) bytesread);
    if (last_newline != NULL)
    {
        char * partial_line = last_newline + 1;
        ctx->process(buf, partial_line - buf, ctx->info);
        size_t bytes_remaining = (buf + bufpos + bytesread) - partial_line;
        memmove(buf, partial_line, bytes_remaining);
        bufpos = bytes_remaining;
    }
    else        // No newline found
    {
        bufpos += bytesread;
    }

    ctx->bufpos = bufpos;
    return 0;
}


int process_lines_cleanup(process_lines_context * ctx)
{
    free(ctx->buf);
    ctx->buf = NULL;
    ctx->bufsize = 0;
    ctx->readsize = 0;
    ctx->process = NULL;
    ctx->fd = -1;
    ctx->info = NULL;
    return 0;
}


/* Print a formatted debugging message to stderr. */
void _debug(__attribute__((unused)) unsigned int level, const char * format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
}

