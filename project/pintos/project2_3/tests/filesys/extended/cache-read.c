#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"
#include "devices/block.h"

void test_main (void) 
{
	create ("read_test", 512);
	int fd = open ("read_test");
	char buffer[512];
	int i;
	for (i = 0; i < 512; i++) {
		buffer[i] = 0;
	}
	reset_read ();
	write (fd, buffer, 512);
	unsigned long long num_read = read_cnt ();
	// unsigned long long num_write = write_cnt ();
	close (fd);

	if (num_read != 0) {
		fail ("Read count should be zero.");
	}
}
	
	