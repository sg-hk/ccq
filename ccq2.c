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
	// front of card
        char *word;
	// back of card
        char *reading;
        char *definition;
        char **audiofiles;
        char **sentences;
	// log data
        int *dates;
        char **results;
        int *times; // in ms, hence int not float
        int due_date;
} Card; 

char *strdup
(const char *s)  
{
        size_t size = strlen(s) + 1;
        char *p = malloc(size); 
        if (p) {
                memcpy(p, s, size);
        } 
        return p;
}

char *read_file_to_string
(const char *filename)
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
ar **parse_string_array 
(cJSON *json_array)
{
        int array_size = cJSON_GetArraySize(json_array); 
        char **string_array = malloc(sizeof(char *) * (array_size + 1)); // + 1 to null-terminate the array
        if (!string_array) return NULL;

        for (int i = 0; i < array_size; ++i) {
                cJSON *item = cJSON_GetArrayItem(json_array, i);
                if (item && item->valuestring) { 
                        string_array[i] = strdup(item->valuestring);
                        if (!string_array[i]) {
                                fprintf(stderr,"Json string array memory error\n"); 
                                for (int j = 0; j < i; ++j) {
                                        free(string_array[j]);
                                } 
                                free(string_array);
                                return NULL;
                        } 
                } else {
                        fprintf(stderr, "Json string array malformed\n");
                        string_array[i] = NULL; 
                }
        }

        string_array[array_size] = NULL; // null-terminating array

        return string_array; 
}

int *parse_int_array 
(cJSON *json_array)
{
        int array_size = cJSON_GetArraySize(json_array); 
        int *int_array = malloc(sizeof(int) * array_size);
        if (!int_array) return NULL;

        for (int i = 0; i < array_size; ++i) {
                cJSON *item = cJSON_GetArrayItem(json_array, i);
                if (item && item->valueint) { 
                        int_array[i] = item->valueint;
                } else {
                        fprintf(stderr, "Json int array malformed\n"); 
                        free(int_array);
                        return NULL;
                } 
        }

        return int_array; 
}


Card parse_card 
(char *word, cJSON *raw_card)
{
        Card parsed_card = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0}; 

        cJSON *back = cJSON_GetObjectItemCaseSensitive(raw_card, "back");
        cJSON *log = cJSON_GetObjectItemCaseSensitive(raw_card, "log"); 

        parsed_card.word = word;
        parsed_card.reading = strdup(cJSON_GetArrayItem(back, 0)->valuestring); 
        parsed_card.definition = strdup(cJSON_GetArrayItem(back, 1)->valuestring);
        cJSON *audiofiles = cJSON_GetArrayItem(back, 2);
        cJSON *sentences = cJSON_GetArrayItem(back, 3); 
        cJSON *dates = cJSON_GetArrayItem(log, 0);
        cJSON *results = cJSON_GetArrayItem(log, 1);
        cJSON *times = cJSON_GetArrayItem(log, 2); 
        parsed_card.audiofiles = parse_string_array(audiofiles);
        parsed_card.sentences = parse_string_array(sentences);
        parsed_card.dates = parse_int_array(dates); 
        parsed_card.results = parse_string_array(results);
        parsed_card.times = parse_int_array(times);
        parsed_card.due_date = cJSON_GetArrayItem(log,3)->valueint; 

        return parsed_card;
} 

void free_card
(Card *card)  
{
        free(card->reading);
        free(card->definition); 
        for (int i = 0; card->audiofiles[i]; ++i) {
                free(card->audiofiles[i]);
        } 
        free(card->audiofiles);
        for (int i = 0; card->sentences[i]; ++i) {
                free(card->sentences[i]); 
        }
        free(card->sentences);
        free(card->dates); 
        for (int i = 0; card->results[i]; ++i) {
                free(card->results[i]);
        } 
        free(card->results);
        free(card->times);
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
	if (argc != 2) {
                fprintf(stderr, "Usage: %s <deck_name>\n", argv[0]);
                exit(EXIT_FAILURE); 
        } else {
		printf("Welcome to ccq v0.02 - a minimalistic flash card program in C\n");
		printf("Today's study deck is \"%s\"\n", argv[1]);
	}

        char filepath[256]; 
        snprintf(filepath, sizeof(filepath), "%s%s%s%s", getenv("HOME"), CCQ_PATH, argv[1], ".json");

        printf("Reading file at %s\n", filepath); 
        char *json_data = read_file_to_string(filepath);
        if (!json_data) {
                fprintf(stderr, "Error reading file %s\n", filepath); 
                exit(EXIT_FAILURE);
        } else {
                printf("Successfully read file\n"); 
        }

        printf("Parsing the JSON structure...\n"); 
        cJSON *root = cJSON_Parse(json_data);
        if (!root) {
                fprintf(stderr, "Error parsing JSON at: %s\n", cJSON_GetErrorPtr()); 
                fprintf(stderr, "Please try validating the JSON through a linter\n");
                exit(EXIT_FAILURE);
        } 
        cJSON *example_deck = cJSON_GetObjectItemCaseSensitive(root, "example_deck"); // USE USER INPUT FOR CHOSEN DECK
        if (!example_deck) {
                fprintf(stderr, "Error accessing example_deck\n"); 
                exit(EXIT_FAILURE);
        }
        printf("Successfully parsed JSON structure\n");

	cJSON *current_element = NULL;
	cJSON_ArrayForEach(current_element, example_deck) {
		if (current_element->type == cJSON_Object) {
			Card card = parse_card(current_element->string, current_element);
			// compare due date to today
			// review the card if so
			// append log data to the Card struct
			// reschedule the card
			// write back into json deck file
			free_card(&card);
		}
	}
	
	cJSON_Delete(root);
	free(json_data);
	return 0;
}
