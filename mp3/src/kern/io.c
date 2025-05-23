//           io.c 
//          

#include "io.h"
#include "string.h"
#include "error.h"

#include <stddef.h>
#include <stdint.h>

//           INTERNAL TYPE DEFINITIONS
//          

struct iovprintf_state {
    struct io_intf * io;
    int err;
};

//           INTERNAL FUNCTION DECLARATIONS
//          
long iolit_read(struct io_intf * io, void * buf, unsigned long bufsz);
long iolit_write(struct io_intf * io, const void * buf, unsigned long n);
int iolit_ctl(struct io_intf * io, int cmd, void * arg);

static void ioterm_close(struct io_intf * io);
static long ioterm_read(struct io_intf * io, void * buf, size_t len);
static long ioterm_write(struct io_intf * io, const void * buf, size_t len);
static int ioterm_ioctl(struct io_intf * io, int cmd, void * arg);

static void iovprintf_putc(char c, void * aux);

//           EXPORTED FUNCTION DEFINITIONS
//          

long ioread_full(struct io_intf * io, void * buf, unsigned long bufsz) {
    long cnt, acc = 0;

    if (io->ops->read == NULL)
        return -ENOTSUP;

    while (acc < bufsz) {
        cnt = io->ops->read(io, buf+acc, bufsz-acc);
        if (cnt < 0)
            return cnt;
        else if (cnt == 0)
            return acc;
        acc += cnt;
    }

    return acc;
}

long iowrite(struct io_intf * io, const void * buf, unsigned long n) {
    long cnt, acc = 0;

    if (io->ops->write == NULL)
        return -ENOTSUP;

    while (acc < n) {
        cnt = io->ops->write(io, buf+acc, n-acc);
        if (cnt < 0)
            return cnt;
        else if (cnt == 0)
            return acc;
        acc += cnt;
    }

    return acc;
}

//           Initialize an io_lit. This function should be called with an io_lit, a buffer, and the size of the device.
//           It should set up all fields within the io_lit struct so that I/O operations can be performed on the io_lit
//           through the io_intf interface. This function should return a pointer to an io_intf object that can be used 
//           to perform I/O operations on the device.

// iolit operations
struct io_ops iolit_ops = {
    .read = iolit_read,
    .write = iolit_write,
    .ctl = iolit_ctl,
    .close = NULL
};

/**iolit_init
 * initializes iolit.
 * input: lit -- the iolit
 *        buf -- a buffer
 *        size -- size of the device
 * output: an initialized iolit
 */
struct io_intf * iolit_init (
    struct io_lit * lit, void * buf, size_t size)
{
    // Implement me!
    lit->buf = buf;
    lit->size = size;
    lit->pos = 0;
    
    lit->io_intf.ops = &iolit_ops;
    // Initialize the reference count to 1 when created
    lit->io_intf.refcnt = 1;

    return &lit->io_intf;
}

/** iolit_read
 *  Reads data from an in-memory buffer represented by io_lit into buf up to bufsz bytes
 * */
long iolit_read(struct io_intf* io, void* buf, unsigned long bufsz) {
    // Cast the generic io_intf pointer to io_lit type
    struct io_lit* lit = (struct io_lit*) io;

    // Check if the buffer pointer is NULL to avoid dereferencing invalid memory
    if (buf == NULL)
        return -EINVAL;

    // If the current position has reached the end of the buffer, return 0 to indicate EOF
    if (lit->pos >= lit->size)
        return 0;

    // If the requested read size exceeds the remaining data, recursively call with adjusted size
    if ((bufsz + lit->pos) > lit->size) {
        return iolit_read(io, buf, lit->size - lit->pos);
    } else {
        // Initialize pointers to the destination buffer and the internal buffer
        char* buffer = (char*) buf;
        char* lit_buffer = lit->buf;

        // Copy data from the internal buffer to the output buffer
        for (uint32_t i = 0; i < bufsz; i++) {
            *buffer = lit_buffer[lit->pos]; // Copy the current byte and increment the position
            lit->pos++;
            buffer++;
        }

        // Return the number of bytes read (equal to bufsz)
        return bufsz;
    }
}

/**iolit_write
 * Writes data from buf to an in-memory buffer represented by io_lit, up to n bytes
 */
long iolit_write(struct io_intf* io, const void* buf, unsigned long n) {
    // Cast the generic io_intf pointer to io_lit type.
    struct io_lit* lit = (struct io_lit*) io;

    // Check if the input buffer is NULL to prevent invalid memory access
    if (buf == NULL)
        return -EINVAL;

    // If the current position has reached the end of the buffer, return 0 indicating no space left
    if (lit->pos >= lit->size)
        return 0;

    // Adjust n if it exceeds the space available from the current position to the end of the buffer
    if ((n + lit->pos) > lit->size) {
        return iolit_write(io, buf, lit->size - lit->pos);
    } else {
        // Initialize pointers for reading from the input buffer and writing to the internal buffer
        const char* buffer = (const char*) buf;
        char* lit_buffer = lit->buf;

        // Copy data byte by byte from the input buffer to the internal buffer
        for (uint32_t i = 0; i < n; i++) {
            lit_buffer[lit->pos] = *buffer; // Copy the current byte and increment the position
            lit->pos++;
            buffer++;
        }

        // Return the number of bytes written (equal to n)
        return n;
    }
}


/**
 * iolit_ctl
 * Control function for handling ioctl-like operations on the io_lit structure.
 */
int iolit_ctl(struct io_intf* io, int cmd, void* arg) {
    // Cast the generic io_intf pointer to io_lit type.
    struct io_lit* lit = (struct io_lit*) io;

    // Check if the io or arg pointer (for set position) is NULL to prevent invalid access.
    if (!lit || (cmd == IOCTL_SETPOS && arg == NULL)) return -EINVAL;

    // Handle the command using a switch-case structure.
    switch (cmd) {
        case IOCTL_GETLEN:
            // Return the total size of the buffer.
            return lit->size;

        case IOCTL_GETPOS:
            // Return the current position within the buffer.
            return lit->pos;

        case IOCTL_SETPOS:
            {
                // Cast arg to int* and set the position if it's within valid bounds.
                int* a = (int*) arg;
                if (*a < 0 || *a >= lit->size) return -EINVAL; // Ensure the new position is valid.
                lit->pos = *a;
                return lit->pos;
            }

        default:
            // Return an error code for unsupported commands.
            return -ENOTSUP;
    }
}



//           I/O term provides three features:
//          
//               1. Input CRLF normalization. Any of the following character sequences in
//                  the input are converted into a single \n:
//          
//                      (a) \r\n,
//                      (b) \r not followed by \n,
//                      (c) \n not preceeded by \r.
//          
//               2. Output CRLF normalization. Any \n not preceeded by \r, or \r not
//                  followed by \n, is written as \r\n. Sequence \r\n is written as \r\n.
//          
//               3. Line editing. The ioterm_getsn function provides line editing of the
//                  input.
//          
//           Input CRLF normalization works by maintaining one bit of state: cr_in.
//           Initially cr_in = 0. When a character ch is read from rawio:
//           
//           if cr_in = 0 and ch == '\r': return '\n', cr_in <- 1;
//           if cr_in = 0 and ch != '\r': return ch;
//           if cr_in = 1 and ch == '\r': return \n;
//           if cr_in = 1 and ch == '\n': skip, cr_in <- 0;
//           if cr_in = 1 and ch != '\r' and ch != '\n': return ch, cr_in <- 0.
//          
//           Ouput CRLF normalization works by maintaining one bit of state: cr_out.
//           Initially, cr_out = 0. When a character ch is written to I/O term:
//          
//           if cr_out = 0 and ch == '\r': output \r\n to rawio, cr_out <- 1;
//           if cr_out = 0 and ch == '\n': output \r\n to rawio;
//           if cr_out = 0 and ch != '\r' and ch != '\n': output ch to rawio;
//           if cr_out = 1 and ch == '\r': output \r\n to rawio;
//           if cr_out = 1 and ch == '\n': no ouput, cr_out <- 0;
//           if cr_out = 1 and ch != '\r' and ch != '\n': output ch, cr_out <- 0.

struct io_intf * ioterm_init(struct io_term * iot, struct io_intf * rawio) {
    static const struct io_ops ops = {
        .close = ioterm_close,
        .read = ioterm_read,
        .write = ioterm_write,
        .ctl = ioterm_ioctl
    };

    iot->io_intf.ops = &ops;
    // Initialize the reference count to 1 when created
    iot->io_intf.refcnt = 1;

    iot->rawio = rawio;
    iot->cr_out = 0;
    iot->cr_in = 0;

    return &iot->io_intf;
};

int ioputs(struct io_intf * io, const char * s) {
    const char nl = '\n';
    size_t slen;
    long wlen;

    slen = strlen(s);

    wlen = iowrite(io, s, slen);
    if (wlen < 0)
        return wlen;

    //           Write newline

    wlen = iowrite(io, &nl, 1);
    if (wlen < 0)
        return wlen;
    
    return 0;
}

long ioprintf(struct io_intf * io, const char * fmt, ...) {
	va_list ap;
	long result;

	va_start(ap, fmt);
	result = iovprintf(io, fmt, ap);
	va_end(ap);
	return result;
}

long iovprintf(struct io_intf * io, const char * fmt, va_list ap) {
    //           state.nout is number of chars written or negative error code
    struct iovprintf_state state = { .io = io, .err = 0 };
    size_t nout;

	nout = vgprintf(iovprintf_putc, &state, fmt, ap);
    return state.err ? state.err : nout;
}

char * ioterm_getsn(struct io_term * iot, char * buf, size_t n) {
    char * p = buf;
    int result;
    char c;

    for (;;) {
        //           already CRLF normalized
        c = iogetc(&iot->io_intf);

        switch (c) {
        //           escape
        case '\133':
            iot->cr_in = 0;
            break;
        //           should not happen
        case '\r':
        case '\n':
            result = ioputc(iot->rawio, '\r');
            if (result < 0)
                return NULL;
            result = ioputc(iot->rawio, '\n');
            if (result < 0)
                return NULL;
            *p = '\0';
            return buf;
        //           backspace
        case '\b':
        //           delete
        case '\177':
            if (p != buf) {
                p -= 1;
                n += 1;
                
                result = ioputc(iot->rawio, '\b');
                if (result < 0)
                    return NULL;
                result = ioputc(iot->rawio, ' ');
                if (result < 0)
                    return NULL;
                result = ioputc(iot->rawio, '\b');
            } else
                //           beep
                result = ioputc(iot->rawio, '\a');
            
            if (result < 0)
                return NULL;
            break;

        default:
            if (n > 1) {
                result = ioputc(iot->rawio, c);
                *p++ = c;
                n -= 1;
            } else
                //           beep
                result = ioputc(iot->rawio, '\a');
            
            if (result < 0)
                return NULL;
        }
    }
}

//           INTERNAL FUNCTION DEFINITIONS
//          

void ioterm_close(struct io_intf * io) {
    struct io_term * const iot = (void*)io - offsetof(struct io_term, io_intf);
    ioclose(iot->rawio);
}

long ioterm_read(struct io_intf * io, void * buf, size_t len) {
    struct io_term * const iot = (void*)io - offsetof(struct io_term, io_intf);
    char * rp;
    char * wp;
    long cnt;
    char ch;

    do {
        //           Fill buffer using backing io interface

        cnt = ioread(iot->rawio, buf, len);

        if (cnt < 0)
            return cnt;
        
        //           Scan though buffer and fix up line endings. We may end up removing some
        //           characters from the buffer.  We maintain two pointers /wp/ (write
        //           position) and and /rp/ (read position). Initially, rp = wp, however, as
        //           we delete characters, /rp/ gets ahead of /wp/, and we copy characters
        //           from *rp to *wp to shift the contents of the buffer.
        //           
        //           The processing logic is as follows:
        //           if cr_in = 0 and ch == '\r': return '\n', cr_in <- 1;
        //           if cr_in = 0 and ch != '\r': return ch;
        //           if cr_in = 1 and ch == '\r': return \n;
        //           if cr_in = 1 and ch == '\n': skip, cr_in <- 0;
        //           if cr_in = 1 and ch != '\r' and ch != '\n': return ch, cr_in <- 0.

        wp = rp = buf;
        while ((void*)rp < buf+cnt) {
            ch = *rp++;

            if (iot->cr_in) {
                switch (ch) {
                case '\r':
                    *wp++ = '\n';
                    break;
                case '\n':
                    iot->cr_in = 0;
                    break;
                default:
                    iot->cr_in = 0;
                    *wp++ = ch;
                }
            } else {
                switch (ch) {
                case '\r':
                    iot->cr_in = 1;
                    *wp++ = '\n';
                    break;
                default:
                    *wp++ = ch;
                }
            }
        }

    //           We need to return at least one character, however, it is possible that
    //           the buffer is still empty. (This would happen if it contained a single
    //           '\n' character and cr_in = 1.) If this happens, read more characters.
    } while (wp == buf);

    return (wp - (char*)buf);
}

long ioterm_write(struct io_intf * io, const void * buf, size_t len) {
    struct io_term * const iot = (void*)io - offsetof(struct io_term, io_intf);
    //           how many bytes from the buffer have been written
    long acc = 0;
    //           everything up to /wp/ in buffer has been written out
    const char * wp;
    //           position in buffer we're reading
    const char * rp;
    long cnt;
    char ch;

    //           Scan through buffer and look for cases where we need to modify the line
    //           ending: lone \r and lone \n get converted to \r\n, while existing \r\n
    //           are not modified. We can't modify the buffer, so mwe may need to do
    //           partial writes.
    //           The strategy we want to implement is:
    //           if cr_out = 0 and ch == '\r': output \r\n to rawio, cr_out <- 1;
    //           if cr_out = 0 and ch == '\n': output \r\n to rawio;
    //           if cr_out = 0 and ch != '\r' and ch != '\n': output ch to rawio;
    //           if cr_out = 1 and ch == '\r': output \r\n to rawio;
    //           if cr_out = 1 and ch == '\n': no ouput, cr_out <- 0;
    //           if cr_out = 1 and ch != '\r' and ch != '\n': output ch, cr_out <- 0.

    wp = rp = buf;

    while ((void*)rp < buf+len) {
        ch = *rp++;
        switch (ch) {
        case '\r':
            //           We need to emit a \r\n sequence. If it already occurs in the
            //           buffer, we're all set. Otherwise, we need to write what we have
            //           from the buffer so far, then write \n, and then continue.
            if ((void*)rp < buf+len && *rp == '\n') {
                //           The easy case: buffer already contains \r\n, so keep going.
                iot->cr_out = 0;
                rp += 1;
            } else {
                //           Next character is not '\n' or we're at the end of the buffer.
                //           We need to write out what we have so far and add a \n.
                cnt = iowrite(iot->rawio, wp, rp - wp);
                if (cnt < 0)
                    return cnt;
                else if (cnt == 0)
                    return acc;
                
                acc += cnt;
                wp += cnt;

                //           Now output \n, which does not count toward /acc/.
                cnt = ioputc(iot->rawio, '\n');
                if (cnt < 0)
                    return cnt;
                
                iot->cr_out = 1;
            }
                
            break;
        
        case '\n':
            //           If last character was \r, skip the \n. This should only occur at
            //           the beginning of the buffer, because we check for a \n after a
            //           \r, except if \r is the last character in the buffer. Since we're
            //           at the start of the buffer, we don't have to write anything out.
            if (iot->cr_out) {
                iot->cr_out = 0;
                wp += 1;
                break;
            }
            
            //           Previous character was not \r, so we need to write a \r first,
            //           then the rest of the buffer. But before that, we need to write
            //           out what we have so far, up to, but not including the \n we're
            //           processing.
            if (wp != rp-1) {
                cnt = iowrite(iot->rawio, wp, rp-1 - wp);
                if (cnt < 0)
                    return cnt;
                else if (cnt == 0)
                    return acc;
                acc += cnt;
                wp += cnt;
            }
            
            cnt = ioputc(iot->rawio, '\r');
            if (cnt < 0)
                return cnt;
            
            //           wp should now point to \n. We'll write it when we drain the
            //           buffer later.

            iot->cr_out = 0;
            break;
            
        default:
            iot->cr_out = 0;
        }
    }

    if (rp != wp) {
        cnt = iowrite(iot->rawio, wp, rp - wp);

        if (cnt < 0)
            return cnt;
        else if (cnt == 0)
            return acc;
        acc += cnt;
    }

    return acc;
}

int ioterm_ioctl(struct io_intf * io, int cmd, void * arg) {
    struct io_term * const iot = (void*)io - offsetof(struct io_term, io_intf);

    //           Pass ioctls through to backing io interface. Seeking is not supported,
    //           because we maintain state on the characters output so far.
    if (cmd != IOCTL_SETPOS)
        return ioctl(iot->rawio, cmd, arg);
    else
        return -ENOTSUP;
}

void iovprintf_putc(char c, void * aux) {
    struct iovprintf_state * const state = aux;
    int result;

    if (state->err == 0) {
        result = ioputc(state->io, c);
        if (result < 0)
            state->err = result;
    }
}
