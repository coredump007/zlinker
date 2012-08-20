#include "sc.h"
#include "log.h"

unsigned __linker_init(unsigned **elfdata)
{
	D("Enter.");
	sc_exit(0);

	return 0;
}
