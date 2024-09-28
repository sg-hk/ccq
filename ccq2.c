/*	存储器 (ccq) by sg-hk

	ccq does five things
 	1. collect due cards
	2. parse due cards
	3. review due cards
	4. store review data
	5. reschedule cards		*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include "cJSON.h"

#define CCQ_PATH "/.local/share/ccq/"
#define INITIAL_CAPACITY 128
#define MAX_FIELD_LENGTH 512

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

int get_due_date(int result, int old_due_date, time_t today)
{
	/* one day is 86400 seconds */
	time_t new_due_date = (result == '\n' && old_due_date != 0) ? // if passed & not new card
    		today + (today - old_due_date) * 2 : // set new date = old + interval*2
		today + 86400; // otherwise reschedule to the next day
	return new_due_date;
}

int get_keypress
(void)
{
	struct termios oldt, newt;
	int ch;
	tcgetattr(STDIN_FILENO, &oldt);
	newt = oldt;
	newt.c_lflag &= ~(ICANON | ECHO);
	tcsetattr(STDIN_FILENO, TCSANOW, &newt);
	ch = getchar(); // get char in new terminal and switch back to old
	tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
	return ch;
}

void reveal_card
(cJSON *word)
{
	cJSON *audiofiles_json = cJSON_GetArrayItem(word,3);
	cJSON *sentences_json = cJSON_GetArrayItem(word, 4);
	int audiofile_count = cJSON_GetArraySize(audiofiles_json);
	char **audiofiles = parse_string_array(audiofiles_json);
	char **sentences = parse_string_array(sentences_json);

	// play a random file from the array
	char play_mpv[256];
	srand(time(NULL));
	int rdraw = (rand() % audiofile_count) - 1;
	snprintf(play_mpv, sizeof(play_mpv), "mpv --really-quiet '%s' >/dev/null 2>&1 &", audiofiles[rdraw]);
	system(play_mpv);

	// show back of card
	printf("Reading: %s\n", cJSON_GetArrayItem(word, 1)->valuestring);
	printf("Definition: %s\n", cJSON_GetArrayItem(word, 2)->valuestring);
	printf("Sentences:\n");
	for (int i = 0; sentences[i] != NULL; ++i) {
		printf("        %s\n", sentences[i]);
	}
}

void update_log
(cJSON *log, int result, int time_taken, time_t today)
{
	cJSON *dates = cJSON_GetArrayItem(log, 0);
	cJSON *results = cJSON_GetArrayItem(log, 1);
	cJSON *times = cJSON_GetArrayItem(log, 2);

	cJSON_AddItemToArray(dates, cJSON_CreateNumber(today));
	cJSON_AddItemToArray(results, cJSON_CreateNumber(result)); // would making it a string be better?
	cJSON_AddItemToArray(times, cJSON_CreateNumber(time_taken));
	
	int old_due_date = cJSON_GetArrayItem(log, 3)->valueint;
	int new_due_date = get_due_date(result, old_due_date, today);
	cJSON_ReplaceItemInArray(log, 3, cJSON_CreateNumber(new_due_date));
}

void review_card
(cJSON *word, cJSON *log, time_t todays)
{
	int result;
	clock_t start, end;
	int time_taken;

	printf("Reviewing [%s]...\n", cJSON_GetArrayItem(word, 0)->valuestring);
	printf("Press Enter (pass) or F (fail)\n");

	start = clock();

	while (1) {
		result = get_keypress();
		if (result == '\n' || result == 'f') {
			end = clock();
			time_taken = (int)((double)(end - start) * 1000 / CLOCKS_PER_SEC); // in milliseconds
			reveal_card(word);
			update_log(log, result, time_taken, todays);
			break;
		} else {
			printf("Input unrecognized. Please press Enter or F");
		}
	}
}

void write_file_from_string
(const char *filepath, const char *updated_data)
{
	FILE *file = fopen(filepath, "w");
	if (file == NULL) {
        	perror("Failed to open file");
        	exit(EXIT_FAILURE);
    	}

	if (fprintf(file, "%s", updated_data) < 0) {
        	perror("Failed to write to file");
        	fclose(file);
        	exit(EXIT_FAILURE);
    	}

	fclose(file);
}

int main 
(int argc, char **argv)
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

	time_t today = time(NULL);
	cJSON *card = NULL;
	cJSON_ArrayForEach(card, example_deck) {
		if (card->type == cJSON_Object) {
		        cJSON *word = cJSON_GetObjectItemCaseSensitive(card, "word"); 
		        cJSON *log = cJSON_GetObjectItemCaseSensitive(card, "log"); 
			int due_date = cJSON_GetArrayItem(log,3)->valueint; 
			if (due_date < today) {
				review_card(word, log, today);
			}
		}
	}

	printf("All cards reviewed!\n");
	free(json_data);

	printf("Updating %s...\n", argv[1]);
	char *updated_json_data = cJSON_Print(root);
	write_file_from_string(filepath, updated_json_data);
	
	cJSON_Delete(root);
	free(updated_json_data);
	return 0;
}
