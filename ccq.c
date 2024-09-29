/*	存储器 (ccq) by sg-hk

	ccq does four things
	1. collect due cards
	2. review due cards
	3. store review data
	4. reschedule cards		*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
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

time_t get_due_date(int result, time_t old_due_date, time_t today)
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

void play_audiofile
(char *audiofile)
{
    // execlp() and not system() because safer
    pid_t pid = fork();
    if (pid == 0) {
        execlp("mpv", "mpv", "--really-quiet", audiofile, "2>&1");
        perror("Failed to execute mpv");
        return;
    }
    else perror("Failed to fork");
}

void render_imagefile
(char *imagefile)
{
    // system() and not execlp() because sixel needs a separate shell
    char command[128];
    snprintf(command, sizeof(command), "magick '%s' -resize 400x400\\> sixel:-", imagefile);
    system(command);
}

void reveal_card
(cJSON *data, cJSON *files)
{
    // show card (text) data
    int n_data_fields = cJSON_GetArraySize(data);
    for (int i = 0; i < n_data_fields; ++i) {
        cJSON* field = cJSON_GetArrayItem(data, i);
        if (cJSON_IsArray(field)) {
            int n_subfields = cJSON_GetArraySize(field);
            for (int j = 0; j < n_subfields; ++j) {
                cJSON* subfield = cJSON_GetArrayItem(field, j);
                if (!cJSON_IsString(subfield)) {
                    printf("Error: subfield isn't string value");
                    return;
                }
                printf("%s\n", subfield->valuestring);
            }
        } else {
            if (!cJSON_IsString(field)) {
                printf("Error: field isn't string value");
                return;
            }
            printf("%s\n", field->valuestring);
        }
    }

    // play/render files
    cJSON *audiofiles_json = cJSON_GetArrayItem(files,0);
    cJSON *imagefiles_json = cJSON_GetArrayItem(files,1);
    if (audiofiles_json) {
        char **audiofiles = parse_string_array(audiofiles_json);
        int audiofile_count = cJSON_GetArraySize(audiofiles_json);
        srand(time(NULL));
        int rdraw = (rand() % audiofile_count - 1);
        play_audiofile(audiofiles[rdraw]);
    }
    if (imagefiles_json) {
        char **imagefiles = parse_string_array(imagefiles_json);
        int imagefile_count = cJSON_GetArraySize(imagefiles_json);
        srand(time(NULL));
        int rdraw = (rand() % imagefile_count - 1);
        render_imagefile(imagefiles[rdraw]);
    }
}

void update_log
(cJSON *log, int result, int time_taken, time_t today)
{
	cJSON *dates = cJSON_GetArrayItem(log, 0);
	cJSON *results = cJSON_GetArrayItem(log, 1);
	cJSON *times = cJSON_GetArrayItem(log, 2);

	cJSON_AddItemToArray(dates, cJSON_CreateNumber(today));
	cJSON_AddItemToArray(results, cJSON_CreateString(result == '\n' ? "pass" : "fail"));
	cJSON_AddItemToArray(times, cJSON_CreateNumber(time_taken));
	
	time_t old_due_date = (time_t)cJSON_GetArrayItem(log, 3)->valueint;
	time_t new_due_date = get_due_date(result, old_due_date, today);
	cJSON_ReplaceItemInArray(log, 3, cJSON_CreateNumber(new_due_date));
}

void review_card
(cJSON *data, cJSON *log, cJSON *files, time_t today)
{
	int result;
	struct timeval start, end;
	int time_taken;

	printf("Reviewing [%s]...\n", cJSON_GetArrayItem(data, 0)->valuestring);
	printf("Press Enter (pass) or f (fail)\n");

	gettimeofday(&start, NULL);

	while (1) {
		result = get_keypress();
		if (result == '\n' || result == 'f' || result == 'F') {
			gettimeofday(&end, NULL);
			time_taken = (int)((end.tv_sec - start.tv_sec) * 1000 + (end.tv_usec - start.tv_usec) / 1000);
			reveal_card(data, files);
			update_log(log, result, time_taken, today);
			break;
		} else {
			printf("Input unrecognized. Please press Enter or F\n");
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
	printf("Welcome to ccq v0.02 - a minimalistic flash card program in C\n");

    char filepath[256];
    snprintf(filepath, sizeof(filepath), "%s%s%s", getenv("HOME"), CCQ_PATH, "master.json");

    printf("Accessing master deck at %s...\n", filepath);
    char *json_data = read_file_to_string(filepath);
    if (!json_data) {
        fprintf(stderr, "Error reading file\n");
        exit(EXIT_FAILURE);
    } else {
        printf("Successfully read file\n");
    }

    printf("\nParsing master deck's json structure...\n");
    cJSON *root = cJSON_Parse(json_data);
    if (!root) {
        fprintf(stderr, "Error parsing JSON at: %s\n", cJSON_GetErrorPtr());
        fprintf(stderr, "Please try validating the JSON through an appropriate tool\n");
        exit(EXIT_FAILURE);
    }
    printf("Successfully parsed structure\n\n");
    free(json_data);

	printf("Please input the deck you would like to study:\n");
    cJSON *sub_decks = NULL;
    cJSON_ArrayForEach(sub_decks, root) {
        printf(" - %s\n", sub_decks->string);
    }
	char study_deck[64];
	fgets(study_deck, sizeof(study_deck), stdin);
	study_deck[strcspn(study_deck, "\n")] = '\0'; // removing newline character
	
    cJSON *sub_deck = cJSON_GetObjectItemCaseSensitive(root, study_deck);
    if (!sub_deck) {
        fprintf(stderr, "Error accessing %s\n", study_deck);
        exit(EXIT_FAILURE);
    } else printf("Today's study deck is %s\n", study_deck);


	time_t today = time(NULL);
	int i = 0;
	cJSON *card = NULL;
	cJSON_ArrayForEach(card, sub_deck) {
        ++i;
		if (card->type == cJSON_Object) {
            cJSON *data = cJSON_GetObjectItemCaseSensitive(card, "card");
			if (!data) printf("The %d-th card has a malformed 'data' array\n", i);
            cJSON *log = cJSON_GetObjectItemCaseSensitive(card, "log");
			if (!log) printf("The %d-th card has a malformed 'log' array\n", i);
            cJSON *files = cJSON_GetObjectItemCaseSensitive(card, "files");
			if (!files) printf("The %d-th card has a malformed 'files' array\n", i);

			int due_date = cJSON_GetArrayItem(log,3)->valueint; 
			if (due_date < today) {
				review_card(data, log, files, today);
			}
		}
		else printf("The %d-th card is malformed\n", i);
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
