#ifndef CCQ_H_
#define CCQ_H_

#include <stdio.h>

/* CONSTANTS */
#define CCQ_PATH "/.local/share/ccq/"

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

// parameters
const float w[] = {
    0.4177, 0, 0.9988, 0, // initial stability for grades A/H/G/E
    7.1949, 0.5345, 1.4604,
    0.0046, 1.54575, 0.1192, 1.01925, 1.9395, 0.11, 0.29605,
    2.2698, 0.2315, 2.9898, 0.51655, 0.6621
};
#define FACTOR 19.0/81.0 // fsrs factor
#define DECAY -0.5 // fsrs decay



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
