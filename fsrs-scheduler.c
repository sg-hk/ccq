#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define MAX_INTV 36500 
#define TARGET 0.9  

enum params {
	0.4072, 1.1829, 3.1262, 15.4722, 7.2102, 0.5316, 1.0651, 0.0234, 1.616, 0.1544, 1.0824, 1.9813, 0.0953, 0.2975, 2.2042, 0.2407, 2.9466, 0.5034, 0.6567
};

typedef struct {
	unsigned short id, reps, lapses;
	double stability;
	unsigned char difficulty;
	unsigned int last_review, next_review;
} Card;
