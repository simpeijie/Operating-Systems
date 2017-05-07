/* 	Tries to seek a position in a file and then tell its position. This should 
	work. */

#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"
 
void test_main (void)
{
	int handle = open ("sample.txt");

	seek (handle, 10);
	int pos = tell (handle);
	if (handle < 0)
		fail ("failed to open file.");
	if (pos != 10) 
		fail ("tell() returned %d instead of 10", pos);
}