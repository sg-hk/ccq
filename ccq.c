/* Program: ccq = 存储器
* Author: sg-hk
* Minimalistic flashcards in the terminal */

#include "fsrs_scheduler.c"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define RELATIVE_PATH "/.local/share/ccq/deck"
#define MAX_LINE_LENGTH 2048
#define INITIAL_CAPACITY 100 

int get_keypress(void) 
{
	// gets character without echoing it to the terminal
	// and without requiring <Return>
	// (thanks StackOverflow)
	struct termios oldt, newt;
	int ch;
	tcgetattr(STDIN_FILENO, &oldt);
	newt = oldt;
	newt.c_lflag &= ~(ICANON | ECHO); // new terminal flags
	tcsetattr(STDIN_FILENO, TCSANOW, &newt);
	ch = getchar();
	tcsetattr(STDIN_FILENO, TCSANOW, &oldt); // switch back to old terminal
	return ch;
}

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

	// get due words
	char line[MAX_LINE_LENGTH];
	char** matching_lines = malloc(INITIAL_CAPACITY * sizeof(char *));
	int capacity = INITIAL_CAPACITY;
	int count = 0;

	while (fgets(line, sizeof(line), deck)) {
		line[strcspn(line, "\n")] = 0; // remove \n from line
		char* last_comma = strrchr(line, ',');
		if (last_comma != NULL) {
			// parse last csv field as review date
			// copy line to memory if date <= today
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
		CardReview reviews[count];
		printf("Press [a]gain, [h]ard, [g]ood, [e]asy:\n");
		int i = 0;
		while (i < count) {
			// parse first csv field as card id
			char* first_comma = strchr(matching_lines[i], ',');
			size_t id_length = first_comma - matching_lines[i];
			char word_id[id_length + 1];
			strncpy(word_id, matching_lines[i], id_length);
			word_id[id_length] = '\0';

			// parse second csv field as front of card
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

			// record user input for each card
			printf("%s\n", word);
			int review_input;
			while (1) {
				review_input = get_keypress();
				if (review_input == 'a' || review_input == 'h' || review_input == 'g' || review_input == 'e') {
					printf("Card ID %s means %s\n\n", word_id, meaning);
					// store id and review in array and move to next line
					reviews[i].card_id = atoi(word_id);
					reviews[i].review_input = review_input;
					i++;
					break;
				} else {
					// otherwise repeat
					printf("Unrecognized input. Press [a], [h], [g] or [e]: ");
				}
			}
		}
		schedule_cards(reviews, count);
	} else {
		printf("No words to review.\n");
	}

	free(matching_lines);

	return 0;
}
