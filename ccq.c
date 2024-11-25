/* 存储器 (ccq) - minimalistic CLI flashcard app */

#include <ctype.h>
#include <fcntl.h>
#include <math.h>
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
    FILE *deck = fopen(deck_path, "rb+");
    if (deck == NULL) {
        perror("Failed to read or create deck");
        exit(EXIT_FAILURE);
    }
    printf("Successfully read deck [%s]\n", argv[1]);

    review_cards(deck);

    fclose(deck);
    exit(EXIT_SUCCESS);
}
