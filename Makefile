CC := gcc
LD := ld
CFLAGS := -fPIC -m32 -fno-stack-protector -nostdlib
LDFLAGS := -L/usr/lib/gcc/i686-linux-gnu/4.6/ -L/usr/lib/i386-linux-gnu/
LDFLAGS += --start-group /usr/lib/gcc/i686-linux-gnu/4.6/libgcc.a /usr/lib/gcc/i686-linux-gnu/4.6/libgcc_eh.a /usr/lib/i386-linux-gnu/libc.a --end-group
LDFLAGS += -Bstatic

.PHONY: all build clean

all: build

build: linker

linker: begin.o log.o linker.o
	$(LD) -o $@ $? $(LDFLAGS)

begin.o: begin.S
	$(CC) $(CFLAGS) -c -o $@ $?

log.o: log.c
	$(CC) $(CFLAGS) -c -o $@ $?

linker.o: linker.c
	$(CC) $(CFLAGS) -c -o $@ $?



clean:
	rm -rf *.o linker
