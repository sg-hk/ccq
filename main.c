#include <stdio.h>
#include <locale.h>
#include <unistd.h>
#include <termios.h>
#include <wchar.h>
#include <time.h>
#include <stdlib.h>

static const char *FOX_DIR = "/.local/share/FOXP2/";
static const int MAX_READ = 1024;
static const int DUE_BUF = 128;
static const int PATH_MAX = 256;

char getrawch(void);

int
main(int argc, char *argv[])
{
        setlocale(LC_ALL, "");
	if (argc != 2) {
		fprintf(stderr, "Usage: %s <study_list>\n", argv[0]);
		exit(1);
	}
	
	char path[PATH_MAX];
        char *home = getenv("HOME");
        if (!home) {perror("getenv('HOME')"); exit(1);}
	snprintf(path, PATH_MAX, "%s%s%s", home, FOX_DIR, argv[1]);
	FILE *slist = fopen(path, "r");
	if (!slist) 
                {perror("fopen"); exit(1);}

        time_t now = time(NULL);
        struct tm *localnow = localtime(&now);
        int pyear = localnow->tm_year + 1900;
        int pmonth = localnow->tm_mon + 1;
        int pday = localnow->tm_mday;

	wchar_t *line = malloc(MAX_READ * sizeof(wchar_t));
        if (!line) {perror("malloc line"); exit(1);}

        wchar_t **fronts = malloc(DUE_BUF * sizeof(wchar_t*));
        wchar_t **backs = malloc(DUE_BUF * sizeof(wchar_t*));
        long *offsets = malloc(DUE_BUF * sizeof(long));
        if (!fronts || !backs || !offsets) {perror("malloc dues"); exit(1);}

	int due_n = 0, due_buf = DUE_BUF;
        long pos;
        size_t chars_read = 0;

        /* we are using UTF-32 files to ensure rewriting at byte offsets works 
         * fgetws won't work --> we need to manually parse the lines */
        while (fread(&line[chars_read], sizeof(wchar_t), 1, slist) == 1) {
                if (line[chars_read] != L'\n' && line[chars_read] != EOF) {
                        ++chars_read;
                        continue;
                }

                line[chars_read + 1] = L'\0';
                printf("More lines!\n");
                /* go back chars_read + the new line */
                pos = ftell(slist) - (chars_read + 1) * sizeof(wchar_t);

                wchar_t *state1, *state2;

                wchar_t *front = wcstok(line, L"|", &state1);
                wchar_t *back = wcstok(NULL, L"|", &state1);
                wchar_t *date = wcstok(NULL, L"|", &state1);

                long date_offset = pos;
                date_offset += wcslen(front) + 1 + wcslen(back) + 1;

                int year = (int)wcstol(wcstok(date, L"/", &state2), NULL, 10);
                int month = (int)wcstol(wcstok(NULL, L"/", &state2), NULL, 10);
                int day = (int)wcstol(wcstok(NULL, L"/", &state2), NULL, 10);

                if (year > pyear ||
                                (year == pyear && month > pmonth) ||
                                (year == pyear && month == pmonth && day > pday)) {
                        chars_read = 0;
                        continue;
                }

                if (due_n >= due_buf) {
                        due_buf *= 2;
                        wchar_t **tfronts = realloc(fronts, due_buf * sizeof(wchar_t*));
                        wchar_t **tbacks = realloc(backs, due_buf * sizeof(wchar_t*));
                        long *toffs = realloc(offsets, due_buf * sizeof(long));
                        if (!tfronts || !tbacks || !toffs) 
                        {perror("realloc dues"); exit(1);}
                        fronts = tfronts;
                        backs = tbacks;
                        offsets = toffs;
                }


                fronts[due_n] = wcsdup(front);
                backs[due_n] = wcsdup(back);
                offsets[due_n] = date_offset;
                wprintf(L"Front: %ls, back: %ls, offset: %i\n",
                                fronts[due_n], backs[due_n], offsets[due_n]);
                wprintf(L"%i/%i/%i\n", year, month, day);
                ++due_n;
                chars_read = 0;
        }
        fclose(slist);

        if (due_n == 0)
        {printf("No cards due\n"); exit(0);}

        printf("There %s %i card%s due today.\n", 
                        due_n > 1 ? "are" : "is", due_n, due_n > 1 ? "s" : "");

        printf("QUIZ: enter if pass, any to fail\n");
        int *incr = malloc(due_n * sizeof(int));
        char ch;
        for (int i = 0; i < due_n; ++i) {
                wprintf(L"FRONT: %ls\n", fronts[i]);
                free(fronts[i]);
                getrawch();
                wprintf(L"BACK: %ls\n", backs[i]);
                free(backs[i]);
                ch = getrawch();
                incr[i] = (ch == '\n') ? 1 : 2;
        }

        free(fronts);
        free(backs);

        /* reopen slist, update the new dates */
        slist = fopen(path, "r+");
        if (!slist) 
        {perror("2nd fopen"); exit(1);}
        for (int i = 0; i < due_n; ++i) {
                time_t ntime = now + incr[i] * 86400;
                struct tm *nlocal = localtime(&ntime);
                wchar_t ndate[11]; // YYYY-MM-DD\0 = 11
                swprintf(ndate, 11, L"%04d/%02d/%02d", 
                                nlocal->tm_year+1900, nlocal->tm_mon+1, nlocal->tm_mday);

                if(fseek(slist, offsets[i] * sizeof(wchar_t), SEEK_SET))
                {perror("fseek"); exit(1);}

                if(fwrite(ndate, sizeof(wchar_t), 10, slist) != 10)
                {perror("fwrite ndate"); exit(1);}
        }

        free(line);
        free(offsets);
        free(incr);
        exit(0);
}

char getrawch(void)
{
        struct termios old, new;
        char ch;

        if (tcgetattr(STDIN_FILENO, &old) == -1)
        {perror("tcgetattr"); exit(1);}

        new = old;
        new.c_lflag &= ~(ICANON | ECHO); // direct read + no echo

        if (tcsetattr(STDIN_FILENO, TCSANOW, &new) == -1)
        {perror("tcsetattr"); exit(1);}

        if (read(STDIN_FILENO, &ch, 1) != 1)
        {perror("read"); exit(1);}

        /* restore old termios */
        if (tcsetattr(STDIN_FILENO, TCSANOW, &old) == -1)
        {perror("tcsetattr"); exit(1);}

        return ch;
}
