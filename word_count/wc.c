#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#define BUFFER_SIZE 5000

struct Tuple {
	int numWords;
	int numChars;
	int numLines;
} Tuple_default = {0, 0, 0};

struct Tuple getCount(FILE *stream) {
	struct Tuple t = Tuple_default;
	char buffer[BUFFER_SIZE];
	char c;
	bool isWord = false;
	while ((c = fgetc(stream)) != EOF) {
		t.numChars++;
		if (isalnum(c) || ispunct(c)) {
			isWord = true;
		}
		else if (isspace(c) && isWord) {
			t.numWords++;
			isWord = false;
		}
		else if (isxdigit(c) && isWord) {
			t.numWords++;
			isWord = false;			
		}
		//else if (iscntrl(c) && isWord) {
		//	t.numWords++;
		//	isWord = false;			
		//}
		if (c == '\n')
			t.numLines++;
	}
	if (isWord) {
		t.numWords++;
		isWord = false;
	}
	return t;
}

int main(int argc, char *argv[]) {
	if (argc == 1) {
		struct Tuple t = getCount(stdin);
		printf("\t%d\t%d\t%d\n", t.numLines, t.numWords, t.numChars);
	}
	else if (argc == 2) {
		FILE *fp;
		fp = fopen((char*)argv[1], "r");
		struct Tuple t = getCount(fp);	
		printf("\t%d\t%d\t%d\n", t.numLines, t.numWords, t.numChars);
		fclose(fp);
	}
	return 0;

}

