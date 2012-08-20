#include <stdio.h>
#include "sc.h"

int main(void)
{
	int fd;
	int r;
	
	fd = sc_open("/tmp/test", O_RDWR | O_CREAT | O_TRUNC, 0777);
	if (fd == -1) {
		fprintf(stderr, "fail to open file.\n");
		return -1;
	}

	fprintf(stderr, "fd: %d.\n", fd);

	r = sc_write(fd, "Hello.\n", 7);
	fprintf(stderr, "r: %d.\n", r);

	r = sc_close(fd);

	fprintf(stderr, "r: %d.\n", r);
	return 0;
}
