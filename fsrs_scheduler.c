#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define MAX_INTV 36500 
#define TARGET 0.9  
#define MAIN_PATH "./local/share/ccq"

float W0 = 0.4072, // initial stability (again)
 W1 = 1.1829, // initial stability (hard)
 W2 = 3.1262, // initial stability (good)
 W3 = 15.4722, // initial stability (easy)
 W4 = 7.2102, // initial difficulty (good)
 W5 = 0.5316, // initial difficulty (multiplier)
 W6 = 1.0651, // difficulty (multiplier)
 W7 = 0.0234, // difficulty (multiplier)
 W8 = 1.616, // stability (exponent)
 W9 = 0.1544, // stability (negative power)
 W10 = 1.0824, // stability (exponent)
 W11 = 1.9813, // fail stability (multiplier)
 W12 = 0.0953, // fail stability (negative power)
 W13 = 0.2975, // fail stability (power)
 W14 = 2.2042, // fail stability (exponent)
 W15 = 0.2407, // stability (multiplier for Hard)
 W16 = 2.9466, // stability (multiplier for Easy)
 W17 = 0.5034, // ??
 W18 = 0.6567; // ??

typedef struct Card {
	unsigned short id, reps, lapses;
	double stability;
	unsigned char difficulty;
	unsigned int last_review, next_review;
} Card;

typedef struct Card {
	int card_id;
	char review_input;
} CardReview;

void schedule_cards(CardReview reviews[], int count)
{
	char path_log[128];
	char path_deck[128];
	snprintf(path_log, sizeof(path_log), "%s%s/review_log", getenv("HOME"), MAIN_PATH);
	snprintf(path_deck, sizeof(path_deck), "%s%s/deck", getenv("HOME"), MAIN_PATH);
	FILE* review_logs = fopen(path_log, "w+");
	FILE* deck = fopen(path_deck, "w+");

	for (int i = 0; i < count; i++) {
		printf("Scheduling card ID:%d with input:%c\n", reviews[i].card_id, reviews[i].review_input);
	}

	fclose(review_logs);
	fclose(deck);
}

// notes for implementation
// we need a data set with a triad:
// e := word, [t1,t2,...,tn], [r1,r2,...,rn]
// with t being the time elapsed since last review
// and r being the outcome (pass/fail? or the 1-4 scale?)

// cards should be rescheduled every 2^n reviews: maybe start at 8, then 16, 32, ..., etc.
