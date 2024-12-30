
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <wchar.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <math.h>

#define CCQ_PATH "/.local/share/ccq/"
#define FACTOR 19.0/81.0 // fsrs factor
#define DECAY -0.5 // fsrs decay

static const float w[] = {
    0.4177, 0, 0.9988, 0, // initial stability for grades A/H/G/E
    7.1949, 0.5345, 1.4604,
    0.0046, 1.54575, 0.1192, 1.01925, 1.9395, 0.11, 0.29605,
    2.2698, 0.2315, 2.9898, 0.51655, 0.6621
};

/* Struct definitions */
typedef struct CorpusEntry {
	char **books;
	int *offsets;
	wchar_t **sentences;
} CorpusEntry;

typedef struct DbEntry {
	wchar_t *key;
	char **readings;
	wchar_t **raw_def;
	char **audiopaths;
	char **imagepaths;
	CorpusEntry sntn_info;
} DbEntry;

typedef struct ScheduleInfo {
	int state;
	float D, S, R;
	int last, due;
} ScheduleInfo;

typedef struct Word {
	DbEntry word;
	ScheduleInfo scheduler;
} Word;

/* Helper functions for scheduling */
float get_d_init(int G) {
    float d_init = w[4] - exp(w[5] * G) + 1;
    return d_init;
}

float get_new_interval(float S) {
    float new_interval = (S / FACTOR) * (pow(0.9, (1 / DECAY)) - 1);
    return new_interval;
}

float get_mean_reversion(float a, float b) {
    float mean_reversion = w[7] * a + (1 - w[7]) * b;
    return mean_reversion;
}

float get_sterm_S(float S, int G) {
    float sterm_S = S * exp(w[17] * (G - 2 + w[18]));
    return sterm_S;
}

float get_R(int t, float S) {
    float R = pow((1 + FACTOR * t / S), DECAY);
    return R;
}

float get_forget_S(float D, float S, float R) {
    float forget_S = w[11] * pow(D, -w[12]) *
        (pow((S + 1), w[13]) - 1) * exp(w[14] * (1 - R));
    return forget_S;
}

float get_recall_S(float D, float S, float R) {
    float recall_S = S * (1 + exp(w[8]) * (11 - D) *
                          pow(S, -w[9]) * (exp(w[10] * (1 - R)) - 1));
    return recall_S;
}

float get_D(float D, int G) {
    float a = get_d_init(4);
    float b = D - w[6] * (G - 3);
    float D_new = get_mean_reversion(a, b);
    return D_new;
}

/* Function definitions */
char *strdup(const char *s) {
	size_t len = strlen(s) + 1;
	char *copy = malloc(len);
	if (!copy) return NULL;
	memcpy(copy, s, len);
	return copy;
}
void dis_raw(struct termios *original) {
	struct termios raw = *original;
	raw.c_lflag &= ~(ECHO | ICANON);
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void en_raw(struct termios *original) {
	tcsetattr(STDIN_FILENO, TCSAFLUSH, original);
}

int get_width(wchar_t wc) {
	return wcwidth(wc);
}

wchar_t *interactive_select(wchar_t **input) {
	struct termios original;
	tcgetattr(STDIN_FILENO, &original);
	dis_raw(&original);

	int cursor_pos = 0;
	int start_select = -1;
	size_t len = wcslen(*input);
	wchar_t *selection = NULL;

	while (1) {
		printf("\033[H\033[J");

		for (int i = 0; i < len; i++) {
			if (start_select != -1 && ((start_select <= i && i <= cursor_pos) || 
					(cursor_pos <= i && i <= start_select))) {
				printf("\033[42m%lc\033[0m", (*input)[i]);
			} else if (i == cursor_pos) {
				printf("\033[7m%lc\033[0m", (*input)[i]);
			} else {
				printf("%lc", (*input)[i]);
			}
		}
		printf("\nUse arrow keys to move (Left/Right), 'v' to toggle selection, Enter to confirm, 'q' to quit.");

		char c = getchar();
		if (c == '\033') {
			getchar();
			switch (getchar()) {
				case 'C':
					if (cursor_pos < len - 1) ++cursor_pos;
					break;
				case 'D':
					if (cursor_pos > 0) --cursor_pos;
					break;
			}
		} else if (c == 'v') {
			if (start_select == -1) {
				start_select = cursor_pos;
			} else {
				start_select = -1;
			}
		} else if (c == '\n') {
			if (start_select != -1) {
				int start = start_select < cursor_pos ? start_select : cursor_pos;
				int end = start_select > cursor_pos ? start_select : cursor_pos;
				size_t selection_len = end - start + 1;

				selection = malloc((selection_len + 1) * sizeof(wchar_t));
				if (!selection) {
					fprintf(stderr, "Memory allocation error for selection\n");
					break;
				}
				wcsncpy(selection, &(*input)[start], selection_len);
				selection[selection_len] = L'\0';
				break;
			}
		} else if (c == 'q') {
			break;
		}
	}

	en_raw(&original);
	return selection;
}

Word build_card(char **sentence) {
	wchar_t *key = interactive_select((wchar_t **)sentence);
	DbEntry db_info = build_db_info(key);
	ScheduleInfo first_sch = {0, 0.0, 0.0, 0.0, 0, 0};
	Word card = {db_info, first_sch};
	return card;
}

wchar_t *decode64(const char *encoded) {
	const char *base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	size_t len = strlen(encoded);
	char *decoded = malloc((len * 3 / 4) + 1);
	if (!decoded) return NULL;

	int val = 0, valb = -8, index = 0;
	for (size_t i = 0; i < len; ++i) {
		char c = encoded[i];
		if (c == '=') break;
		const char *pos = strchr(base64_chars, c);
		if (!pos) continue;
		val = (val << 6) + (pos - base64_chars);
		valb += 6;
		if (valb >= 0) {
			decoded[index++] = (char)((val >> valb) & 0xFF);
			valb -= 8;
		}
	}
	decoded[index] = '\0';

	/* Convert to wchar_t */
	size_t wlen = mbstowcs(NULL, decoded, 0) + 1;
	wchar_t *wdecoded = malloc(wlen * sizeof(wchar_t));
	if (!wdecoded) {
		free(decoded);
		return NULL;
	}
	mbstowcs(wdecoded, decoded, wlen);
	free(decoded);
	return wdecoded;
}


/* Build the DbEntry array from the file */
DbEntry *build_db(FILE *file) {
	DbEntry *db = NULL;
	size_t db_size = 0;
	size_t line_buf_size = 1024;
	char *line = malloc(line_buf_size);

	if (!line) {
		fprintf(stderr, "Memory allocation failed.\n");
		return NULL;
	}

	while (fgets(line, line_buf_size, file)) {
		while (line[strlen(line) - 1] != '\n' && !feof(file)) {
			line_buf_size *= 2;
			line = realloc(line, line_buf_size);
			if (!line) {
				fprintf(stderr, "Memory allocation failed.\n");
				free(db);
				return NULL;
			}
			fgets(line + strlen(line), line_buf_size - strlen(line), file);
		}

		// Check for end of file
		if (strcmp(line, "0\n") == 0) break;

		// Expand DbEntry array
		db = realloc(db, (db_size + 1) * sizeof(DbEntry));
		if (!db) {
			fprintf(stderr, "Memory allocation failed.\n");
			free(line);
			return NULL;
		}

		// Tokenize the line by ';'
		char *token = strtok(line, ";");
		db[db_size].key = (wchar_t *)decode_base64(token);

		// Parse readings
		token = strtok(NULL, ";");
		if (strchr(token, ',')) {
			char *read_tok = strtok(token, ",");
			size_t r_count = 0;
			while (read_tok) {
				db[db_size].readings = realloc(db[db_size].readings, (r_count + 1) * sizeof(char *));
				db[db_size].readings[r_count++] = decode_base64(read_tok);
				read_tok = strtok(NULL, ",");
			}
			db[db_size].readings = realloc(db[db_size].readings, (r_count + 1) * sizeof(char *));
			db[db_size].readings[r_count] = NULL;
		} else {
			db[db_size].readings = malloc(2 * sizeof(char *));
			db[db_size].readings[0] = decode_base64(token);
			db[db_size].readings[1] = NULL;
		}

		// Parse definitions
		token = strtok(NULL, ";");
		if (strchr(token, ',')) {
			char *def_tok = strtok(token, ",");
			size_t d_count = 0;
			while (def_tok) {
				db[db_size].raw_def = realloc(db[db_size].raw_def, (d_count + 1) * sizeof(wchar_t *));
				db[db_size].raw_def[d_count++] = (wchar_t *)decode_base64(def_tok);
				def_tok = strtok(NULL, ",");
			}
			db[db_size].raw_def = realloc(db[db_size].raw_def, (d_count + 1) * sizeof(wchar_t *));
			db[db_size].raw_def[d_count] = NULL;
		} else {
			db[db_size].raw_def = malloc(2 * sizeof(wchar_t *));
			db[db_size].raw_def[0] = (wchar_t *)decode_base64(token);
			db[db_size].raw_def[1] = NULL;
		}

		// Parse audio paths
		token = strtok(NULL, ";");
		if (strchr(token, ',')) {
			char *audio_tok = strtok(token, ",");
			size_t a_count = 0;
			while (audio_tok) {
				db[db_size].audiopaths = realloc(db[db_size].audiopaths, (a_count + 1) * sizeof(char *));
				db[db_size].audiopaths[a_count++] = strdup(audio_tok);
				audio_tok = strtok(NULL, ",");
			}
			db[db_size].audiopaths = realloc(db[db_size].audiopaths, (a_count + 1) * sizeof(char *));
			db[db_size].audiopaths[a_count] = NULL;
		} else {
			db[db_size].audiopaths = malloc(2 * sizeof(char *));
			db[db_size].audiopaths[0] = strdup(token);
			db[db_size].audiopaths[1] = NULL;
		}

		// Parse image paths
		token = strtok(NULL, ";");
		if (strchr(token, ',')) {
			char *img_tok = strtok(token, ",");
			size_t i_count = 0;
			while (img_tok) {
				db[db_size].imagepaths = realloc(db[db_size].imagepaths, (i_count + 1) * sizeof(char *));
				db[db_size].imagepaths[i_count++] = strdup(img_tok);
				img_tok = strtok(NULL, ",");
			}
			db[db_size].imagepaths = realloc(db[db_size].imagepaths, (i_count + 1) * sizeof(char *));
			db[db_size].imagepaths[i_count] = NULL;
		} else {
			db[db_size].imagepaths = malloc(2 * sizeof(char *));
			db[db_size].imagepaths[0] = strdup(token);
			db[db_size].imagepaths[1] = NULL;
		}

		// Parse file offsets
		token = strtok(NULL, ";");
		char *file_tok = strtok(token, ",");
		size_t f_count = 0;
		while (file_tok) {
			db[db_size].sntn_info.books = realloc(db[db_size].sntn_info.books, (f_count + 1) * sizeof(char *));
			db[db_size].sntn_info.offsets = realloc(db[db_size].sntn_info.offsets, (f_count + 1) * sizeof(int));
			sscanf(file_tok, "%[^-],%d", db[db_size].sntn_info.books[f_count], &db[db_size].sntn_info.offsets[f_count]);
			f_count++;
			file_tok = strtok(NULL, ",");
		}
		db[db_size].sntn_info.sentences = NULL; // Default to NULL

		++db_size;
	}

	free(line);
	return db;
}

DbEntry bsw(wchar_t *key, DbEntry *dicdb, int dicdb_size) {
	DbEntry result;
	return result;
}

CorpusEntry bss(CorpusEntry pos_arr) {
	wchar_t **matches;
	int n_match;
	for (int i = 0; i < n_match; ++i)
		pos_arr.sentences[i] = matches[i];
	return pos_arr;
}

DbEntry build_db_info(wchar_t *key) {
	FILE *dictionaries = fopen("dicdb", "r");
	int dicdb_size;
	DbEntry *dicdb = build_dicdb(dictionaries);
	DbEntry match = bsw(key, dicdb, dicdb_size);
	match.sntn_info = bss(match.sntn_info);
	return match;
}

int get_keypress(void) {
	struct termios original;
	int ch;
	tcgetattr(STDIN_FILENO, &original);
	dis_raw(&original);
	ch = getchar();
	en_raw(&original);
	return ch;
}

char *sanitize(char *input_string) {
    size_t len = strlen(input_string);
    char *sanitized_string = malloc(len + 1);
    if (!sanitized_string) {
        fprintf(stderr, "Memory allocation error\n");
        return NULL;
    }

    size_t j = 0;
    for (size_t i = 0; i < len; ++i) {
        char c = input_string[i];
        /* checks if letter, number, _, - , ordd Chinesechar */
        if (isalnum(c) || c == '_' || c == '-' || (c & 0x80)) {
            sanitized_string[j++] = c;
        }
    }
    sanitized_string[j] = '\0';

    return sanitized_string;
}

int play_audio(char *path) {
	char *spath = sanitize(path);
	pid_t pid = fork();
	if (pid == 0) {
		// Child process
		execlp("mpv", "mpv", spath, (char *)NULL);
		exit(EXIT_FAILURE);
	}
	free(spath);
	return 0;
}

int render_image(char *path) {
	char *spath = sanitize(path);
	pid_t pid = fork();
	if (pid == 0) {
		// Child process
		execlp("feh", "feh", spath, (char *)NULL);
		exit(EXIT_FAILURE);
	}
	free(spath);
	return 0;
}

void reveal_card(DbEntry due_card) {
	wprintf(L"Key: %ls\n", due_card.key);

	printf("Readings:\n");
	for (int i = 0; due_card.readings[i] != NULL; ++i) {
		printf("  %s\n", due_card.readings[i]);
	}

	printf("Definitions:\n");
	for (int i = 0; due_card.raw_def[i] != NULL; ++i) {
		wprintf(L"  %ls\n", due_card.raw_def[i]);
	}

	printf("Playing audio files...\n");
	for (int i = 0; due_card.audiopaths[i] != NULL; ++i) {
		printf("  Playing: %s\n", due_card.audiopaths[i]);
		play_audio(due_card.audiopaths[i]);
		sleep(1);
	}

	printf("Rendering images...\n");
	for (int i = 0; due_card.imagepaths[i] != NULL; ++i) {
		printf("  Displaying: %s\n", due_card.imagepaths[i]);
		render_image(due_card.imagepaths[i]);
		sleep(1);
	}

	printf("Sentence Info:\n");
	for (int i = 0; due_card.sntn_info.books[i] != NULL; ++i) {
		printf("  File: %s, Offset: %d\n",
		       due_card.sntn_info.books[i],
		       due_card.sntn_info.offsets[i]);
	}

}

Word *get_due(Word *cards) {
    /* Function to filter due cards */
    for (int num_cards = 0; cards[num_cards] != 0; ++ num_cards)
        ;
  
	time_t now = time(NULL);
	Word *due_cards = malloc(num_cards * sizeof(Word));
	if (!due_cards) {
		fprintf(stderr, "Memory allocation failed.\n");
		*num_due_cards = 0;
		return NULL;
	}

	int count = 0;
  
	for (int i = 0; i < num_cards; ++i) {
		if (cards[i].scheduler.due <= now) {
			due_cards[count++] = cards[i];
		}
	}
  
	*num_due_cards = count;
	due_cards = realloc(due_cards, count * sizeof(Word));

	return due_cards;
}

void schedule_card(Word card, int study_result) {
    ScheduleInfo *old_sch = &card.scheduler;
    int G = study_result == '\n' ? 2 : 0;
    int now = (int)time(NULL);
    float days_since = old_sch->last != 0 ? (now - old_sch->last) / 86400.0 : 0;
    float finterval = 0;

    old_sch->last = now;

    if (old_sch->state == 0) { // new
        old_sch->state = 1;
        old_sch->D = get_d_init(G);
        old_sch->S = w[G];
        old_sch->R = 1.0;
        old_sch->due = now + 86400;
    } else if (old_sch->state == 1 && study_result != '\n') { // young failed
        old_sch->D = get_D(old_sch->D, G);
        old_sch->S = get_recall_S(old_sch->D, old_sch->S, old_sch->R);
        old_sch->R = get_R(days_since, old_sch->S);
        old_sch->due = now + 86400;
    } else if (old_sch->state == 1 && study_result == '\n') { // young pass
        old_sch->state = 2;
        old_sch->D = get_D(old_sch->D, G);
        old_sch->S = get_recall_S(old_sch->D, old_sch->S, old_sch->R);
        old_sch->R = get_R(days_since, old_sch->S);
        finterval = get_new_interval(old_sch->S);
        old_sch->due = now + round(finterval);
    } else if (old_sch->state == 2 && study_result != '\n') { // mature failed
        old_sch->state = 1;
        old_sch->D = get_D(old_sch->D, G);
        old_sch->S = get_forget_S(old_sch->D, old_sch->S, old_sch->R);
        old_sch->R = get_R(days_since, old_sch->S);
        old_sch->due = now + 86400;
    } else if (old_sch->state == 2 && study_result == '\n') { // mature pass
        old_sch->D = get_D(old_sch->D, G);
        old_sch->S = get_recall_S(old_sch->D, old_sch->S, old_sch->R);
        old_sch->R = get_R(days_since, old_sch->S);
        finterval = get_new_interval(old_sch->S);
        old_sch->due = now + round(finterval);
    } else {
        fprintf(stderr, "Mistake in card scheduling\n");
    }
}

void review_cards(FILE *deck) {
	Word *cards;
	int size = get_due(cards);
	int study_result[size];
	for (int i = 0; i < size; ++i) {
		study_result[i] = reveal_card(cards[i].word);
		schedule_card(cards[i], study_result[i]);
	}
}

int main() {
	return 0;
}
