/* 存储器 (ccq) - minimalistic CLI flashcard app */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define CCQ_PATH "/.local/share/ccq/"
#define FACTOR 19.0/81.0 // fsrs factor
#define DECAY -0.5 // fsrs decay

const float w[] = {
    0.4177, 0, 0.9988, 0, // initial stability for grades A/H/G/E
    7.1949, 0.5345, 1.4604,
    0.0046, 1.54575, 0.1192, 1.01925, 1.9395, 0.11, 0.29605,
    2.2698, 0.2315, 2.9898, 0.51655, 0.6621
};

typedef struct ScheduleCard {
    int state;
    float D, S, R;
    int last, due;
} ScheduleCard;


float get_d_init
(int G)
{
    float d_init = w[4] - exp(w[5] * G) + 1;
    return d_init;
}

float get_new_interval
(float S)
{
    float new_interval = (S / FACTOR) * (pow(0.9, (1 / DECAY)) - 1);
    return new_interval;
}

float get_mean_reversion
(float a, float b)
{
    float mean_reversion = w[7] * a + (1 - w[7]) * b;
    return mean_reversion;
}

float get_sterm_S
(float S, int G)
{
    float sterm_S = S * exp(w[17] * (G - 2 + w[18]));
    return sterm_S;
}

float get_R
(int t, float S)
{
    float R = pow((1 + FACTOR * t / S), DECAY);
    return R;
}

float get_forget_S
(float D, float S, float R)
{
    float forget_S = w[11] * pow(D, -w[12]) *
        (pow((S + 1), w[13]) - 1) * exp(w[14] * (1 - R));
    return forget_S;
}

float get_recall_S
(float D, float S, float R, int G)
{
    float recall_S = S * (1 + exp(w[8]) * (11 - D) *
        pow(S, -w[9]) * (exp(w[10] * (1 - R)) - 1));
    return recall_S;
}

float get_D
(float D, int G)
{
    float a = get_d_init(4);
    float b = D - w[6] * (G - 3);
    float D_new = get_mean_reversion(a, b);
    return D_new;
}

ScheduleCard schedule_card
(int result, int state, float D, float S, float R, int last)
{
    ScheduleCard scheduled_card = {0, 0.0, 0.0, 0.0, 0, 0};
    int G = result == '\n' ? 0 : 2; // only grades 0 ("again") and 2 ("good")
    int today = (int)time(NULL);
    float days_since = last != 0 ? (today - last) / 86400 : 0; // 0 if new
    float finterval = 0;

    scheduled_card.last = today;

    if (state == 0) { // new
        scheduled_card.state = 1;
        scheduled_card.D = get_d_init(G);
        scheduled_card.S = w[G];
        scheduled_card.R = 1;
        scheduled_card.due = today + 86400;
    } else if (state == 1 && result != '\n') { // young failed
        scheduled_card.D = get_D(D, G);
        scheduled_card.S = get_forget_S(D, S, R);
        scheduled_card.R = get_R(days_since, S);
        scheduled_card.due = today + 86400;
    } else if (state == 1 && result == '\n') { // young pass
        scheduled_card.state = 2;
        scheduled_card.D = get_D(D, G);
        scheduled_card.S = get_recall_S(D, S, R, G);
        scheduled_card.R = get_R(days_since, S);
        finterval = get_new_interval(scheduled_card.S);
        scheduled_card.due = today + round(finterval);
    } else if (state == 2 && result != '\n') { // mature failed
        scheduled_card.state = 1;
        scheduled_card.D = get_D(D, G);
        scheduled_card.R = get_R(days_since, S);
        scheduled_card.due = today + 86400;
    } else if (state == 2 && result == '\n') {
        scheduled_card.D = get_D(D, G);
        scheduled_card.R = get_R(days_since, S);
        finterval = get_new_interval(scheduled_card.S);
        scheduled_card.due = today + round(finterval);
    }

    return scheduled_card;
}

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

void review_cards
(FILE *deck)
{
    // first pass: count due cards
    int today = (int)time(NULL);
    int i = 0, buffer_size = 1024, length_fields = 42;
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
    printf("Starting review...\n");
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

            // get pass/fail
            printf("\tREVIEW: [%s]\n", front);
            while (1) {
                result = get_keypress();
                if (result == '\n' || result == 'f' || result == 'F') {
                    printf("\t%s\n", back); // TO DO change this into reveal_card void function which also parses media and sentence
                    break;
                } else {
                    printf("Input unrecognized. Please press Enter or F\n");
                }
            }

            // get new scheduling data
            printf("Scheduling card [%s]...\n", front);
            ScheduleCard new = schedule_card(result, state, D, S, R, last);
            char new_fields[43]; // the exact max length 42 + null terminator
            snprintf(new_fields, sizeof(new_fields), "%d|%.3f|%.3f|%.3f|%d|%d\n",
                     new.state, new.D, new.S, new.R, new.last, new.due);
            printf("[%s]\tLENGTH [%lu]\n", new_fields, strlen(new_fields));
            exit(EXIT_SUCCESS);
            fseek(deck, -length_fields, SEEK_CUR);
            fwrite(new_fields, sizeof(char), strlen(new_fields), deck);
            printf("Scheduled card!\n");

        }
    }
    free(line);
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
