/****************************************************************************
 ptp.c
 jk, Aug 27, 2011
 ****************************************************************************/

#define _GNU_SOURCE             // For memrchr(3)

// #include <stdlib.h>
#include <unistd.h>
#include <string.h>

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


int process_lines(process_lines_context * ctx)
{
    char * buf = ctx->buf;
    size_t bufsize = ctx->bufsize;
    size_t bufpos = ctx->bufpos;
    size_t readsize = ctx->readsize;

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

    // bytesread > 0
    /* Find last newline, call process; copy any trailing partial line to start of buf.
     * If no newline, just update bufpos. */
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

    ctx->buf = buf;
    ctx->bufsize = bufsize;
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
void debug(int level, const char * msg, ...)
{
    #ifdef DEBUG
    va_list args;
    va_start(args, msg);
    vfprintf(stderr, msg, args);
    #endif
}

