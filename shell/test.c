#include <stdlib.h>
#include <stdio.h>

int main() {
	char **ptr = (char **) malloc(sizeof(char *) * 5);
	int i;
	for (i = 0; i < 5; i++) {
		ptr[i] = "Hey there";
	}

	ptr[0] = "Weeeee";

	for (i = 0; i < 5; i++) {
		printf("%s\n", ptr[i]);
	}

	return 0;
}
