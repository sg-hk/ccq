/*	存储器 (ccq) by sg-hk

	ccq does five things
 	1. collect due cards
	2. parse due cards
	3. review due cards
	4. store review data
	5. reschedule cards		*/

#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <time.h>
#include "cJSON.h"

#define CCQ_PATH "/.local/share/ccq/"
#define INITIAL_CAPACITY 128
#define MAX_FIELD_LENGTH 512

typedef struct Card {
	const char *word;
	const char *reading;
	const char *definition;
	const char **audiofiles;
	const char **sentences;
	const char **sentence_readings;
	int *review_dates;
	char **review_results;
	int *review_times;
	int due_date;
} Card;

char *read_file_to_string(const char *filename)
{
	FILE *f = fopen(filename, "rb");
	if (f == NULL) {
		perror("Deck access error");
		exit(EXIT_FAILURE);
	}
	fseek(f, 0, SEEK_END);
	long length = ftell(f);
	fseek(f, 0, SEEK_SET);
	char *buffer = (char *)malloc(length + 1);
	if (buffer) {
		fread(buffer, 1, length, f);
		buffer[length] = '\0';
	}
	fclose(f);
	return buffer;
}

Card parse_card(const char *word, cJSON *raw_card)
{
	Card parsed_card = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0};

	cJSON *back = cJSON_GetObjectItemCaseSensitive(raw_card, "back");
	cJSON *context = cJSON_GetObjectItemCaseSensitive(raw_card, "context");
	cJSON *log = cJSON_GetObjectItemCaseSensitive(raw_card, "log");
	cJSON *due_date = cJSON_GetObjectItemCaseSensitive(raw_card, "due_date");

	// loop through arrays (audio files, sentences, sentence readings, log data)
	int audiofile_count = cJSON_GetArraySize(cJSON_GetArrayItem(back, 2));
	parsed_card.audiofiles = malloc(sizeof(char *) * (audiofile_count + 1));
	for (int i = 0; i < audiofile_count; ++i) {
		parsed_card.audiofiles[i] = strdup(cJSON_GetArrayItem(cJSON_GetArrayItem(back, 2), i)->valuestring);
	}
	parsed_card.audiofiles[audiofile_count] = NULL;

	int sentence_count = cJSON_GetArraySize(cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(context, "sentences"));
	parsed_card.sentences = malloc(sizeof(char *) * (sentence_count + 1));
	for (int i = 0; i < sentence_count; ++i) {
		parsed_card.sentences[i] = strdup(cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(context, "sentences"), i)->valuestring);
	}
	parsed_card.sentences[sentence_count] = NULL;

	int sentence_reading_count = cJSON_GetArraySize(cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(context, "sentence_readings"));
	parsed_card.sentence_readings = malloc(sizeof(char *) * (sentence_reading_count + 1));
	for (int i = 0; i < sentence_reading_count; ++i) {
		parsed_card.sentence_readings[i] = strdup(cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(context, "sentence_readings"), i)->valuestring);
	}
	parsed_card.sentence_readings[sentence_reading_count] = NULL;

	int review_count = cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(log, "review"));
	parsed_card.review_dates = malloc(sizeof(int) * review_count);
	parsed_card.review_results = malloc(sizeof(char *) * review_count);
	parsed_card.review_times = malloc(sizeof(int) * review_count);
	for (int i = 0; i < review_count; ++i) {
		cJSON *review = cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(log, "reviews"), i);
		parsed_card.review_dates[i] = cJSON_GetObjectItemCaseSensitive(review, "date")->valueint;
		parsed_card.review_results[i] = strdup(cJSON_GetObjectItemCaseSensitive(review, "result")->valuestring);
		parsed_card.review_times[i] = cJSON_GetObjectItemCaseSensitive(review, "time")->valueint;
	}

	// non-arrays can be attributed directly
	parsed_card.word = word;
	parsed_card.reading = strdup(cJSON_GetArrayItem(back, 0)->valuestring);
	parsed_card.definition = strdup(cJSON_GetArrayItem(back, 1)->valuestring);
	parsed_card.due_date = cJSONdue_date->valueint;
	return parsed_card;
}

void free_card(Card *card) {
    free((void*)card->reading);
    free((void*)card->definition);
    for (int i = 0; card->audiofiles[i]; i++) {
        free((void*)card->audiofiles[i]);
    }
    free(card->audiofiles);
    for (int i = 0; card->sentences[i]; i++) {
        free((void*)card->sentences[i]);
    }
    free(card->sentences);
    for (int i = 0; card->sentence_readings[i]; i++) {
        free((void*)card->sentence_readings[i]);
    }
    free(card->sentence_readings);
    free(card->review_dates);
    for (int i = 0; card->review_results[i]; i++) {
        free(card->review_results[i]);
    }
    free(card->review_results);
    free(card->review_times);
}

time_t reschedule(char result, time_t old_due_date, time_t todays_date)
{
	/* one day is 86400 seconds */
	time_t new_due_date = (result == 'p' && old_due_date != 0) ? // if passed & not new card
    		todays_date + (todays_date - old_due_date) * 2 : // interval*2
		todays_date + 86400; // otherwise reschedule to the next day
	return new_due_date;
}

Card review_card(Card due_card, time_t todays_date)
{
	Card updated_card = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};

	/*print and get key press*/
	/*change log data*/
	/*change due date*/

	return updated_card;
}

Card *update_deck(char *path)
{
	Card *updated_cards = NULL;
	return updated_cards;
}

int main(int argc, char *argv[])
{
	time_t todays_date = time(NULL);
	printf("ccq v0.2 - a minimalistic flashcard program in C\n");

	if (argv[1] != NULL) {
		char path[128];
		snprintf(path, sizeof(path), "%s%s%s", getenv("HOME"), CCQ_PATH, argv[0]);
		char *json_data = read_file_to_string(path);
		if (json_data == NULL) {
			perror("JSON parsing error");
			exit(EXIT_FAILURE);
		}
	}
	else {
		printf("\nno deck was given as input\n");
		printf("usage: %s <deck_name>\n", argv[0]);
		printf("the decks are located in /.local/share/ccq\n");
		exit(EXIT_FAILURE);
	}

	cJSON *root = cJSON_Parse(json_data);
	if (!root) {
		fprintf(stderr, "JSON parsing error: %s\n", cJSON_GetErrorPtr());
		free(json_data);
		exit(EXIT_FAILURE);
	}

	// parse the whole card

	cJSON *words = cJSON_GetObjectItemCaseSensitive(root, "words");
	size_t card_count = cJSON_GetArraySize(words);
	Card **cards = malloc(sizeof(Card *) * card_count);
	
	cJSON *entry = NULL;
	size_t index = 0;
	cJSON_ArrayForEach(entry, words) {
		cards[++index] = parse_card(entry->child);
	}

	// review the card

	// update the card (log & scheduling)

	// freeing memory
	for (size_t i = 0; i < card_count; ++i) {
		Card *card = cards[i];
		free((void *)card->reading);
		free((void *)card->reading);
		for (size_t j = 0; card->audiofiles[j]; ++j) {
			free((void *)card->audiofiles[j]);
		}
		free(card->audiofiles);
		free(card);
	}
	free(cards);
	cJSON_Delete(root);
	free(json_data);

	return 0;
}
