#include <fcntl.h>
#include <locale.h>
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
#include <wchar.h>

#include "config.h"

#define NELEMS(array) (sizeof(array) / sizeof((array)[0]))

/* 
 * CONSTANTS
 */

const char *ccq_dirpath = "/.local/share/ccq/";
const char *db_fpath = "/.local/share/ccq/dictionary_masterfile";
const char *media_dirpath = "/.local/share/ccq/media";

const float factor = 19.0/81.0; // fsrs factor
const float decay = -0.5; // fsrs decay


const float w[] =
{
	0.4177, 0, 0.9988, 0, // initial stability for grades A/H/G/E
	7.1949, 0.5345, 1.4604,
	0.0046, 1.54575, 0.1192, 1.01925, 1.9395, 0.11, 0.29605,
	2.2698, 0.2315, 2.9898, 0.51655, 0.6621
};

/* 
 * STRUCTS
 */

typedef struct CorpusInfo {
	char **files;
	int *bytes;
	wchar_t **sntns;
} CorpusInfo;

typedef struct ScheduleInfo {
	int state;
	float D, S, R;
	int last, due;
} ScheduleInfo;

typedef struct Entry {
	wchar_t *k;
	char **rds;
	wchar_t **defs;
	char **audio_arr;
	char **img_arr;
	CorpusInfo sinfo;
	ScheduleInfo schdl;
} Entry;


/*
 * FUNCTIONS
 */

/* Helper: scheduling */ 
float get_d_init(int G) 
{
    float d_init = w[4] - exp(w[5] * G) + 1;
    return d_init;
}

float get_new_interval(float S) 
{
    float new_interval = (S / factor) * (pow(0.9, (1 / decay)) - 1);
    return new_interval;
}

float get_mean_reversion(float a, float b) 
{
    float mean_reversion = w[7] * a + (1 - w[7]) * b;
    return mean_reversion;
}

float get_sterm_S(float S, int G) 
{
    float sterm_S = S * exp(w[17] * (G - 2 + w[18]));
    return sterm_S;
}

float get_R(int t, float S) 
{
    float R = pow((1 + factor * t / S), decay);
    return R;
}

float get_forget_S(float D, float S, float R) 
{
    float forget_S = w[11] * pow(D, -w[12]) *
        (pow((S + 1), w[13]) - 1) * exp(w[14] * (1 - R));
    return forget_S;
}

float get_recall_S(float D, float S, float R) 
{
    float recall_S = S * (1 + exp(w[8]) * (11 - D) *
                          pow(S, -w[9]) * (exp(w[10] * (1 - R)) - 1));
    return recall_S;
}

float get_D(float D, int G) 
{
    float a = get_d_init(4);
    float b = D - w[6] * (G - 3);
    float D_new = get_mean_reversion(a, b);
    return D_new;
}

ScheduleInfo schedule(ScheduleInfo old_sch, int result)
{
    ScheduleInfo scheduled_card = {0};
    int G = result == '\n' ? 2 : 0; // only grades 0 ("again") and 2 ("good")
    int now = (int)time(NULL);
    float days_since = old_sch.last != 0 ? (now - old_sch.last) / 86400 : 0; // 0 if new
    float finterval = 0;

    scheduled_card.last = now;

    if (old_sch.state == 0) { // new
        scheduled_card.state = 1;
        scheduled_card.D = get_d_init(G);
        scheduled_card.S = w[G];
        scheduled_card.R = 1.0;
        scheduled_card.due = now + 86400;
    } else if (old_sch.state == 1 && result != '\n') { // young failed
        scheduled_card.D = get_D(old_sch.D, G);
        scheduled_card.S = get_recall_S(old_sch.D, old_sch.S, old_sch.R); // _recall not _forget bc young
        scheduled_card.R = get_R(days_since, old_sch.S);
        scheduled_card.due = now + 86400;
    } else if (old_sch.state == 1 && result == '\n') { // young pass
        scheduled_card.state = 2;
        scheduled_card.D = get_D(old_sch.D, G);
        scheduled_card.S = get_recall_S(old_sch.D, old_sch.S, old_sch.R);
        scheduled_card.R = get_R(days_since, old_sch.S);
        finterval = get_new_interval(scheduled_card.S);
        scheduled_card.due = now + round(finterval);
    } else if (old_sch.state == 2 && result != '\n') { // mature failed
        scheduled_card.state = 1;
        scheduled_card.D = get_D(old_sch.D, G);
        scheduled_card.S = get_forget_S(old_sch.D, old_sch.S, old_sch.R);
        scheduled_card.R = get_R(days_since, old_sch.S);
        scheduled_card.due = now + 86400;
    } else if (old_sch.state == 2 && result == '\n') { // mature pass
        scheduled_card.D = get_D(old_sch.D, G);
        scheduled_card.S = get_recall_S(old_sch.D, old_sch.S, old_sch.R);
        scheduled_card.R = get_R(days_since, old_sch.S);
        finterval = get_new_interval(scheduled_card.S);
        scheduled_card.due = now + round(finterval);
    } else {
	    fprintf(stderr, "Mistake in card scheduling\n");
    }

    return scheduled_card;
}

void dis_raw(struct termios *original) {
    struct termios raw = *original;
    raw.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void en_raw(struct termios *original) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, original);
}

int get_keypress(void)
{
	/* function is same as getchar() but without
	 * - echoing char
	 * - requiring Return */
	struct termios original;
	int ch;
	tcgetattr(STDIN_FILENO, &original);
	dis_raw(&original);
	ch = getchar(); // get char in new terminal and switch back to old
	en_raw(&original);
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
				getenv("HOME"), ccq_dirpath, "media/", audio);
		execlp("mpv", "mpv", filepath, (char *)NULL);
		perror("Failed to execute mpv");
		return 1;
	} else {
		int status;
		if (waitpid(pid, &status, 0) == -1) {
			perror("Failed to wait for child mpv process");
			return 1;
		}

		//        waitpid(pid, &status, WNOHANG);
		//        the above would cause to exit early
		if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
			return 0;
		} else {
			fprintf(stderr, "Exited audio playback with error\n");
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
			printf("File path %s has dangerous character: [%c]. ", input, input[i]);
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
	// TO BE IMPLEMENTED: feh/sixel logic
	// system() and not execlp() because sixel needs a separate shell
	// sanitize() prevents shell injections
	char *safe_image = sanitize(image);
	if (safe_image == NULL) {
		return 1;
	}

	char filepath[128];
	snprintf(filepath, sizeof(filepath), "%s%s%s%s",
			getenv("HOME"), ccq_dirpath, "media/", safe_image);
	char command[256];
	snprintf(command, sizeof(command), "magick '%s' -resize 400x400\\> sixel:-",
			filepath); // this does not work on macOS

	int result = system(command);
	free(safe_image);

	if (result == -1) {
		perror("Failed to execute command");
		return 1;
	}
	return 0;
}

wchar_t **itr_tok(wchar_t *string, char *delimiter)
{
	wchar_t **r_arr;
	return r_arr;
}

wchar_t *db64(wchar_t *encoded)
{
	wchar_t *decoded;
	return decoded;
}

wchar_t **parse_sntns(char *sntn_raw)
{
	/*
	 * input expected to be
	 * b64,...
	 */
	wchar_t **s = itr_tok(sntn_raw, ",");
	int n = NELEMS(s);
	for (int i = 0; i < n; i++) {
		s[i] = db64(s[i]);
	}
	return s;
}

wchar_t **parse_fps(char *fp_raw)
{
	wchar_t **fn = itr_tok(fp_raw, ",");
	int n = NELEMS(fn);
	int buffer = 256;

	char *new_fp = malloc(buffer * sizeof(char));
	wchar_t **fp_arr = malloc(n * sizeof (wchar_t*));
	for (int i = 0; i < n; i++) {
		fp_arr[i] = malloc(buffer * sizeof(char));
	}

	for (int i = 0; i < n; i++) {
		new_fp = "\0";
		snprintf(new_fp, sizeof(new_fp), "%s%s%s", getenv("USER"), media_dirpath, fn[i]);
		fp_arr[i] = strdup(new_fp);
	}
	
	return fp_arr;
}

wchar_t **parse_rds(wchar_t *rd_raw)
{
	/* parses a raw ccq reading into a plain wchar** 
	 * the format expected is:
	 * b64,b64,... */
	wchar_t **rd_arr = itr_tok(rd_raw, ",");

	int n = (int)NELEMS(rd_arr);

	for (int i = 0; i < n; i++) {
		rd_arr[i] = db64(rd_arr[i]); /* the decoder has its own sanitize call */
	}

	return rd_arr;
}

wchar_t ***parse_defs(wchar_t *def_raw)
{
	/* parses a raw ccq definition into plain wchar**
	 * the format expected is:
	 * b64,b64,b64;b64,b64,b64;... 
	 * standing for "reading,dictionary, definition" */

	wchar_t **def_arr = itr_tok(def_raw, ";");
	int n = (int)NELEMS(def_arr);

	
	wchar_t *ccq_df[n][3];
	for (int i = 0; i < n; ++i) {
		for (int j = 0; j < 3; j) {
			/* gets a[0] reading, a[1] dictionary name, a[2] definition */
			wchar_t *tok = strdup((wchar_t*)strtok(def_arr[i]), ',');
			ccq_df[i][j] = db64(tok);
		}
	}


	return ccq_df;
}

Entry parse_deck(char *s)
{
	/*
	 * parses ccq formatted decks, as below:
	 * key|pinyin_b64|defs_b64|audio_arr|image_arr|filename-offset,...|schedulingfloats
	 * */

	Entry r;

	/* key in plain text */
	r.k = strdup((wchar_t*)strtok(s, "|"));

	/* array of pinyin strings in b64 */
	wchar_t *rd_raw = strdup((wchar_t*)strtok(NULL, "|"));
	r.rds = parse_rds(rd_raw);

	/* array of hanzi strings in b64 */
	wchar_t *def_raw = strdup((wchar_t*)strtok(NULL, "|"));
	r.defs = parse_defs(def_raw);

	/* array of audio filenames plaintext */
	wchar_t *ofp_raw = strdup((wchar_t*)strtok(NULL, "|"));
	r.audio_arr = parse_fps(ofp_raw);

	/* array of image filenames plaintext */
	wchar_t *ifp_raw = strdup((wchar_t*)strtok(NULL, "|"));
	r.img_arr = parse_fps(ifp_raw);

	/* array of hanzi strings in b64 */
	wchar_t *sntn_raw = strdup((wchar_t*)strtok(NULL, "|"));
	r.sinfo.sntns = parse_sntns(sntn_raw);

	return r;

}

wchar_t *interactive(wchar_t *text)
{
	wchar_t *selection;
	return selection;
}


void print(Entry c)
{
	printf("[k]%ls\n[r]%ls\n[d]%ls\n", c.k, c.rds[0], c.defs[0], c.sinfo.sntns[0]);
	play_audio(c.audio_arr[0]);
	render_image(c.img_arr[0]);
}

void phelp(char *prog_name)
{
	printf("Usage: %s flag <argument>\n", prog_name);
	printf("\t-a <key>\n");
	printf("\t-d <key>\n");
	printf("\t-r <deck>\n");
	printf("\t-s <key>\n");
	printf("\t<text>\n");
	printf("\n");
	printf("For more detailed information see man ccq\n");
}


/* Essential functions */ 
void create(wchar_t *key)
{
	// ask user for deck name
	// bsearch(deck) if not NULL return error
	// bsearch(db) if NULL return error
	// Entry newc = parse_db(char *db_match_line)
	// if (newc.sinfo.books)
	//	newc.sinfo.sentences = search_corpus(newc.sinfo);
	// else print_warning("no sentences found")
	// newc.scheduler = first_sch();
}

void delete(wchar_t *key)
{
	// ask user for deck name
	// bsearch(deck) if NULL return error, exit 
	// parse_deck(line)
	// print(card)
	// get_keypress() y or n(exit)
	// remove line in file
}

void search(wchar_t *key)
{
	// ask user for deck name
	// bsearch(deck) if not NULL parse, print
	// bsearch(db) if not NULL parse, print
	// if (db) search_corpus(parsed_info);
	// exit
}


void review(char *deck)
{
	FILE *d = fopen("deck", "r");
	if (!d)
		fprintf(stderr, "Deck open error\n");

	int today = (int)time(NULL);
	int i = 0, buffer = 1024;
	char *line = malloc(buffer * sizeof(char));
	while (fgets(line, buffer, d) != NULL) {
		while ((int)strlen(line) == buffer - 1 && line[buffer - 2] != '\n') {
			// - 1 because fgets stop there to guarantee space for \0
			buffer *= 2;
			char *new_line = realloc(line, buffer);
			if (!new_line) {
				free(line);
				fprintf(stderr, "Memory allocation failure for new_line\n");
				exit(EXIT_FAILURE);
			}
			line = new_line;
			if (!fgets(line + buffer / 2 - 1, buffer / 2 + 1, d)) break;
			// rereading right before midpoint of new buffer (one before end of previous)
			// to overwrite the \0 placed by the previous fgets call
		}
		if (strrchr(line, '|')) {
			int date = atoi(strrchr(line, '|') + 1);
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
	rewind(d);
	printf("Starting review... ");
	printf("Press Enter (pass) or R (fail)\n");
	// re-initialize buffer and line
	buffer = 1024;
	line = malloc(buffer);
	if (!line) {
		fprintf(stderr, "Memory allocation failure for line\n");
		exit(EXIT_FAILURE);
	}

	char result;
	while (fgets(line, buffer, d) != NULL) {
		// realloc logic
		while ((int)strlen(line) == buffer - 1 && line[buffer - 2] != '\n') {
			buffer *= 2;
			char *new_line = realloc(line, buffer);
			if (!new_line) {
				free(line);
				fprintf(stderr, "Memory allocation failure for new_line\n");
				exit(EXIT_FAILURE);
			}
			line = new_line;
			if (!fgets(line + buffer / 2 - 1, buffer / 2 + 1, d)) break;
		}

		int date = atoi(strrchr(line, '|') + 1);
		if (date < today) {
			Entry due_card = parse_deck(line);
			while (1) {
				result = get_keypress();
				if (result == '\n' || result == 'f' || result == 'F') {
					print(due_card);
					schedule(due_card, deck);
					break;
				} else
					printf("Input unrecognized. Please press Enter of F\n");
			}
		}
	}

			
}


int main(int argc, char **argv)
{
	if (argc != 2 && argc != 3) {
		fprintf(stderr, "Argument error...Exiting\n\n");
		phelp(argv[0]);
		exit(1);
	}


	if (argc == 2) { // ccq <text strings>
		wchar_t *key = interactive((wchar_t *)argv[1]);
		printf("[a]dd or [s]earch %ls?\n", key);
		int result = get_keypress();
		if (result == 'a')
			create(key);
		else if (result == 's')
			search(key);
	}

	char *deck = NULL;
	wchar_t *key = NULL;
	char *pos_arg = NULL;
	int opt;

	while ((opt = getopt(argc, argv, "r:d:s:a:h")) != -1) {
		switch (opt) {
		case 'r':
			deck = optarg;
			review((wchar_t*)deck);
			break;
		case 'd':
			key = optarg;
			delete((wchar_t *)key);
			break;
		case 's':
			key = (wchar_t*)optarg;
			search(key);
			break;
		case 'a':
			key = (wchar_t*)optarg;
			create(key);
			break;
		case 'h':
			phelp(argv[0]);
			exit(0);
		default:
			exit(1);
		}
	}

	if (optind < argc)
		pos_arg = argv[optind];

	return 0;
}
