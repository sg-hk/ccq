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

	// review cards
	if (count > 0) {
		printf("Today's reviews:\n");
		for (int i = 0; i < count; i++) {
			// parse second csv field as front of card
			char* first_comma = strchr(matching_lines[i], ',');
			char* second_comma = strchr(first_comma + 1, ',');
			size_t length_word = second_comma - first_comma - 1;
			char word[length_word + 1];
			strncpy(word, first_comma + 1, length_word);
			word[length_word] = '\0';
			// parse third csv field as back of card
			char* third_comma = strchr(second_comma + 1, ',');
			size_t length_meaning = third_comma - second_comma - 1;
			char meaning[length_meaning + 1];
			strncpy(meaning, second_comma + 1, length_meaning);
			meaning[length_meaning] = '\0';
			// print results
			printf(word);
			printf(" has meaning ");
			printf(meaning);
			printf("\n");
		}
	} else {
		printf("No words to review.\n");
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
