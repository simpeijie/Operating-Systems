#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"

void test_main (void) 
{
	create ("hit_miss", 512);
	int fd = open ("hit_miss");
	char buffer[512];
	int i;
	for (i = 0; i < 512; i++) {
		buffer[i] = i;
	}
	write (fd, buffer, 512);

	for (i = 0; i < 10; i++) {
		read (fd, buffer, 512);
	}
	int hit_cnt = hit ();
	int miss_cnt = miss ();
	close (fd);

	fd = open ("hit_miss");
	for (i = 0; i < 10; i++) {
		read(fd, buffer, 512);
	}

	int new_hit_cnt = hit ();
	int new_miss_cnt = miss ();
	// close (fd);

	if (hit_cnt *(new_hit_cnt + new_miss_cnt) >= new_hit_cnt * (hit_cnt + miss_cnt)) {
		fail ("New hit rate should be greater than old hit rate.");
	}

}