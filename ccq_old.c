/* 存储器 (ccq) by sg-hk
 * a minimalistic vocabulary app in C
 * - flash card reviewer and scheduler
 * - dictionary lookups */

#include <ctype.h>
#include <locale.h>
#include <ncurses.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>
#include "cJSON.h"

#define CCQ_PATH "/.local/share/ccq/"
#define MAX_PATH 128
#define MAX_LONG 2048


/* ccq-review functions start here */

char *strdup // needs to be defined separately in >c99
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

time_t get_due_date
(int result, time_t old_due_date, time_t today)
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
        char filepath[128];
        snprintf(filepath, sizeof(filepath), "%s%s%s%s", getenv("HOME"), CCQ_PATH, "media/", audiofile);
        execlp("mpv", "mpv", "--really-quiet", filepath, "2>&1");
        perror("Failed to execute mpv");
        return;
    } else perror("Failed to fork");
}

char *sanitize
(char *input)
{
    /* malloc needs to be used so that memory is allocated on the heap
     * stack memory sanitized[128] doesn't persist beyond function */
    char *sanitized = malloc(128);
    if (!sanitized) {
        perror("Failed to allocate memory for sanitizing");
        return NULL;
    }
    int j = 0;

    for (int i = 0; input[i] != '\0' && j < 127; ++i) {
        if (isalnum(input[i] || input[i] == '.' || input[i] == '/' || input[i] == '_')) {
            sanitized[++j] = input[i];
        } else {
            printf("Image file path %s has dangerous characters\n", input);
            printf("File skipped...\n");
            return NULL;
        }
    }
    sanitized[j] = '\0';
    return sanitized;
}

void render_imagefile
(char *imagefile)
{
    // system() and not execlp() because sixel needs a separate shell
    // steps are taken to prevent shell injections
    imagefile = sanitize(imagefile);
    if (imagefile) {
        char filepath[128];
        snprintf(filepath, sizeof(filepath), "%s%s%s%s", getenv("HOME"), CCQ_PATH, "media/", imagefile);
        char command[256];
        snprintf(command, sizeof(command), "magick '%s' -resize 400x400\\> sixel:-", imagefile);
        system(command);
    }
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

        for (int i = 0; i < audiofile_count; ++i) {
            free(audiofiles[i]);
        }
        free(audiofiles);
    }
    if (imagefiles_json) {
        char **imagefiles = parse_string_array(imagefiles_json);
        int imagefile_count = cJSON_GetArraySize(imagefiles_json);
        srand(time(NULL));
        int rdraw = (rand() % imagefile_count - 1);
        render_imagefile(imagefiles[rdraw]);

        for (int i = 0; i < imagefile_count; ++i) {
            free(imagefiles[i]);
        }
        free(imagefiles);
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

void review
(char *deck_name)
{
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
    
    cJSON *sub_decks = NULL;
    cJSON_ArrayForEach(sub_decks, root) {
        printf(" - %s\n", sub_decks->string);
    }
    char sub_deck[64];
    fgets(sub_deck, sizeof(sub_deck), stdin);
    sub_deck[strcspn(sub_deck, "\n")] = '\0'; // removing newline character

    cJSON *sub_deck_object = cJSON_GetObjectItemCaseSensitive(root, sub_deck);
    if (!sub_deck_object) {
        fprintf(stderr, "Error accessing %s\n", sub_deck);
        exit(EXIT_FAILURE);
    } else printf("The chosen deck %s has correctly been accessed\n", sub_deck);

    time_t today = time(NULL);
    int i = 0;
    cJSON *card = NULL;
    cJSON_ArrayForEach(card, sub_deck_object) {
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

    printf("Updating %s...\n", sub_deck);
    char *updated_json_data = cJSON_Print(root);
    write_file_from_string(filepath, updated_json_data);

    cJSON_Delete(root);
    free(updated_json_data);

    return;
}

/* ccq-parse functions start here */

char *parse_by_char
(char *search_string)
{
    /* this function parses multibyte strings character by character:
     * UTF-8 encodes characters in 1-4 byte sequences
     * and the number of leading 1s in the first byte reflects this
     * 0... = 1 byte, 11110... = 4 bytes
     * hence using bitwise & we can determine bytes per char */

    char *parsed_string = malloc(40); // max dictionary search is 10 chars, 4 bytes each
    if (!parsed_string) {
        perror("Failed to allocate memory to parsed_string");
        return NULL;
    }

    unsigned char *p = (unsigned char *)search_string; // UTF-8 uses full 0-255 range
    int bytes_in_char, i = 0;
    while (*p) {
        if (*p < 128) {  // if p < 128, leading bit is 0, single byte char
            bytes_in_char = 1;
        // the masks below check for both the leading 1s and a 0 following them
        } else if ((*p & 0xE0) == 0xC0) {  // 0xE0 = 1110 0000; 0xC0 = 1100 0000
            bytes_in_char = 2;
        } else if ((*p & 0xF0) == 0xE0) {  // 0xF0 = 1111 0000; 0xE0 = 1110 0000
            bytes_in_char = 3;
        } else if ((*p & 0xF8) == 0xF0) {  // 0xF8 = 1111 1000; 0xF0 = 1111 0000
            bytes_in_char = 4;
        } else {  // unexpected byte in UTF-8 sequence
            fprintf(stderr, "Error in UTF-8 encoding\n");
            free(parsed_string);
            return NULL;
        }

        if (i + bytes_in_char >= 100) {
            fprintf(stderr, "Parsed string exceeds buffer size\n");
            free(parsed_string);
            return NULL;
        }
        memcpy(&parsed_string[i], p, bytes_in_char);
        i += bytes_in_char;
        p += bytes_in_char; // move pointer to next char
    }

    parsed_string[i] = '\0';
    return parsed_string;
}

wchar_t **build_search_array
(wchar_t **parsed_string, int len)
{
    wchar_t **search_array = malloc(sizeof(MAX_LONG));
    // truncate the parsed_string

    // some regex to remove punctuation
    // some regex to have a copy of the string without intermediate chars
    
    return search_array;
}

void binary_search
(char *search_string, char *dic_path)
{
    clock_t start = clock();
    
    int low = 0, mid = 0, high = 0;
    char return_string[MAX_LONG];
    char *found_key, *reading, *definition;
    bool found = false;

    FILE *dictionaries = fopen(dic_path, "r");
    if (!dictionaries) {
        perror("Error opening dictionary database");
        exit(EXIT_FAILURE);
    }

    fseek(dictionaries, 0, SEEK_END);
    high = ftell(dictionaries) - 1;

    while (low <= high && !found) {
        mid = (low + high) / 2;
        fseek(dictionaries, mid, SEEK_SET);

        if (mid != 0) {
            fgets(return_string, MAX_LONG, dictionaries);
        }

        if (!fgets(return_string, MAX_LONG, dictionaries)) {
            printf("fgets fails to read a line\n");
            printf("Debug: Last Read string: '%s'\n", return_string);
            break;
        }

        found_key = strtok(return_string, "|");
        if (found_key) {
            int comparison = strcmp(found_key, search_string);
            if (comparison == 0) {
                found = true;
                reading = strtok(NULL, "|");
                definition = strtok(NULL, "|");
                printf("Reading:\n\t%s\n", reading);
                printf("Definition:\n\t%s\n", definition);
            } else if (comparison < 0) {
                low = mid + 1;
            } else {
                high = mid - 1;
            }
        }
    }

    if (!found) {
        printf("Key '%s' not found.\n", search_string);
    }

    fclose(dictionaries);
    clock_t end = clock();
    double search_time = (double)(end - start) / CLOCKS_PER_SEC;
    printf("\nBinary search time: %f seconds\n", search_time);

    return;
}

void parse
(char *to_be_parsed, char *dic_path)
{
    wchar_t input[1024];
    mbstowcs(input, to_be_parsed, 1024);
    int len = wcslen(input);
    if (len == 1) {
        binary_search(to_be_parsed, dic_path);
        return;
    }

    initscr();
    noecho();
    cbreak();
    keypad(stdscr, TRUE);

    // offsets to move cursor correctly in TUI
    int offsets[1024];
    int byte_offset = 0;
    for (int i = 0; i < len; ++i) {
        offsets[i] = byte_offset;
        byte_offset += wcwidth(input[i]); // length of hanzi should be 2
    }

    mvaddwstr(0, 0, input); // move cursor to 0,0 and add the string "input"
    move(0, 0); // move cursor back to 0,0

    int ch, x = 0;
    while ((ch = getch()) != '\n') {
        switch (ch) {
            case KEY_LEFT:
                if (x > 0) x--;
                break;
            case KEY_RIGHT:
                if (x < len - 1) x++;
                break;
        }
        move(0, offsets[x]);  // move cursor to correct offset index
    }

    endwin(); // exit TUI

    /* transform the string in an array of chunks, such that
     * "my name is john" becomes
     * ["my", "my name", "my name is", "my name is john"]
     * and finally do dictionary searches on each element */
     
    wchar_t return_string[len];
    swprintf(return_string, len, &input[x]); // the desired substring
    return_array = parse_by_char(return_string); // split up by character
    recursive_array = build_search_array(return_array, len); // and arranged incrementally
    for (int i = 0; i < len; ++i) {
        binary_search(return_array[i]);
    }

    return;
}

void browse_deck // NOT CODED YET
(char* deck_name)
{
    if (cJSON_IsArray(sub_deck) || !cJSON_IsArray(sub_deck)) {
        return;
    }
}

void print_help()
{
    printf("ccq detailed usage:\n");
    printf("\t -b: browse the deck <string>\n");
    printf("\t -h: print this help message and exit\n");
    printf("\t -p <string>: parse the string <string> for dictionary lookups\n");
    printf("\t -r <string>: review the deck <string>\n");
    printf("\nConsult man ccq for further information\n");
    return;
}

int main 
(int argc, char **argv)
{
    setlocale(LC_ALL, "");

    printf("Welcome to ccq v0.02 - a minimalistic flash card program in C\n");

    bool browse_flag = false, help_flag = false, parse_flag = false, review_flag = false;
    char *to_be_parsed = NULL, *deck_name = NULL, dic_path[MAX_PATH];

    if (argc != 0) {
        int opt;
        while ((opt = getopt(argc, argv, "b:hp:r")) != -1) {
            switch(opt) {
                case 'b':
                    deck_name = optarg;
                    browse_flag = true;
                    break;
                case 'h':
                    help_flag = true;
                case 'p':
                    to_be_parsed = optarg;
                    parse_flag = true;
                    break;
                case 'r':
                    deck_name = optarg;
                    review_flag = true;
                default:
                    fprintf(stderr, "Usage: %s [-b <string>] [-p <string>] [-r <string>]\n", argv[0]);
                    exit(EXIT_FAILURE);
            }
        }

        if (browse_flag + parse_flag + review_flag + help_flag > 1) {
            fprintf(stderr, "You can only enable one flag at a time\n");
            exit(EXIT_FAILURE);
        }

        snprintf(dic_path, sizeof(dic_path), "%s%s%s", getenv("HOME"), CCQ_PATH, "all_dictionaries");

        if (browse_flag) {
            browse_deck(deck_name);
        } else if (help_flag) {
            print_help();
        } else if (parse_flag) {
            parse(to_be_parsed, dic_path);
        } else if (review_flag) {
            review(deck_name);
        }
    }

    exit(EXIT_SUCCESS);
}
