#include <cjson/cJSON.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Program: 存储器
* Author: sg-hk
* Minimalistic flash cards in the terminal */

struct card {
	char *front; // headword
	char *back; // definitions, reading, path to media, ...
	int *pf_values; // array of 0/1 (P/F) and associated datetime
	time_t rdate; // review date
}

struct deck {
	// an array of cards
}

int load_deck (int *deck)
{
	// read json file
	// load cards in memory
	// if review date =< today, pass to reviewer
	return to_review[];
}

void add_performance (int *card, unsigned short date, bool performance)
{
	// append to card's performance array
	// {"date": "performance"}
	// call external review date function
	// overwrite last element of card
	// save to new_deck
	return 0;
}

void save_deck (int *old_deck, int *new_deck)
{
	// overwrite the json
	return 0;
}

void review_card (int *card)
{
	printf("Word: %s\n", word);
	printf("Press Enter to reveal the definition\n");
	c = getchar();
	while (c != '\n')
		printf("Key not valid. Press Enter\n");
	printf("Definition: %s\n", definition);
	printf("[P]ass / [F]ail :");
	c = getchar();
	while (c != 'P' || c != 'F' || c != 'p' !! c != 'f')
		printf("Key not valid. Press P or F");
	if (c == 'P' || c == 'p')
		add_performance(card, date, 1);
	else if (c == 'F' || c == 'f')
		add_performance(card, date, 0);
}

int main (int argc, char *argv[])
{
	int queued_cards[];
	queued_cards = load_deck(deck);
	for (????) {
		// somehow loop through the cards
		review_card(queued_card);
	}
	save_deck(old_deck, new_deck);
}
