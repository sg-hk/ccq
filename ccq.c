/* Program: ccq = 存储器
* Author: sg-hk
* Minimalistic flashcards in the terminal */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define RELATIVE_PATH "/.local/share/ccq/deck"
#define MAX_LINE_LENGTH 2048
#define INITIAL_CAPACITY 100 

int main(int argc, char *argv[])
{
	// get today as int YYYYMMDD
	time_t t = time(NULL);
	struct tm date = *localtime(&t);
	int today = (date.tm_year + 1900) * 10000 + (date.tm_mon + 1) * 100 + date.tm_mday;
	printf("%d\n", today);

	// load deck
	char path[128];
	snprintf(path, sizeof(path), "%s%s", getenv("HOME"), RELATIVE_PATH);
	FILE* deck = fopen(path, "a+");
	if (deck == NULL) {
		perror("Deck access error");
		exit(EXIT_FAILURE);
	}
	rewind(deck);

	// get due words
	char line[MAX_LINE_LENGTH];
	char** matching_lines = malloc(INITIAL_CAPACITY * sizeof(char *));
	int capacity = INITIAL_CAPACITY;
	int count = 0;

	while (fgets(line, sizeof(line), deck)) {
		line[strcspn(line, "\n")] = 0; // remove \n from string
		char* last_comma = strrchr(line, ',');
		if (last_comma != NULL) {
			char* date_str = last_comma + 1;
			int date = atoi(date_str);
			if (date <= today) {
				if (count >= capacity) {
					capacity += 100;
					matching_lines = realloc(matching_lines, capacity * sizeof(char*));
				}
				matching_lines[count] = strdup(line);
				count++;
			}
		}
	}
	
	fclose(deck);

	if (count > 0) {
		printf("Matching lines:\n");
		for (int i = 0; i < count; i++) {
			printf("%s\n", matching_lines[i]);
			free(matching_lines[i]);
		}
	} else {
		printf("No line found with date >= today\n");
	}
	
	free(matching_lines);

	return 0;
}
// later: print test field, wait for 1-4 keypress, print answer field
// -> termios.h, remove ICONAN and ECHO flags (no EOF no print)
// -> getchar
// -> back to old terminal
// later: calling fsrs
// -> implement the basic algo with default params in C
// -> then just modify python optimizer code to work with our file format
// -> periodically call optimizer? or how best to use it?
// later: rewrite according to line ID, close file
// -> load csv to memory
// -> find most efficient function to find line with first field matching
// -> fprint_s() to change line
// -> close
