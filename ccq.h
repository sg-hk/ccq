#ifndef CCQ_H_
#define CCQ_H_

#include <stdio.h>

/* CONSTANTS */
#define CCQ_PATH "/.local/share/ccq/"
#define DB_SIZE 846271 // dictionary database size

// Card structs
typedef struct WordInfo {
    char *key;
    char *reading;
    char *definition;
} WordInfo;

typedef struct ContextInfo {
    char **sentences;
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

/* FUNCTIONS */
// Reviewing
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
// Editing


#endif // CCQ_H_
