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

int height = DB_SIZE;

Card *b_search
(char *key, Card *database, int height)
{
    // musl's implementation
    int try, sign, base = 0;
    while (height > 0) {
        try = base + height / 2;
        sign = strcmp(key, database[try].word.key);
        if (sign < 0) {
            height /= 2;
        } else if (sign > 0) {
            base = try + 1;
            height -= height / 2 + 1;
        } else {
            return &database[try];
        }
    }
    return NULL;
}

//char **retrieve_audio
//(char *hanzi)
//{
//    // this should implement a trie
//    // and build an array of string paths
//    return char **audio_paths;
//}
//
//void add_card
//(FILE *deck, char *hanzi, char *sentence, char *image_path)
//{
//    /* this function will get its arguments from ccq_parse
//     * or directly from command line */
//
//    // add rsync pull here
//    //
//    // get last id
//    fseek(deck, 0, SEEK_END);
//    long fpos = ftell(deck);
//    while (fpos > 0) {
//		--fpos;
//		fseek(deck, fpos, SEEK_SET);
//		if (fgetc(deck) == '\n') {
//			break;
//		}
//	}
//	int buffer = 1024;
//	char *line = malloc(buffer * sizeof(buffer));
//	if (!line) {
//		fprintf(stderr, "Error allocating memory to line\n");
//        return;
//    }
//    int current_size = 0, ch = 0;
//    while ((ch = fgetc(deck)) != '\n' && ch != EOF) {
//        line[++current_size] = ch;
//        if (current_size == buffer) {
//            buffer *= 2;
//            char *new_line = realloc(line, buffer);
//            if (!new_line) {
//                free(line);
//                fprintf(stderr, "Error reallocating memory to line\n");
//                return;
//            }
//            line = new_line;
//        }
//    }
//    line[current_size] = '\0';
//    char *first_pipe = strchr(line, '|');
//    if (!first_pipe) {
//        fprintf(stderr, "No '|' found in line\n");
//        free(line);
//        return;
//    }
//    *first_pipe = '\0'; // null-terminate at |
//    int id = atoi(line) + 1; // so that we can just atoi the line
//    free(line);
//
//    // get dictionary data and audio filepath
//    WordInfo back = retrieve_word(hanzi);
//    int length = strlen(back.reading) + strlen(back.definition) + 1; // +1  because \n
//    char *audio_path = retrieve_audio(hanzi);
//
//	// initialize scheduler data
//	int state = 0;
//	float D = 0.0;
//	float S = 0.0;
//	float R = 0.0;
//	int last = 0;
//	int due = 0;
//
//	// safely write to variable
//	int buffer_size = snprintf(NULL, 0, "%d|%s|%s|%s|%s|%s|%s|%d|%.3f|%.3f|%.3f|%d|%d",
//                               id, hanzi, back.reading, back.definition,
//                               sentence, audio_path, image_path,
//                               0, 0.0f, 0.0f, 0.0f, 0, 0) + 1;
//	char *card_line = malloc(buffer_size);
//	if (card_line == NULL) {
//		perror("Failed to allocate memory for card_line");
//		free(audio_path);
//		return;
//	}
//	snprintf(card_line, buffer_size, "%d|%s|%s|%s|%s|%s|%s|%d|%.3f|%.3f|%.3f|%d|%d",
//             id, hanzi, back.reading, back.definition,
//             sentence, audio_path, image_path,
//             0, 0.0f, 0.0f, 0.0f, 0, 0);
//
//
//	// write to deck
//	fseek(deck, 0, SEEK_END);
//	if (fputs(card_line, deck) == EOF) {
//		perror("Failed to write to deck");
//	}
//	fflush(deck);
//	free(audio_path);
//	free(card_line);
//
//	// add rsync push here
//
//	return;
//}
//
//void add_sentence
//(FILE *deck, char *hanzi, char *sentence)
//{
//	// binary search hanzi in deck's second fields
//	// append sentence to sentence field
//	return;
//}
//
//void ccq_parse
//(char *text)
//{
//	// TUI here with:
//	// - vim keybindings to navigate text
//	// - vim v key to select text
//	// - add_card and add_sentence logic
//	// look-up panel in TUI
//	// recursive tiled panels for nested look-ups
//	return;
//}
