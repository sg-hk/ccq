/* 存储器 (ccq) - minimalistic CLI flashcard app */

#include <ctype.h>
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

#define CCQ_PATH "/.local/share/ccq/"
#define FACTOR 19.0/81.0 // fsrs factor
#define DECAY -0.5 // fsrs decay

const float w[] = {
    0.4177, 0, 0.9988, 0, // initial stability for grades A/H/G/E
    7.1949, 0.5345, 1.4604,
    0.0046, 1.54575, 0.1192, 1.01925, 1.9395, 0.11, 0.29605,
    2.2698, 0.2315, 2.9898, 0.51655, 0.6621
};

typedef struct WordInfo {
	char *reading;
	char *definition;
} WordInfo;

typedef struct ScheduleCard {
	int state;
	float D, S, R;
	int last, due;
} ScheduleCard;

/* SCHEDULING FUNCTIONS */
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
(float D, float S, float R)
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
    int G = result == '\n' ? 2 : 0; // only grades 0 ("again") and 2 ("good")
    int now = (int)time(NULL);
    float days_since = last != 0 ? (now - last) / 86400 : 0; // 0 if new
    float finterval = 0;

    scheduled_card.last = now;

    if (state == 0) { // new
        scheduled_card.state = 1;
        scheduled_card.D = get_d_init(G);
        scheduled_card.S = w[G];
        scheduled_card.R = 1.0;
        scheduled_card.due = now + 86400;
    } else if (state == 1 && result != '\n') { // young failed
        scheduled_card.D = get_D(D, G);
        scheduled_card.S = get_recall_S(D, S, R); // _recall not _forget bc young
        scheduled_card.R = get_R(days_since, S);
        scheduled_card.due = now + 86400;
    } else if (state == 1 && result == '\n') { // young pass
        scheduled_card.state = 2;
        scheduled_card.D = get_D(D, G);
        scheduled_card.S = get_recall_S(D, S, R);
        scheduled_card.R = get_R(days_since, S);
        finterval = get_new_interval(scheduled_card.S);
        scheduled_card.due = now + round(finterval);
    } else if (state == 2 && result != '\n') { // mature failed
        scheduled_card.state = 1;
        scheduled_card.D = get_D(D, G);
        scheduled_card.S = get_forget_S(D, S, R);
        scheduled_card.R = get_R(days_since, S);
        scheduled_card.due = now + 86400;
    } else if (state == 2 && result == '\n') { // mature pass
        scheduled_card.D = get_D(D, G);
        scheduled_card.S = get_recall_S(D, S, R);
        scheduled_card.R = get_R(days_since, S);
        finterval = get_new_interval(scheduled_card.S);
        scheduled_card.due = now + round(finterval);
    }

    return scheduled_card;
}

/* REVIEWING FUNCTIONS */
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

int play_audio
(char *audio)
{
    // execlp() and not system() because safer
    pid_t pid = fork();
    if (pid == -1) {
        perror("Failed to fork");
        return 1;
    }

    if (pid == 0) {
        char filepath[128];
        snprintf(filepath, sizeof(filepath), "%s%s%s%s",
                 getenv("HOME"), CCQ_PATH, "media/", audio);
        execlp("mpv", "mpv", "--really-quiet", filepath, "2>&1");
        perror("Failed to execute mpv");
        exit(EXIT_FAILURE);
    } else {
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            return 0;
        } else {
            return 1;
        }
    }
}

char *sanitize
(char *input)
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

int render_image
(char *image)
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

int reveal_card
(char *front, char *back, char *sentence, char *audio, char *image)
{
    printf("FRONT:\n\t%s\n", front);
    printf("BACK:\n\t%s\n", back); // here maybe deserialize the \n
    printf("SENTENCE:\n\t%s\n", sentence);
    printf("AUDIO:\n");
    if (play_audio(audio) != 0) {
        fprintf(stderr, "Error playing audio file\n");
        return 1;
    }
    printf("IMAGE:\n");
    if (render_image(image) != 0) {
        fprintf(stderr, "Error displaying image file\n");
        return 1;
    }
    return 0;
}

void review_cards
(FILE *deck)
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
            while (1) {
                result = get_keypress();
                if (result == '\n' || result == 'f' || result == 'F') {
                    if (reveal_card(front, back, sentence, audio, image) == 0) {
                        isSuccessful = true;
                    }
                    break;
                } else {
                    printf("Input unrecognized. Please press Enter or F\n");
                }
            }

            // get new scheduling data
            if (isSuccessful) {
                printf("Scheduling card [%s]...\n", id_char);
                ScheduleCard new = schedule_card(result, state, D, S, R, last);
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

/* ADD, UPDATE, PARSE FUNCTIONS */
WordInfo retrieve_word
(char *hanzi)
{
	// lookup hanzi in hash table
	// open matched dictionary files
	// retrieve second (reading) and third (definition) fields
	// concatenate definitions by reading and add dic name
	char *reading;
	char *definition;
	WordInfo word_info = {reading, definition};
	return word_info;
}

char *retrieve_audio
(char *hanzi)
{
	char *audio_path = hanzi;
	return audio_path;
}

void add_card
(FILE *deck, int last_id, char *hanzi, char *reading, char *definition,
 char *sentence, char *image_path)
{
	// add rsync pull here
	// get card data
	int id = last_id + 1;
	WordInfo back = retrieve_word(hanzi);
	int length = strlen(reading) + strlen(definition) + 1; // + \n
	char *audio_path = retrieve_audio(hanzi);
	int state = 0;
	float D = 0.0;
	float S = 0.0;
	float R = 0.0;
	int last = 0;
	int due = 0;

	// safely write to variable
	int buffer_size = snprintf(NULL, 0, "%d|%s|%s|%s|%s|%s|%s|%d|%.3f|%.3f|%.3f|%d|%d",
			id, hanzi, back.reading, back.definition,
			sentence, audio_path, image_path,
			0, 0.0f, 0.0f, 0.0f, 0, 0) + 1;
	char *card_line = malloc(buffer_size);
	if (card_line == NULL) {
		perror("Failed to allocate memory for card_line");
		free(audio_path);
		return;
	}
	snprintf(card_line, buffer_size, "%d|%s|%s|%s|%s|%s|%s|%d|%.3f|%.3f|%.3f|%d|%d",
			id, hanzi, back.reading, back.definition,
			sentence, audio_path, image_path,
			0, 0.0f, 0.0f, 0.0f, 0, 0);


	// write to deck
	fseek(deck, 0, SEEK_END);
	if (fputs(card_line, deck) == EOF) {
		perror("Failed to write to deck");
	}
	fflush(deck);
	free(audio_path);
	free(card_line);
	// add rsync push here
	// return to main functions
	return;
}

void add_sentence
(FILE *deck, char *hanzi, char *sentence)
{
	// binary search hanzi in deck's second fields
	// append sentence to sentence field
	return;
}

void ccq_parse
(char *text)
{
	// TUI here with:
	// - vim keybindings to navigate text
	// - vim v key to select text
	// - add_card and add_sentence logic
	// look-up panel in TUI
	// recursive tiled panels for nested look-ups
	return;
}


/* MAIN FUNCTION */
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
