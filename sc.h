#ifndef SC_H
#define SC_H

#include <asm/unistd.h>
#include <fcntl.h>

static inline int sc_write(int fd, const char * buf, int len)
{
  int status;
  __asm__ volatile ("pushl %%ebx\n"\
                    "movl %%esi,%%ebx\n"\
                    "int $0x80\n" \
                    "popl %%ebx\n"\
                    : "=a" (status) \
                    : "0" (__NR_write),"S" ((long)(fd)),"c" ((long)(buf)),"d" ((long)(len)));
}

static inline int sc_open(const char *pathname, int flags, unsigned long mode)
{
  int status;
  __asm__ volatile ("pushl %%ebx\n"\
                    "movl %%esi,%%ebx\n"\
                    "int $0x80\n" \
                    "popl %%ebx\n"\
                    : "=a" (status) \
                    : "0" (__NR_open),"S" ((long)(pathname)),"c" ((long)(flags)),"d" ((long)(mode)));
}

static inline int sc_close(int fd)
{
	int __res;
	__asm__ volatile ("movl %%ecx,%%ebx\n"\
			"int $0x80" \
			:  "=a" (__res) : "0" (__NR_close),"c" ((long)(fd)));
}

static inline void sc_exit(int status)
{
  int __res;
  __asm__ volatile ("movl %%ecx,%%ebx\n"\
                    "int $0x80" \
                    :  "=a" (__res) : "0" (__NR_exit),"c" ((long)(status)));
}
#endif
