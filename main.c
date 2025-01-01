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

typedef struct SchNum {
	int state;
	float D, S, R;
	int last, due;
} SchNum;

typedef struct Entry {
	wchar_t *k;
	wchar_t **rds;
	wchar_t **defs;
	wchar_t **audio_arr;
	wchar_t **img_arr;
	wchar_t *pos_arr; // fn-byte,fn-byte
	wchar_t **sntn_arr;
	SchNum schdl;
} Entry;

/*
 * FUNCTIONS
 */

/* Utils: general */

int compare_key(const void *key, const void *line)
{
	/*
	 * used in bsearch and qsort 
	 */

	wchar_t *f = wcsdup(*(wchar_t**)line);
	wchar_t *k = wcstok(f, L"|");
	int res = wcscmp((wchar_t*)key, k);
	free(f);
	return res;
}

wchar_t *ec64(wchar_t *p)
{
	/*
	 * encodes wchar_t * to wchar_t * base 64
	 */
	static char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	size_t sl = wcslen(p), ml = sl * 4 + 1;
	char *mb = malloc(ml), *outb;
	if(!mb) return NULL;
	wcstombs(mb, p, ml);
	{
		int n = 0, i = 0;
		while(mb[n]) n++;
		int olen = (n+2)/3*4;
		outb = malloc(olen+1);
		if(!outb){ free(mb); return NULL; }
		int j=0; for(; i<n; i+=3) {
			unsigned long v = mb[i]<<16;
			if(i+1<n) v |= mb[i+1]<<8;
			if(i+2<n) v |= mb[i+2];
			outb[j++] = b64[(v>>18)&63];
			outb[j++] = b64[(v>>12)&63];
			outb[j++] = (i+1<n)?b64[(v>>6)&63]:'=';
			outb[j++] = (i+2<n)?b64[v&63]:'=';
		}
		outb[j] = 0;
	}
	free(mb);
	{
		size_t wl = mbstowcs(NULL, outb, 0) + 1;
		wchar_t *r = malloc(wl*sizeof(wchar_t));
		if(!r){ free(outb); return NULL; }
		mbstowcs(r, outb, wl);
		free(outb);
		return r;
	}
}

wchar_t *dc64(wchar_t *enc)
{
	/*
	 * decodes wchar_t * base 64 input as wchar_t * plain text
	 */

	static int t[256] = { 
		['A']=0, ['B']=1, ['C']=2, ['D']=3, ['E']=4, ['F']=5, ['G']=6, ['H']=7,
		['I']=8, ['J']=9, ['K']=10,['L']=11,['M']=12,['N']=13,['O']=14,['P']=15,
		['Q']=16,['R']=17,['S']=18,['T']=19,['U']=20,['V']=21,['W']=22,['X']=23,
		['Y']=24,['Z']=25,['a']=26,['b']=27,['c']=28,['d']=29,['e']=30,['f']=31,
		['g']=32,['h']=33,['i']=34,['j']=35,['k']=36,['l']=37,['m']=38,['n']=39,
		['o']=40,['p']=41,['q']=42,['r']=43,['s']=44,['t']=45,['u']=46,['v']=47,
		['w']=48,['x']=49,['y']=50,['z']=51,['0']=52,['1']=53,['2']=54,['3']=55,
		['4']=56,['5']=57,['6']=58,['7']=59,['8']=60,['9']=61,['+']=62,['/']=63
	};

	size_t wl = wcslen(enc);
	size_t bl = wl * 4 + 1;
	char *nb = malloc(bl);
	if (!nb) return NULL;
	mbstowcs(NULL, NULL, 0); /* reset state */
	wcstombs(nb, enc, bl);

	int len = 0;
	for (int i = 0; nb[i]; i++) {
		if (nb[i] == '=' || nb[i] == '\r' || nb[i] == '\n') break;
		len++;
	}

	int pad = (nb[len-1] == '=') + (nb[len-2] == '=');
	int out_size = (len * 3) / 4 - pad;
	unsigned char *db = malloc(out_size + 1);
	if (!db) {
		free(nb);
		return NULL;
	}

	int j = 0;
	for (int i = 0; i < len; i += 4) {
		int v = (t[(unsigned char)nb[i]]   << 18) +
		        (t[(unsigned char)nb[i+1]] << 12) +
		        (t[(unsigned char)nb[i+2]] << 6)  +
		         t[(unsigned char)nb[i+3]];
		db[j++] = (v >> 16) & 0xFF;
		if (nb[i+2] != '=') db[j++] = (v >> 8) & 0xFF;
		if (nb[i+3] != '=') db[j++] = v & 0xFF;
	}
	db[out_size] = 0;

	free(nb);

	size_t wc_needed = mbstowcs(NULL, (char*)db, 0) + 1;
	wchar_t *out = malloc(wc_needed * sizeof(wchar_t));
	if (!out) {
		free(db);
		return NULL;
	}
	mbstowcs(out, (char*)db, wc_needed);
	free(db);
	return out;
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
	/* getchar() without echo and Return */
	struct termios original;
	int ch;
	tcgetattr(STDIN_FILENO, &original);
	dis_raw(&original);
	ch = getchar(); // get char in new terminal and switch back to old
	en_raw(&original);
	return ch;
}

/* Utils: scheduling */
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
			exit(1);
		}
		dup2(dev_null, STDOUT_FILENO);
		close(dev_null);

		char filepath[128];
		snprintf(filepath, sizeof(filepath), "%s%s%s%s",
				getenv("HOME"), ccq_dirpath, "media/", audio);
		execlp("mpv", "mpv", filepath, (char *)NULL);
		perror("Failed to execute mpv");
		exit(1);
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

wchar_t *sanitize(char *input)
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

	return (wchar_t *)sanitized;
}

int render_image(char *image)
{
	// TO BE IMPLEMENTED: feh/sixel logic
	// system() and not execlp() because sixel needs a separate shell
	// sanitize() prevents shell injections
	wchar_t *safe_image = sanitize(image);
	if (safe_image == NULL) {
		return 1;
	}

	char filepath[128];
	snprintf(filepath, sizeof(filepath), "%s%s%ls",
			getenv("HOME"), media_dirpath, safe_image);
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

wchar_t **itr_tok(wchar_t *s, char *d)
{
        /*
         * iterative tokeniser for wide strings
         * returns wchar_t** (NULL-terminated)
         */

        size_t dl = strlen(d);
        wchar_t *wd = malloc((dl + 1) * sizeof(wchar_t));
        if (!wd) return NULL;
        mbstowcs(wd, d, dl + 1);

        wchar_t **ra = NULL;
        wchar_t *save, *tok;
        int c = 0;

        tok = wcstok(s, wd, &save);
        while (tok) {
                wchar_t **tmp = realloc(ra, sizeof(wchar_t*) * (c + 2));
                if (!tmp) {
                        free(ra);
                        free(wd);
                        return NULL;
                }
                ra = tmp;
                ra[c] = wcsdup(tok);
                c++;
                ra[c] = NULL;
                tok = wcstok(NULL, wd, &save);
        }

        free(wd);
        return ra;
}

Entry parse(wchar_t *s)
{
	Entry e;
	wchar_t **f = itr_tok(s, "|");

	e.k = wcsdup(f[0]);

	{
		wchar_t **x = itr_tok(f[1], ",");
		int c = 0; while (x[c]) c++;
		e.rds = malloc((c + 1) * sizeof(wchar_t*));
		for (int i = 0; i < c; i++) e.rds[i] = dc64(x[i]);
		e.rds[c] = NULL;
	}

	{
		wchar_t **x = itr_tok(f[2], ",");
		int c = 0; while (x[c]) c++;
		e.defs = malloc((c + 1) * sizeof(wchar_t*));
		for (int i = 0; i < c; i++) e.defs[i] = dc64(x[i]);
		e.defs[c] = NULL;
	}

	e.audio_arr = itr_tok(f[3], ",");
	e.img_arr   = itr_tok(f[4], ",");

	{
		wchar_t **x = itr_tok(f[5], ",");
		int c = 0; while (x[c]) c++;
		e.sntn_arr = malloc((c + 1) * sizeof(wchar_t*));
		for (int i = 0; i < c; i++) e.sntn_arr[i] = dc64(x[i]);
		e.sntn_arr[c] = NULL;
	}

	{
		wchar_t **x = itr_tok(f[6], ",");
		e.schdl.state = wcstol(x[0], NULL, 10);
		e.schdl.D     = wcstof(x[1], NULL);
		e.schdl.S     = wcstof(x[2], NULL);
		e.schdl.R     = wcstof(x[3], NULL);
		e.schdl.last  = wcstol(x[4], NULL, 10);
		e.schdl.due   = wcstol(x[5], NULL, 10);
	}

	e.pos_arr = NULL;
	return e;
}

wchar_t *write(Entry e)
{
	wchar_t *buf = malloc(1024*sizeof(wchar_t));
	if(!buf) return NULL;
	buf[0]=0;
	{
		wcscat(buf,e.k);
		wcscat(buf,L"|");
	}
	{
		wchar_t tmp[512]; tmp[0]=0;
		for(int i=0;e.rds && e.rds[i];i++){
			wchar_t *enc = ec64(e.rds[i]);
			wcscat(tmp,enc); free(enc);
			if(e.rds[i+1]) wcscat(tmp,L",");
		}
		wcscat(buf,tmp);
		wcscat(buf,L"|");
	}
	{
		wchar_t tmp[512]; tmp[0]=0;
		for(int i=0;e.defs && e.defs[i];i++){
			wchar_t *enc = ec64(e.defs[i]);
			wcscat(tmp,enc); free(enc);
			if(e.defs[i+1]) wcscat(tmp,L",");
		}
		wcscat(buf,tmp);
		wcscat(buf,L"|");
	}
	{
		wchar_t tmp[512]; tmp[0]=0;
		for(int i=0;e.audio_arr && e.audio_arr[i];i++){
			wcscat(tmp,e.audio_arr[i]);
			if(e.audio_arr[i+1]) wcscat(tmp,L",");
		}
		wcscat(buf,tmp);
		wcscat(buf,L"|");
	}
	{
		wchar_t tmp[512]; tmp[0]=0;
		for(int i=0;e.img_arr && e.img_arr[i];i++){
			wcscat(tmp,e.img_arr[i]);
			if(e.img_arr[i+1]) wcscat(tmp,L",");
		}
		wcscat(buf,tmp);
		wcscat(buf,L"|");
	}
	{
		wchar_t tmp[512]; tmp[0]=0;
		for(int i=0;e.sntn_arr && e.sntn_arr[i];i++){
			wchar_t *enc = ec64(e.sntn_arr[i]);
			wcscat(tmp,enc); free(enc);
			if(e.sntn_arr[i+1]) wcscat(tmp,L",");
		}
		wcscat(buf,tmp);
		wcscat(buf,L"|");
	}
	{
		wchar_t tmp[64];
		swprintf(tmp,64,L"%d,%f,%f,%f,%d,%d", e.schdl.state, e.schdl.D, e.schdl.S, e.schdl.R, e.schdl.last, e.schdl.due);
		wcscat(buf,tmp);
	}
	return buf;
}

char *ask_user(void)
{
	wprintf(L"Enter deck name: ");
	fflush(stdout);
	wchar_t buf[256] = {0};
	fgetws(buf, sizeof(buf) / sizeof(buf[0]), stdin);
	buf[wcslen(buf) - 1] = L'\0';
	wchar_t *r = wcsdup(buf);
	r = sanitize(r);
	return r;
}

wchar_t *interactive(wchar_t *text)
{
	int len = wcslen(text);
	int start = -1, end = -1, pos = 0;
	wchar_t *selection = NULL;

	while (1) {
		/* Clear screen and print the text with cursor position */
		printf("\033[H\033[J"); // Clear screen
		for (int i = 0; i < len; i++) {
			if (i == pos) {
				printf("\033[7m%lc\033[0m", text[i]); /* Highlight cursor position */
			} else if (start != -1 && i >= start && i <= end) {
				printf("\033[4m%lc\033[0m", text[i]); /* Underline selection */
			} else {
				printf("%lc", text[i]);
			}
		}
		printf("\n");

		int ch = get_keypress();
		if (ch == 'v') {
			if (start == -1) {
				start = pos;
				end = pos;
			} else {
				end = pos;
			}
		} else if (ch == '\n') {
			if (start != -1 && end != -1) {
				int sel_len = end - start + 1;
				selection = malloc((sel_len + 1) * sizeof(wchar_t));
				if (!selection) {
					perror("Memory allocation error for selection");
					return NULL;
				}
				wcsncpy(selection, text + start, sel_len);
				selection[sel_len] = L'\0';
				return selection;
			}
		} else if (ch == 27) { // Escape sequence
			get_keypress(); // Skip [
			ch = get_keypress();
			if (ch == 'C' && pos < len - 1) { // Right arrow
				pos++;
			} else if (ch == 'D' && pos > 0) { // Left arrow
				pos--;
			}
		} else {
			return NULL;
		}
	}
}

void print(Entry c)
{
	printf("[k]%ls\n[r]%ls\n[d]%ls\n", c.k, c.rds[0], c.defs[0], c.sntn_arr[0]);
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

wchar_t *fsearch(wchar_t *k, wchar_t *fn, int n)
{
        /*
         * searches for key in file,
         * assumes sorted lines of entries formatted as
         * key|other_field|...\n
         * returns matched line if found
         * NULL otherwise
         */

	char fp[256];
	snprintf(fp, sizeof(fp), "%s%s%s", getenv("USER"), ccq_dirpath, (char*)fn);
	FILE *f = fopen(fp, "r");
	if (!f) {
		return NULL;
	}

	wchar_t **ls = malloc(n * sizeof(wchar_t*));
	wchar_t buf[1024], *r, *m;
	int i = 0;

	while (i < n && fgetws(buf, sizeof(buf) / sizeof(buf[0]), f)) {
		size_t len = wcslen(buf);
		if (len && buf[len - 1] == L'\n') {
			buf[len - 1] = L'\0'; /* trim newline */
		}
		ls[i++] = wcsdup(buf);
	}
	fclose(f);

	r = bsearch(&k, ls, i, sizeof(wchar_t*), compare_key);
	if (r) {
		m = wcsdup(*(wchar_t**)r);
	} else {
		m = NULL;
	}

	for (int j = 0; j < i; j++) {
		free(ls[j]);
	}
	free(ls);

	return m;
}

/* Essential functions */ 
void create(wchar_t *key)
{
	char *deck = ask_user(); // sanitized in function
	char filepath[256];
	snprintf(filepath, sizeof(filepath), "%s%s%s", getenv("USER"), ccq_dirpath, deck);
	FILE *d = fopen(filepath, "w");
	if (!d) {
		fprintf(stderr, "Deck open error at %s\n", filepath);
		return;
	}
	if (bsearch(key, deck)) {
		fprintf(stderr, "Card [%ls] already exists\n", key);
		return;
	}
	if (char *line_match = bsearch(key, db)) {
		fprintf(stderr, "Card [%ls] not found in dictionaries\n", key);
		return;
	}
	
	Entry newc = parse(line_match);
	if (newc.pos_arr)
		newc.sntn_arr = search_corpus(newc.pos_arr);
	newc.schdl = first_sch();
}

void delete(wchar_t *key)
{
	char *deck = ask_user();
	char filepath[256];
	snprintf(filepath, sizeof(filepath), "%s%s%s", getenv("USER"), ccq_dirpath, deck);
	FILE *d = fopen(filepath, "w");
	if (!d) {
		fprintf(stderr, "Deck open error at %s\n", filepath);
		return;
	}

	char **line = search(key, deck); // search returns array: line [0], position[1]
	if (!line) {
		fprintf(stderr, "[%ls] not in deck [%s]. Exiting...\n", key, deck)
		exit(EXIT_FAILURE);
	}
	fclose(d);

	char *match = sanitize(line[0]);
	int count = atoi(line[1]);
	printf("[%ls] found as entry number [%d] in [%s]\n", key, count, deck);
	printf("[d]elete, [e]xit, [s]how\n");
	int result = get_keypress();
	if (result == 'd') {

if (result == 'd') {
	FILE *orig = fopen(filepath, "r");
	if (!orig) {
		fprintf(stderr, "Could not reopen file for reading.\n");
		return;
	}
	char tmp[256];
	snprintf(tmp, sizeof(tmp), "%s.tmp", filepath);
	FILE *temp = fopen(tmp, "w+");
	if (!temp) {
		fclose(orig);
		fprintf(stderr, "Could not create temp file.\n");
		return;
	}
	int ln = 0;
	char buf[2048];
	while (fgets(buf, sizeof(buf), orig) != NULL) {
		for (;ln != count; ++ln)
			fputs(buf, temp);
	}
	fclose(orig);
	fclose(temp);
	remove(filepath);
	rename(tmp, filepath);

	deck_size =- 1;
	/* later add update config.h logic here */

	printf("Deleted entry %d from deck.\n", count);
	exit(EXIT_SUCCESS);
}
	} else if (result == 'e') {
		printf("User aborted. Exiting...\n");
		exit(EXIT_SUCCESS);
	} else if (result == 's') {
		Entry res = parse(match);
		print(res);
		exit(EXIT_SUCCESS);
	}
}

void search(wchar_t *key) // this retrieves deck/db info
{
	bool card_exists = false;
	wchar_t *deck_lmatch = fsearch(key, deck, deck_size);
	if (deck_lmatch) {
		card_exists = true;
		printf("Deck match found:\n");
		Entry deck_card = parse(deck_lmatch);
		print(deck_card);
	} else {
		printf("No deck match\n");
	}

	wchar_t *db_lmatch = fsearch(key, db, db_size);
	if (db_lmatch) {
		printf("Deck match found:\n");
		Entry db_entry = parse(db_lmatch);
		print(db_entry);
	} else {
		printf("No db match\n");
	}
	if (!card_exists) {
		printf("Do you want to add this card? Enter / exit\n");
		int result = get_keypress();
		if (result == '\n') {
			/* HERE WRITE THE LOGIC TO CHANGE DECK FILE */
		} else {
			return;
		}
	}

	return;
}

SchNum schedule(SchNum old_sch, int result) // reschedules based on result
{
    SchNum scheduled = {0};
    int G = result == '\n' ? 2 : 0; // only grades 0 ("again") and 2 ("good")
    int now = (int)time(NULL);
    float days_since = old_sch.last != 0 ? (now - old_sch.last) / 86400 : 0; // 0 if new
    float finterval = 0;

    scheduled.last = now;

    if (old_sch.state == 0) { // new
        scheduled.state = 1;
        scheduled.D = get_d_init(G);
        scheduled.S = w[G];
        scheduled.R = 1.0;
        scheduled.due = now + 86400;
    } else if (old_sch.state == 1 && result != '\n') { // young failed
        scheduled.D = get_D(old_sch.D, G);
        scheduled.S = get_recall_S(old_sch.D, old_sch.S, old_sch.R); // _recall not _forget bc young
        scheduled.R = get_R(days_since, old_sch.S);
        scheduled.due = now + 86400;
    } else if (old_sch.state == 1 && result == '\n') { // young pass
        scheduled.state = 2;
        scheduled.D = get_D(old_sch.D, G);
        scheduled.S = get_recall_S(old_sch.D, old_sch.S, old_sch.R);
        scheduled.R = get_R(days_since, old_sch.S);
        finterval = get_new_interval(scheduled.S);
        scheduled.due = now + round(finterval);
    } else if (old_sch.state == 2 && result != '\n') { // mature failed
        scheduled.state = 1;
        scheduled.D = get_D(old_sch.D, G);
        scheduled.S = get_forget_S(old_sch.D, old_sch.S, old_sch.R);
        scheduled.R = get_R(days_since, old_sch.S);
        scheduled.due = now + 86400;
    } else if (old_sch.state == 2 && result == '\n') { // mature pass
        scheduled.D = get_D(old_sch.D, G);
        scheduled.S = get_recall_S(old_sch.D, old_sch.S, old_sch.R);
        scheduled.R = get_R(days_since, old_sch.S);
        finterval = get_new_interval(scheduled.S);
        scheduled.due = now + round(finterval);
    } else {
	    fprintf(stderr, "Mistake in card scheduling\n");
    }

    return scheduled;
}

void review(char *deck)
{
	/* 
	 * First goes through deck to get count of due cards
	 * Then goes through deck again to gather the lines
	 * Prints, gets user key press, schedules
	 * */
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
			Entry due_card = parse(line);
			while (1) {
				result = get_keypress();
				if (result == '\n' || result == 'f' || result == 'F') {
					print(due_card);
					schedule(due_card.schdl, deck);
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