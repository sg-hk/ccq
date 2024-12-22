/* 存储器 (ccq) - minimalistic CLI flashcard app */

#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "ccq.h"

char **parse_bracketed_field
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

DicEntry *build_dic_db
(FILE *dic_db)
{
	DicEntry *database = malloc(DB_SIZE * sizeof(DicEntry));
	int buffer = 1024;
	char *line = malloc(buffer);
	if (!line) {
		fprintf(stderr, "Malloc error on line\n");
		exit(EXIT_FAILURE);
	}
	int i = 0;

	while (fgets(line, sizeof(line), dic_db) != NULL) {
		while (strlen(line) == buffer - 1 && line[buffer - 2] != '\n') {
			buffer *= 2;
			line = realloc(line, buffer);
			if (!line) {
				fprintf(stderr, "Failed to reallocate line buffer\n");
				exit(EXIT_FAILURE);
			}
			fgets(line + strlen(line), buffer - strlen(line), dic_db);
		}

		char *key = strtok(line, "|");
		char *definition = strtok(NULL, "|");
		char *recordings_raw = definition + strlen(definition) + 1;
		if (!key || !definition || !recordings_raw) {
			fprintf(stderr, "Malformed line [%s]", line);
			continue;
		}
		recordings_raw[strlen(recordings_raw) - 1] = '\0';

		// build recordings[] array
		char **recordings = parse_bracketed_field(recordings_raw); 

		database[i].key = strdup(key);
		database[i].definition = strdup(definition);
		database[i].recordings = recordings;
		free(recordings);
		++i;

	}

	free(line);
	return database;
}

Card *build_card_db
(FILE *card_db)
{
	int init_size = 200;
	Card *database = malloc(init_size * sizeof(Card));
	if (!database) {
		fprintf(stderr, "Malloc error on card database\n");
		exit(EXIT_FAILURE);
	}
	int buffer = 1024;
	char *line = malloc(buffer);
	if (!line) {
		fprintf(stderr, "Malloc error on line\n");
		exit(EXIT_FAILURE);
	}
	int i = 0;

	while (fgets(line, sizeof(line), card_db) != NULL) {
		while (strlen(line) == buffer - 1 && line[buffer - 2] != '\n') {
			buffer *= 2;
			line = realloc(line, buffer);
			if (!line) {
				fprintf(stderr, "Failed to reallocate line buffer\n");
				exit(EXIT_FAILURE);
			}
			fgets(line + strlen(line), buffer - strlen(line), card_db);
		}

		char *key = strtok(line, "|");
		char *definition = strtok(NULL, "|");
		char *recordings_raw = definition + strlen(definition) + 1;
		if (!key || !definition || !recordings_raw) {
			fprintf(stderr, "Malformed line [%s]", line);
			continue;
		}
		recordings_raw[strlen(recordings_raw) - 1] = '\0';

		// build recordings[] array
		char **recordings = parse_bracketed_field(recordings_raw);

		database[i].word.key = strdup(key);
		database[i].word.definition = strdup(definition);
		database[i].context.recordings = recordings;
		free(recordings);
		++i;

	}

	free(line);

	return database;
}


int main
(int argc, char **argv)
{
	if (argc != 2) {
		fprintf(stderr, "Usage %s <deck>\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	char deck_path[128];
	snprintf(deck_path, sizeof(deck_path), "%s%s%s", getenv("HOME"), CCQ_PATH, argv[1]);
	printf("Accessing deck [%s]... ", argv[1]);
	FILE *deck = fopen(deck_path, "r+");
	if (deck == NULL) {
		perror("Failed to read or create deck");
		exit(EXIT_FAILURE);
	}
	printf("Successfully read deck [%s]\n", argv[1]);

	review_cards(deck);

	fclose(deck);
	exit(EXIT_SUCCESS);
}
