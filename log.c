#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>

#include "sc.h"
#include "log.h"

typedef struct {
	void *opaque;
	void (*send)(void *opaque, const char *data, int len);
} Out;

static void out_send(Out *o, const void *data, size_t len)
{
	o->send(o->opaque, data, (int)len);
}

static void out_send_repeat(Out *o, char ch, int count)
{
	char pad[8];
	const int padSize = (int)sizeof(pad);

	memset(pad, ch, sizeof(pad));
	while (count > 0) {
		int avail = count;
		if (avail > padSize) {
			avail = padSize;
		}
		o->send(o->opaque, pad, avail);
		count -= avail;
	}
}

/*** formatted output implementation
 ***/

/* Parse a decimal string from 'format + *ppos',
 * return the value, and writes the new position past
 * the decimal string in '*ppos' on exit.
 *
 * NOTE: Does *not* handle a sign prefix.
 */
static unsigned
parse_decimal(const char *format, int *ppos)
{
    const char* p = format + *ppos;
    unsigned result = 0;

    for (;;) {
        int ch = *p;
        unsigned d = (unsigned)(ch - '0');

        if (d >= 10U)
            break;

        result = result*10 + d;
        p++;
    }
    *ppos = p - format;
    return result;
}

/* write an octal/decimal/number into a bounded buffer.
 * assumes that bufsize > 0, and 'digits' is a string of
 * digits of at least 'base' values.
 */
static void
format_number(char *buffer, size_t bufsize, uint64_t value, int base, const char *digits)
{
    char *pos = buffer;
    char *end = buffer + bufsize - 1;

    /* generate digit string in reverse order */
    while (value) {
        unsigned d = value % base;
        value /= base;
        if (pos < end) {
            *pos++ = digits[d];
        }
    }

    /* special case for 0 */
    if (pos == buffer) {
        if (pos < end) {
            *pos++ = '0';
        }
    }
    pos[0] = '\0';

    /* now reverse digit string in-place */
    end = pos - 1;
    pos = buffer;
    while (pos < end) {
        int ch = pos[0];
        pos[0] = end[0];
        end[0] = (char) ch;
        pos++;
        end--;
    }
}

/* Write an integer (octal or decimal) into a buffer, assumes buffsize > 2 */
static void
format_integer(char *buffer, size_t buffsize, uint64_t value, int base, int isSigned)
{
    if (isSigned && (int64_t)value < 0) {
        buffer[0] = '-';
        buffer += 1;
        buffsize -= 1;
        value = (uint64_t)(-(int64_t)value);
    }

    format_number(buffer, buffsize, value, base, "0123456789");
}

/* Write an octal into a buffer, assumes buffsize > 2 */
static void
format_octal(char *buffer, size_t buffsize, uint64_t value, int isSigned)
{
    format_integer(buffer, buffsize, value, 8, isSigned);
}

/* Write a decimal into a buffer, assumes buffsize > 2 */
static void
format_decimal(char *buffer, size_t buffsize, uint64_t value, int isSigned)
{
    format_integer(buffer, buffsize, value, 10, isSigned);
}

/* Write an hexadecimal into a buffer, isCap is true for capital alphas.
 * Assumes bufsize > 2 */
static void
format_hex(char *buffer, size_t buffsize, uint64_t value, int isCap)
{
    const char *digits = isCap ? "0123456789ABCDEF" : "0123456789abcdef";

    format_number(buffer, buffsize, value, 16, digits);
}

/* Perform formatted output to an output target 'o' */
static void out_vformat(Out *o, const char *format, va_list args)
{
	int nn = 0;

	for (;;) {
		int mm;
		int padZero = 0;
		int padLeft = 0;
		char sign = '\0';
		int width = -1;
		int prec  = -1;
		size_t bytelen = sizeof(int);
		const char*  str;
		int slen;
		char buffer[32];  /* temporary buffer used to format numbers */

		char  c;

		/* first, find all characters that are not 0 or '%' */
		/* then send them to the output directly */
		mm = nn;
		do {
			c = format[mm];
			if (c == '\0' || c == '%')
				break;
			mm++;
		} while (1);

		if (mm > nn) {
			out_send(o, format+nn, mm-nn);
			nn = mm;
		}

		/* is this it ? then exit */
		if (c == '\0')
			break;

		/* nope, we are at a '%' modifier */
		nn++;  // skip it

		/* parse flags */
		for (;;) {
			c = format[nn++];
			if (c == '\0') {  /* single trailing '%' ? */
				c = '%';
				out_send(o, &c, 1);
				return;
			}
			else if (c == '0') {
				padZero = 1;
				continue;
			}
			else if (c == '-') {
				padLeft = 1;
				continue;
			}
			else if (c == ' ' || c == '+') {
				sign = c;
				continue;
			}
			break;
		}

		/* parse field width */
		if ((c >= '0' && c <= '9')) {
			nn --;
			width = (int)parse_decimal(format, &nn);
			c = format[nn++];
		}

		/* parse precision */
		if (c == '.') {
			prec = (int)parse_decimal(format, &nn);
			c = format[nn++];
		}

		/* length modifier */
		switch (c) {
			case 'h':
				bytelen = sizeof(short);
				if (format[nn] == 'h') {
					bytelen = sizeof(char);
					nn += 1;
				}
				c = format[nn++];
				break;
			case 'l':
				bytelen = sizeof(long);
				if (format[nn] == 'l') {
					bytelen = sizeof(long long);
					nn += 1;
				}
				c = format[nn++];
				break;
			case 'z':
				bytelen = sizeof(size_t);
				c = format[nn++];
				break;
			case 't':
				bytelen = sizeof(ptrdiff_t);
				c = format[nn++];
				break;
			default:
				;
		}

		/* conversion specifier */
		if (c == 's') {
			/* string */
			str = va_arg(args, const char*);
		} else if (c == 'c') {
			/* character */
			/* NOTE: char is promoted to int when passed through the stack */
			buffer[0] = (char) va_arg(args, int);
			buffer[1] = '\0';
			str = buffer;
		} else if (c == 'p') {
			uint64_t  value = (uintptr_t) va_arg(args, void*);
			buffer[0] = '0';
			buffer[1] = 'x';
			format_hex(buffer + 2, sizeof buffer-2, value, 0);
			str = buffer;
		} else {
			/* integers - first read value from stack */
			uint64_t value;
			int isSigned = (c == 'd' || c == 'i' || c == 'o');

			/* NOTE: int8_t and int16_t are promoted to int when passed
			 *       through the stack
			 */
			switch (bytelen) {
				case 1: value = (uint8_t)  va_arg(args, int); break;
				case 2: value = (uint16_t) va_arg(args, int); break;
				case 4: value = va_arg(args, uint32_t); break;
				case 8: value = va_arg(args, uint64_t); break;
				default: return;  /* should not happen */
			}

			/* sign extension, if needed */
			if (isSigned) {
				int shift = 64 - 8*bytelen;
				value = (uint64_t)(((int64_t)(value << shift)) >> shift);
			}

			/* format the number properly into our buffer */
			switch (c) {
				case 'i': case 'd':
					format_integer(buffer, sizeof buffer, value, 10, isSigned);
					break;
				case 'o':
					format_integer(buffer, sizeof buffer, value, 8, isSigned);
					break;
				case 'x': case 'X':
					format_hex(buffer, sizeof buffer, value, (c == 'X'));
					break;
				default:
					buffer[0] = '\0';
			}
			/* then point to it */
			str = buffer;
		}

		/* if we are here, 'str' points to the content that must be
		 * outputted. handle padding and alignment now */

		slen = strlen(str);

		if (slen < width && !padLeft) {
			char padChar = padZero ? '0' : ' ';
			out_send_repeat(o, padChar, width - slen);
		}

		out_send(o, str, slen);

		if (slen < width && padLeft) {
			char padChar = padZero ? '0' : ' ';
			out_send_repeat(o, padChar, width - slen);
		}
	}
}

typedef struct {
	Out out[1];
	int fd;
	int total;
} FdOut;

static void fd_out_send(void *opaque, const char *data, int len)
{
	FdOut *fdo = opaque;

	if (len < 0)
		len = strlen(data);

	while (len > 0) {
		int ret = sc_write(fdo->fd, data, len);
		if (ret < 0) {
			if (errno == EINTR)
				continue;
			break;
		}
		data += ret;
		len -= ret;
		fdo->total += ret;
	}
}

static Out* fd_out_init(FdOut *fdo, int  fd)
{
	fdo->out->opaque = fdo;
	fdo->out->send = fd_out_send;
	fdo->fd = fd;
	fdo->total = 0;

	return fdo->out;
}

static int fd_out_length(FdOut *fdo)
{
	return fdo->total;
}

static int log_fd = -1;

int format_fd(const char *format, ...)
{
	FdOut fdo;
	Out* out;
	va_list args;

	if (log_fd == -1) {
		log_fd = sc_open(LOG_FILE_PATH, O_RDWR | O_CREAT | O_TRUNC, 0777);
		if (log_fd == -1)
			return 0;
	}

	out = fd_out_init(&fdo, log_fd);
	if (out == NULL)
		return 0;

	va_start(args, format);
	out_vformat(out, format, args);
	va_end(args);

	return fd_out_length(&fdo);
}
