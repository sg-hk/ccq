/* The reviewing functions:
 * * search entire word database
 * * gather due
 * * review: display data, gather user input
 * * pass on card's ScheduleInfo and user input to scheduler */

#include <ctype.h>
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

int get_keypress(void)
{
    /* function is same as getchar() but without
     * - echoing char
     * - requiring Return */
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

int play_audio(char *audio)
{
    // execlp() and not system() because safer
    pid_t pid = fork();
    if (pid == -1) {
        perror("Failed to fork");
        return 1;
    }

    if (pid == 0) {
        int dev_null = open("/dev/null", O_WRONLY);
        if (dev_null == -1) {
            perror("Failed to open /dev/null");
            return 1;
        }
        dup2(dev_null, STDOUT_FILENO);
        close(dev_null);

        char filepath[128];
        snprintf(filepath, sizeof(filepath), "%s%s%s%s",
                 getenv("HOME"), CCQ_PATH, "media/", audio);
        execlp("mpv", "mpv", filepath, (char *)NULL);
        perror("Failed to execute mpv");
        return 1;
    } else {
        int status;
        waitpid(pid, &status, WNOHANG);
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            return 0;
        } else {
            return 1;
        }
    }
}

char *sanitize(char *input)
{
    char *sanitized = malloc(128);
    if (!sanitized) {
        perror("Memory allocation error for sanitized filepath");
        return NULL;
    }

    for (int i = 0, j = 0; input[i] != '\0' && j < 127; ++i) {
        if (isalnum(input[i]) || input[i] == '.' || input[i] == '/' || input[i] == '_' ||
            input[i] == '-') {
            sanitized[j++] = input[i];  // increment j AFTER using it
        } else {
            printf("Image file path %s has dangerous character: [%c]. ", input, input[i]);
            printf("File skipped...\n");
            free(sanitized);
            return NULL;
        }
        if (input[i+1] == '\0') {
            sanitized[j] = '\0';
        }
    }

    return sanitized;
}

int render_image(char *image)
{
    // system() and not execlp() because sixel needs a separate shell
    // sanitize() prevents shell injections
    char *safe_image = sanitize(image);
    if (safe_image == NULL) {
        return 1;
    }

    char filepath[128];
    snprintf(filepath, sizeof(filepath), "%s%s%s%s",
             getenv("HOME"), CCQ_PATH, "media/", safe_image);
    char command[256];
    snprintf(command, sizeof(command), "magick '%s' -resize 400x400\\> sixel:-",
             filepath);

    int result = system(command);
    free(safe_image);

    if (result == -1) {
        perror("Failed to execute command");
        return 1;
    }
    return 0;
}

int reveal_card(Card *card)
{
    printf("FRONT:\n\t%s\n", card->word.key);
    printf("DEFINITION:\n\t%s\n", card->word.definition); // here maybe deserialize the \n
    printf("SENTENCE:\n\t%s\n", card->context.sentences[0]);
    printf("AUDIO:\n");
    if (play_audio(card->context.recordings[0]) != 0) {
        fprintf(stderr, "Error playing audio file\n");
        return 1;
    }
    printf("IMAGE:\n");
    if (render_image(card->context.images[0]) != 0) {
        fprintf(stderr, "Error displaying image file\n");
        return 1;
    }
    return 0;
}

void review_cards(FILE *deck)
{
    // first pass: count due cards
    int today = (int)time(NULL);
    int i = 0, buffer_size = 1024, length_fields = 42;
    bool isSuccessful = false;
    char *line = malloc(buffer_size);
    if (!line) {
        fprintf(stderr, "Memory allocation failure for line\n");
        exit(EXIT_FAILURE);
    }

    printf("Counting due cards... ");
    while (fgets(line, buffer_size, deck) != NULL) {
        while ((int)strlen(line) == buffer_size - 1 && line[buffer_size - 2] != '\n') {
            // - 1 because fgets stop there to guarantee space for \0
            buffer_size *= 2;
            char *new_line = realloc(line, buffer_size);
            if (!new_line) {
                free(line);
                fprintf(stderr, "Memory allocation failure for new_line\n");
                exit(EXIT_FAILURE);
            }
            line = new_line;
            if (!fgets(line + buffer_size / 2 - 1, buffer_size / 2 + 1, deck)) break;
            // rereading right before midpoint of new buffer (one before end of previous)
            // to overwrite the \0 placed by the previous fgets call
        }
        if (strrchr(line, '|')) {
            int date = atoi(strrchr(line, '|') + 1);
            if (date < today) ++i;
        } else {
            fprintf(stderr, "ERROR HERE\n");
            fprintf(stderr, "Line malformed: [%s]\n", line);
            free(line);
            exit(EXIT_FAILURE);
        }
    }

    free(line);

    if (i == 0) {
        printf("No cards to review\n");
        exit(EXIT_SUCCESS);
    }

    printf("Successfully counted [%d] due cards\n\n", i);

    // second pass: review cards
    rewind(deck);
    printf("Starting review... ");
    printf("Press Enter (pass) or R (fail)\n");
    // re-initialize buffer and line
    buffer_size = 1024;
    line = malloc(buffer_size);
    if (!line) {
        fprintf(stderr, "Memory allocation failure for line\n");
        exit(EXIT_FAILURE);
    }

    char result;
    while (fgets(line, buffer_size, deck) != NULL) {
        while ((int)strlen(line) == buffer_size - 1 && line[buffer_size - 2] != '\n') {
            buffer_size *= 2;
            char *new_line = realloc(line, buffer_size);
            if (!new_line) {
                free(line);
                fprintf(stderr, "Memory allocation failure for new_line\n");
                exit(EXIT_FAILURE);
            }
            line = new_line;
            if (!fgets(line + buffer_size / 2 - 1, buffer_size / 2 + 1, deck)) break;
        }

        int date = atoi(strrchr(line, '|') + 1);
        if (date < today) {
            Card curr = {{NULL, NULL}, {NULL, NULL, NULL},
                         {0, 0.0, 0.0, 0.0, 0, date}};
            // word info
            curr.word.key = strtok(line, "|");
            curr.word.definition = strtok(NULL, "|");
            if (curr.word.key == NULL || curr.word.definition == NULL) {
                fprintf(stderr, "Line malformed: [%s]\n", line);
                exit(EXIT_FAILURE);
            }

            // context info
	    int c_init = 3;
	    curr.context.sentences = malloc(c_init * sizeof(char*));
	    curr.context.recordings = malloc(c_init * sizeof(char*));
	    curr.context.images = malloc(c_init * sizeof(char*));

	    if (!curr.context.sentences || !curr.context.recordings || !curr.context.images) {
		    fprintf(stderr, "Memory allocation failed\n");
		    exit(EXIT_FAILURE);
	    }

            curr.context.sentences[0] = strtok(NULL, "|");
	    printf("Sentences ok\n");
            curr.context.recordings[0] = strtok(NULL, "|");
	    printf("Recordings ok\n");
            curr.context.images[0] = strtok(NULL, "|");
	    printf("Images ok\n");

            // schedule info & conversions
            char *state_char = strtok(NULL, "|");
            char *difficulty_char = strtok(NULL, "|");
            char *stability_char = strtok(NULL, "|");
            char *retrievability_char = strtok(NULL, "|");
            char *last_review_char = strtok(NULL, "|");
            curr.schedule.state = atoi(state_char); // note atoi returns 0 on failure
            curr.schedule.D = atof(difficulty_char); // same, 0.0
            curr.schedule.S = atof(stability_char);
            curr.schedule.R = atof(retrievability_char);
            curr.schedule.last = atoi(last_review_char);

            // get pass/fail
            while (1) {
                result = get_keypress();
                if (result == '\n' || result == 'f' || result == 'F') {
                    if (reveal_card(&curr) == 0) {
                        isSuccessful = true;
                    }
                    break;
                } else {
                    printf("Input unrecognized. Please press Enter or F\n");
                }
            }

            // get new scheduling data
            if (isSuccessful) {
                printf("Scheduling card [%s]...\n", curr.word.key);
                ScheduleInfo new = schedule_card(curr.schedule, result);
                char new_fields[43]; // the length 42 (constant) + null terminator
                snprintf(new_fields, sizeof(new_fields), "%d|%.3f|%.3f|%.3f|%d|%d\n",
                         new.state, new.D, new.S, new.R, new.last, new.due);
                fseek(deck, -length_fields, SEEK_CUR);
                fwrite(new_fields, sizeof(char), strlen(new_fields), deck);
                printf("Scheduled card!\n");
            } else {
                fprintf(stderr, "Card was not scheduled\n");
            }

        }
    }
    free(line);
    printf("Successfully reviewed all due cards\n\n");
    return;
}
