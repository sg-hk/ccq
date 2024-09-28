#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"

#define CCQ_PATH "/.local/share/ccq/"

typedef struct Card {
        char *word;
        char *reading;
        char *definition;
        char **audiofiles;
        char **sentences;
        int *dates;
        char **results;
        int *times; // in milliseconds
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

char **parse_string_array
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

	string_array[array_size] = NULL;

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

char* read_file_to_string
(const char *filename) 
{
    FILE *f = fopen(filename, "rb");
    if (f == NULL) return NULL;
    fseek(f, 0, SEEK_END);
    long length = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buffer = (char*)malloc(length + 1);
    if (buffer) {
        fread(buffer, 1, length, f);
        buffer[length] = '\0'; // Null-terminate the string
}
fclose(f);
    return buffer;
}

int main
(int argc, char **argv)
{
	if (argc != 2) {
		fprintf(stderr, "Usage: %s <deck_name>\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	char filepath[256];
	snprintf(filepath, sizeof(filepath), "%s%s%s%s", getenv("HOME"), CCQ_PATH, argv[1], ".json");

	char *json_data = read_file_to_string(filepath);
	if (!json_data) {
		fprintf(stderr, "Error reading file %s\n", filepath);
		exit(EXIT_FAILURE);
	} else {
		printf("Successfully parsed file at %s\n", filepath);
	}

	cJSON *root = cJSON_Parse(json_data);
	if (!root) {
		fprintf(stderr, "Error parsing JSON at: %s\n", cJSON_GetErrorPtr());
		fprintf(stderr, "Please try validating the JSON through a linter\n");
		exit(EXIT_FAILURE);
	}
	cJSON *example_deck = cJSON_GetObjectItemCaseSensitive(root, "example_deck");
	if (!example_deck) {
		fprintf(stderr, "Error accessing example_deck\n");
		exit(EXIT_FAILURE);
	}

	cJSON *current_element = NULL;
	cJSON_ArrayForEach(current_element, example_deck) {
		if (current_element->type == cJSON_Object) {
			Card card = parse_card(current_element->string, current_element);
			printf("Word: %s\n", card.word);
			printf("Reading: %s\n", card.reading);
			printf("Definition: %s\n", card.definition);
			if (card.audiofiles) {
		                printf("Audiofiles:\n");
		                for (char **audio = card.audiofiles; *audio; ++audio) {
					printf("  %s\n", *audio);
		                }
	            	}
	    		if (card.sentences) {
	                	printf("Sentences:\n");
	                	for (char **sentence = card.sentences; *sentence; ++sentence) {
	                    		printf("  %s\n", *sentence);
	                	}
	            	}
	    		if (card.dates) {
	                	printf("Review Dates:\n");
	                	for (int *date = card.dates; *date; ++date) {
	                    		printf("  %d\n", *date);
	                	}
	            	}
	    		if (card.results) {
	                	printf("Review Results:\n");
	                	for (char **result = card.results; *result; ++result) {
	                    		printf("  %s\n", *result);
	                	}
	            	}
	    		if (card.times) {
	                	printf("Review Times:\n");
	                	for (int *time = card.times; *time; ++time) {
	                    	printf("  %d milliseconds\n", *time);
	                	}
	            	}
	            	printf("Due Date: %d\n", card.due_date);

		free_card(&card);
        	}
    }

    cJSON_Delete(root);
    free(json_data);
    return 0;
}
