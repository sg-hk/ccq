/* Program: ccq = 存储器
* Author: sg-hk
* Minimalistic flashcards in the terminal */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int main(int argc, char *argv[])
{
	// get today as int YYYYMMDD
	time_t t = time(NULL);
	struct tm date = *localtime(&t);
	int today = (date.tm_year + 1900) * 10000 + (date.tm_mon + 1) * 100 + date.tm_mday;
	printf("%d\n", today);

	// load csv
	

	return 0;
}
// next:
// -> filter by last integer
// -> save lines to array, lock csv file?
// -> parse lines
// -> print all fields on new line [TESTING FEATURE]
// later: print only 2nd field, wait for KP, print rest of fields
// later: p/f mechanism
// this is going to be tricky 
// -> termios.h, remove ICONAN and ECHO flags (no EOF no print)
// -> getchar
// -> back to old terminal
// later: calling fsrs
// -> implement the basic algo with default params in C
// -> then just modify python optimizer code to work with our file format
// -> periodically call optimizer? or how best to use it?
// later: rewrite according to line ID, close file
// -> load csv to memory
// -> find most efficient function to find line with first field matching
// -> fprint_s() to change line
// -> close
