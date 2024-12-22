#include <stdio.h>
#include <stdlib.h>
#include <string.h>


char **parse_brackets
(char *field)
{
    field[strlen(field) - 1] = '\0'; // remove ]
    char *content = field + 1; // remove [

    int buffer_size = 10;
    int count = 0;
    char **result = malloc(buffer_size * sizeof(char *));
    if (!result) {
        fprintf(stderr, "Memory allocation failed for bracket parsing\n");
        exit(EXIT_FAILURE);
    }

    char *token = strtok(content, "|");
    while (token) {
        if (count >= buffer_size) {
            buffer_size *= 2;
            result = realloc(result, buffer_size * sizeof(char *));
            if (!result) {
                fprintf(stderr, "Memory reallocation failed for bracket parsing\n");
                exit(EXIT_FAILURE);
            }
        }

        result[count++] = strdup(token); // not ++count
        if (!result[count - 1]) {
            fprintf(stderr, "Memory allocation failed for token duplication\n");
            exit(EXIT_FAILURE);
        }

        token = strtok(NULL, "|");
    }

    result[++count] = NULL; // for easy iteration. and not count++

    return result;
}


int main
(void)
{
	char path_dic[128];
	snprintf(path_dic, 128, "%s%s", getenv("HOME"), "/code/ccq/test_dic");
	FILE *test_dic = fopen(path_dic, "r");
	if (!test_dic) {
		fprintf(stderr, "Path malformed\n");
		exit(EXIT_FAILURE);
	}

	char path_wrd[128];
	snprintf(path_wrd, 128, "%s%s", getenv("HOME"), "/code/ccq/test_wrd");
	FILE *test_wrd = fopen(path_wrd, "r");
	if (!test_wrd) {
		fprintf(stderr, "Path malformed\n");
		exit(EXIT_FAILURE);
	}

	char line[256];
	// parse dic
	while (fgets(line, sizeof(line), test_dic) != NULL) {
		char *key = strtok(line, "|");
		char *definition = strtok(NULL, "|");
		char *recordings_raw = NULL;
		char **recordings = parse_brackets(recordings_raw);
	}
	// parse wrd 
	while (fgets(line, sizeof(line), test_wrd) != NULL) {
		char *key = strtok(line, "|");
		char *definition = strtok(NULL, "|");
		char *recordings_raw = NULL;
		char **recordings = parse_brackets(recordings_raw);
		char *sentences_raw = NULL;
		char **sentences = parse_brackets(sentences_raw);
		char *images_raw = NULL;
		char **images = parse_brackets(images_raw);
	}

	return 0;
}
