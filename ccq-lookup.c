#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <time.h>

#define MAX_SMALL 32
#define MAX_PATH 128
#define MAX_LONG 2048
#define DIC_PATH "/.local/share/ccq/all_dictionaries"

char *parse_string
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

void binary_search
(char *search_string, char *dic_path)
{
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
            fgets(return_string, MAX_LONG, dictionaries); // read partial line to align to a new line
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
}

int main
(int argc, char **argv)
{
    char dic_path[MAX_PATH];
    setlocale(LC_ALL, "");

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <string>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    snprintf(dic_path, sizeof(dic_path), "%s%s", getenv("HOME"), DIC_PATH);

    clock_t start = clock();
    binary_search(argv[1], dic_path);
    clock_t end = clock();
    double search_time = (double)(end - start) / CLOCKS_PER_SEC;
    printf("\nBinary search time: %f seconds\n", search_time);

    return 0;
}
