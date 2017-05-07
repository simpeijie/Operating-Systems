/* 	Tries to obtain the filesize of a valid fd, this should work. */

#include <limits.h>
#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"

void 
test_main (void)
{
	int handle = open ("sample.txt");
	if (handle < 0)
		fail ("failed to open file.");

	int size = filesize (handle);
	if (size != 239)
		fail ("filesize() returned %d instead of 239.", size);
}