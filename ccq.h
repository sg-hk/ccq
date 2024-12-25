#ifndef CCQ_H_
#define CCQ_H_

#include <stdio.h>
#include <wchar.h>

/* CONSTANTS */
#define CCQ_PATH "/.local/share/ccq/"
#define DB_SIZE 846271 // dictionary database size (n lines/entries)

// Card structs
typedef struct WordInfo {
	wchar_t *key;
	wchar_t *definition;
} WordInfo;

typedef struct ContextInfo {
	wchar_t **sentences;
	char **images;
	char **recordings;
} ContextInfo;

typedef struct ScheduleInfo {
	int state;
	float D, S, R;
	int last, due;
} ScheduleInfo;

typedef struct Card {
	WordInfo word;
	ContextInfo context;
	ScheduleInfo schedule;
} Card;

typedef struct DicEntry {
	char *key;
	char *definition;
	char **recordings;
} DicEntry;

/* FUNCTIONS */
// Reviewing
void dis_raw(struct termios *original);
void en_raw(struct termios *original);
int get_keypress();
int play_audio(char *audio);
char *sanitize(char *input);
int render_image(char *image);
int reveal_card(Card *card);
void review_cards(FILE *deck);
// Scheduling
float get_d_init(int G);
float get_new_interval(float S);
float get_mean_reversion(float a, float b);
float get_sterm_S(float S, int G);
float get_R(int t, float S);
float get_forget_S(float D, float S, float R);
float get_recall_S(float D, float S, float R);
float get_D(float D, int G);
ScheduleInfo schedule_card(ScheduleInfo old_sch, int result);
// Adding


#endif // CCQ_H_
