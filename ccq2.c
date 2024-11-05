/* 存储器 (ccq) - minimalistic CLI flashcard app */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define CCQ_PATH "/.local/share/ccq/"

typedef struct ScheduleCard {
    int state;
    float D, S, R;
    int last, due;
} ScheduleCard;

int get_keypress
(void)
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

ScheduleCard schedule_card
(float *w, int result, int state, float D, float S, float R, int last)
{
    ScheduleCard scheduled_card = {0, 0.0, 0.0, 0.0, 0, 0};
    int G = result == '\n' ? 0 : 2; // only grades 0 ("again") and 2 ("good")
    int today = (int)time(NULL);
    float days_since = (today - last) / 86400;

    scheduled_card.last = today;

    if (state == 0) {
        float d_new = w[4] - exp(w[5] * G) + 1;
        scheduled_card.D = d_new;
        scheduled_card.S = w[G];
        scheduled_card.R = 1;
    } else {
        float d_upd = D + (-w[6] * (G - 2)) * ((10 - D) / 9);
        float s_upd = S * (exp(w[8]) * (11 - D) * powf(S, -w[9]) * (exp(w[10] * (1 - R)) - 1) + 1);
        float r_upd = powf((1 + 19/81 + days_since / S), -0.5);
        scheduled_card.D = d_upd;
        scheduled_card.S = s_upd;
        scheduled_card.R = r_upd;
    }

    if (result == '\n') {
        scheduled_card.state = 1;
    } else {
        scheduled_card.state = 2;
    }

    float finterval = (19 * scheduled_card.S) / 81 * (powf(0.9, (-1/0.5)) - 1);
    int interval = round(finterval);
    scheduled_card.due = last + interval;

    return scheduled_card;
}

void review_cards
(FILE *deck)
{
    // first pass: count due cards
    int today = (int)time(NULL);
    int i = 0;
    int buffer_size = 1024;
    char *line = malloc(buffer_size);
    if (!line) {
        fprintf(stderr, "Memory allocation failure for line\n");
        exit(EXIT_FAILURE);
    }

    printf("Counting due cards...\n");
    while (fgets(line, sizeof(line), deck) != NULL) {
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
            printf("CARD DUE: %d\nTODAY: %d\n", date, today);
            if (date < today) ++i;
        } else {
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
    printf("Starting review...\n");
    printf("Press Enter (pass) or R (fail)\n");
    char result;
    float w[] = {
        0.4177, 0, 0.9988, 0, // initial stability for grades A/H/G/E
        7.1949, 0.5345, 1.4604,
        0.0046, 1.54575, 0.1192, 1.01925, 1.9395, 0.11, 0.29605,
        2.2698, 0.2315, 2.9898, 0.51655, 0.6621
    };

    while (fgets(line, sizeof(line), deck) != NULL) {
        int date = atoi(strrchr(line, '|') + 1);
        if (date < today) {
            // main
            char *id_char = strtok(line, "|");
            char *front = strtok(NULL, "|");
            char *back = strtok(NULL, "|");
            if (id_char == NULL || front == NULL || back == NULL) {
                fprintf(stderr, "Line malformed: [%s]\n", line);
                exit(EXIT_FAILURE);
            }

            // context
            char *sentence = strtok(NULL, "|");
            char *audio = strtok(NULL, "|");
            char *image = strtok(NULL, "|");

            // scheduling & conversions
            char *state_char = strtok(NULL, "|");
            char *difficulty_char = strtok(NULL, "|");
            char *stability_char = strtok(NULL, "|");
            char *retrievability_char = strtok(NULL, "|");
            char *last_review_char = strtok(NULL, "|");
            int state = atoi(state_char); // note atoi returns 0 on failure
            float D = atof(difficulty_char); // same, 0.0
            float S = atof(stability_char);
            float R = atof(retrievability_char);
            int last = atoi(last_review_char);

            // length for file pointer positioning
            int length = 0;

            // get pass/fail
            printf("\tREVIEW: [%s]\n", front);
            while (1) {
                result = get_keypress();
                if (result == '\n' || result == 'f' || result == 'F') {
                    printf("\t%s\n", back);
                    break;
                } else {
                    printf("Input unrecognized. Please press Enter or F\n");
                }
            }

            // get new scheduling data
            ScheduleCard new = schedule_card(w, result, state, D, S, R, last);
            fseek(deck, -length, SEEK_CUR);
            char new_fields[length];
            snprintf(new_fields, sizeof(new_fields), "%d|%f|%f|%f|%d|%d\n",
                     new.state, new.D, new.S, new.R, new.last, new.due);
            fwrite(new_fields, sizeof(char), strlen(new_fields), deck);

        }
    }
    printf("Successfully reviewed all due cards\n\n");
    return;
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
    printf("Accessing deck [%s]...\n", argv[1]);
    FILE *deck = fopen(deck_path, "rb+");
    if (deck == NULL) {
        perror("Failed to read or create deck");
        exit(EXIT_FAILURE);
    }
    printf("Successfully read deck [%s]\n\n", argv[1]);

    review_cards(deck);

    fclose(deck);
    exit(EXIT_SUCCESS);
}
