/*	存储器 (ccq) by sg-hk

	ccq does five things
 	1. collect due cards
	2. parse due cards
	3. review due cards
	4. store review data
	5. reschedule cards		*/

#include <dirent.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define INITIAL_CAPACITY 100 
#define MAX_LINE_LENGTH 2048
#define CCQ_PATH "/.local/share/ccq/"

typedef struct ReviewData {
	char** due;
	int wcount;
} ReviewData;

typedef struct ParsedLine {
	char* word;
	char* reading;
	char* definition;
	char* sentence;
	char* sreading; // sentence reading
	char** audiofiles;
	int fcount; // audio file count
} ParsedLine;

typedef struct LogData {
	char* word;
	int date;
	int result;
	double time; // review time taken in seconds
} LogData;

int get_day(void)
{
	time_t t = time(NULL);
	struct tm date = *localtime(&t);
	int today = (date.tm_year + 1900) * 10000 + (date.tm_mon + 1) * 100 + date.tm_mday;
	return today;
}

ReviewData get_due(void)
{
	ReviewData reviews = {NULL, 0};
	int today = get_day();

	char path[128];
	snprintf(path, sizeof(path), "%s%s%s", getenv("HOME"), CCQ_PATH, "deck");
	FILE* deck = fopen(path, "r");
	if (deck == NULL) {
		perror("Deck access error");
		exit(EXIT_FAILURE);
	}

	char line[MAX_LINE_LENGTH];
	reviews.due = malloc(INITIAL_CAPACITY * sizeof(char *));
	int capacity = INITIAL_CAPACITY;

	while (fgets(line, sizeof(line), deck)) {
		line[strcspn(line, "\n")] = 0;
		char* last_pipe = strrchr(line, '|');
		if (last_pipe != NULL) {
			char* date_str = last_pipe + 1;
			int date = atoi(date_str);
			if (date <= today) {
				if (reviews.wcount >= capacity) {
					capacity += 100;
					reviews.due = realloc(reviews.due, capacity * sizeof(char*));
				}
				reviews.due[reviews.wcount] = strdup(line);
				++reviews.wcount;
			}
		}
	}
	fclose(deck);

	return reviews;
}

ParsedLine parse_line(char* line) 
{
	ParsedLine parsed_line = {NULL, NULL, NULL, NULL, NULL, NULL, 0};

	char* token = strtok(line, "|");
	if (token != NULL) parsed_line.word = strdup(token);

	token = strtok(NULL, "|");
	if (token != NULL) parsed_line.reading = strdup(token);

	token = strtok(NULL, "|");
	if (token != NULL) parsed_line.definition = strdup(token);

	token = strtok(NULL, "|");
	if (token != NULL) parsed_line.sentence = strdup(token);

	token = strtok(NULL, "|");
	if (token != NULL) parsed_line.sreading = strdup(token);

	token = strtok(NULL, "|");
	/* audio file list parsing */
	if (token != NULL) {
		int i = 0;
		char** audiofiles = malloc(10 * sizeof(char*)); // space for 10 paths
		int capacity = 10;

		char* audiopaths = strtok(token, ";");
		if (audiopaths == NULL) parsed_line.audiofiles[0] = strdup(token);
		while (audiopaths != NULL) {
			if (i >= capacity) {
				capacity += 10;
				audiofiles = realloc(audiofiles, capacity * sizeof(char*));
			}
			char path[128];
			snprintf(path, sizeof(path), "%s%s%s%s", getenv("HOME"), CCQ_PATH, "audio/", audiopaths);
			audiofiles[i] = strdup(path);
			++i;
			audiopaths = strtok(NULL, ";");
		}
		parsed_line.audiofiles = audiofiles;
		parsed_line.fcount = i;
	}

	return parsed_line;
}

int get_keypress(void) 
{
	struct termios oldt, newt;
	int ch;
	tcgetattr(STDIN_FILENO, &oldt);
	newt = oldt;
	newt.c_lflag &= ~(ICANON | ECHO); // new terminal flags
	tcsetattr(STDIN_FILENO, TCSANOW, &newt);
	ch = getchar();
	tcsetattr(STDIN_FILENO, TCSANOW, &oldt); // switch back to old terminal
	return ch;
}

int review_line(ParsedLine parsed_line)
{
	printf("【%s】的意思是...\n", parsed_line.word);
	int result;
	while (1) {
		result = get_keypress();
		if (result == 't' || result == 'w') {
			printf("拼音：\t\t%s\n", parsed_line.reading);
			printf("意义：\t\t%s\n", parsed_line.definition);
			if (parsed_line.sentence != NULL) printf("例句：\t\t %s\n", parsed_line.sentence);
			if (parsed_line.sreading != NULL) printf("例句的拼音：\t%s\n", parsed_line.sreading);
			// randomly play from audio database
			if (parsed_line.fcount > 1) {
				char command[256];
				srand(time(NULL));
				int rdraw = (rand() % parsed_line.fcount) - 1;
				snprintf(command, sizeof(command), "mpv --really-quiet '%s' >/dev/null 2>&1 &", parsed_line.audiofiles[rdraw]);
				system(command);
			} else if (parsed_line.fcount == 1) {
				char command[256];
				snprintf(command, sizeof(command), "mpv --really-quiet '%s' >/dev/null 2>&1 &", parsed_line.audiofiles[0]);
				system(command);
			}
			for (int i = 0; i < parsed_line.fcount; ++i) {
				free(parsed_line.audiofiles[i]);
			}
			free(parsed_line.audiofiles);
			break;
		} else {
			printf("无法识别的输入。\n");
		}
	}
	free(parsed_line.word);
	free(parsed_line.reading);
	free(parsed_line.definition);
	free(parsed_line.sentence);
	free(parsed_line.sreading);

	return result;
}

void log_reviews(LogData* logs, int count)
{
	char path[128];

	snprintf(path, sizeof(path), "%s%s%s", getenv("HOME"), CCQ_PATH, "log");
	FILE* log = fopen(path, "a");
	if (log == NULL) {
		perror("Log access error");
		exit(EXIT_FAILURE);
	}

	for (int i = 0; i < count; ++i) {
		printf("Debug: About to log: '%s'\n", logs[i].word);
		fprintf(log, "%s|%d|%d|%0.2lf\n", logs[i].word, logs[i].date, logs[i].result, logs[i].time);
	}

	fclose(log);
}

void reschedule(LogData* logs, int count)
{
	int today = get_day();
	int scheduled_date = 0;

	char path[128];
	snprintf(path, sizeof(path), "%s%s%s", getenv("HOME"), CCQ_PATH, "deck");
	FILE* deck = fopen(path, "w");
	if (deck == NULL) {
		perror("Deck access error");
		exit(EXIT_FAILURE);
	}

	/* very simple algorithm:
	eventually this will be replaced with FSRS */
	for (int i = 0; i < count; ++i) {
		if logs[i].result == "f") scheduled_date = today + 1;
		else scheduled_date = today + interval * 2
	}

	/* here reopen deck
	find logs[i].word matching the strtoken(|,line) of each line of deck
	and update the last field after | */

	fclose(deck);
}

int main(int argc, char *argv[])
{
	setlocale(LC_ALL, "");
	ReviewData reviews = get_due();
	LogData reviews_log[reviews.wcount];
	int i = 0;

	if (reviews.wcount == 0) {
		printf("No reviews scheduled today\n");
		return 0;
	} else {
		printf("今天有%d个单词要学习。准备好了吗？\n", reviews.wcount);
		printf("按 [t]（通过）或 [w]（未通过）\n");
	}

	while (i < reviews.wcount) {
		LogData review_log = {NULL, 0, 0, 0.0};
		struct timespec start, end;

		ParsedLine parsed_line = parse_line(reviews.due[i]);
		printf("Debug: Before assigning to review_log.word: '%s'\n", parsed_line.word);
		review_log.word = parsed_line.word;
		printf("Debug: After assigning to review_log.word: '%s'\n", review_log.word);
		review_log.date = get_day();
		clock_gettime(CLOCK_MONOTONIC, &start);
		review_log.result = review_line(parsed_line);
		clock_gettime(CLOCK_MONOTONIC, &end);
		review_log.time = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
		reviews_log[i] = review_log;
		free(reviews.due[i]);
		++i;
	}

	log_reviews(reviews_log, reviews.wcount);
	reschedule(reviews_log);
	free(reviews_log);
	free(reviews.due);

	return 0;
}
