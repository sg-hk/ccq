#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <termios.h>
#include <time.h>
#include <stdlib.h>

static const char *A122 = "/.local/share/A122/";
static const int MAX_READ = 1024;
static const int DUE_BUF = 128;
static const int PATH_MAX = 256;
static const int DATE_LEN = 10;

char getrawch(void);
int to_utf8(uint32_t ch, char *out);

int
main(int argc, char *argv[])
{
	if (argc != 2) {
		fprintf(stderr, "Usage: %s <study_list>\n", argv[0]);
		exit(1);
	}
	
	char path[PATH_MAX];
        char *home = getenv("HOME");
        if (!home) {perror("getenv('HOME')"); exit(1);}
	snprintf(path, PATH_MAX, "%s%s%s", home, A122, argv[1]);
	FILE *slist = fopen(path, "r");
	if (!slist) {perror("fopen"); exit(1);}

        uint32_t bom = 0;
        fread(&bom, sizeof(uint32_t), 1, slist);
        if (bom != 0x0000FEFF) {
                fprintf(stderr, "%s is not UTF-32\n", argv[1]);
                fclose(slist);
                exit(1);
        }

        time_t now = time(NULL);
        struct tm *localnow = localtime(&now);
        int pyear = localnow->tm_year + 1900;
        int pmonth = localnow->tm_mon + 1;
        int pday = localnow->tm_mday;

	uint32_t *line = malloc(MAX_READ * sizeof(uint32_t));
        if (!line) {perror("malloc line"); exit(1);}

        uint32_t **fronts = malloc(DUE_BUF * sizeof(uint32_t*));
        uint32_t **backs = malloc(DUE_BUF * sizeof(uint32_t*));
        long *offsets = malloc(DUE_BUF * sizeof(long));
        if (!fronts || !backs || !offsets) {perror("malloc dues"); exit(1);}

	int due_n = 0, due_buf = DUE_BUF;
        long pos;
        size_t ch_read = 0;

        /* dumb and fun manual parsing */
        while (fread(&line[ch_read], sizeof(uint32_t), 1, slist) == 1) {
                if (ch_read >= MAX_READ -1) {
                        fprintf(stderr, "line too long!\n");
                        exit(1);
                }

                if (line[ch_read] != (uint32_t)'\n') {
                        ++ch_read;
                        continue;
                }

                line[ch_read + 1] = (uint32_t)'\0';

                int slash = 0;
                while(line[slash] != (uint32_t)'/')
                        ++slash;
                
                int year = 0, month = 0, day = 0;
                for (int i = 0; i < 4; ++i)
                        year = year * 10 + line[slash-4+i] - '0';
                if (year > pyear)
                        continue;
                if (year == pyear) {
                        for (int i = 0; i < 2; ++i) {
                                month = month * 10 + line[slash+1+i] - '0';
                                day = day * 10 + line[slash+4+i] - '0';
                        }
                        if (month > pmonth)
                                continue;
                        if (month == pmonth && day > pday)
                                continue;
                }
                
                /* go back to line start, +1 because we read \n too */
                pos = ftell(slist) - (ch_read + 1)*sizeof(uint32_t);
                ch_read = 0; // reset for other line counts
                /* go forward to date start */
                pos += (slash - 4)*sizeof(uint32_t);
                offsets[due_n] = pos;

                fronts[due_n] = malloc(MAX_READ*sizeof(uint32_t));
                if (!fronts[due_n]) {perror("malloc fronts[due]"); exit(1);}
                int pipe = 0;
                while(line[pipe] != (uint32_t)'|') {
                        fronts[due_n][pipe] = line[pipe];
                        ++pipe;
                }
                fronts[due_n][pipe] = (uint32_t)'\0';

                /* slash points to '/', pipe points to '|'
                 * back field is from pipe+1 to slash-6 for |YYYY/ */
                backs[due_n] = malloc(MAX_READ*sizeof(uint32_t));
                if (!backs[due_n]) {perror("malloc backs[due]"); exit(1);}
                for (int i = 0; i < (slash-(pipe+1)-5); ++i)
                        backs[due_n][i] = line[pipe+1+i];
                backs[due_n][slash-(pipe+1)-5+1] = (uint32_t)'\0';

                ++due_n; // increment at end to turn from index to counter
                if (due_n >= due_buf) {
                        due_buf *= 2;
                        uint32_t **tfr = realloc(fronts, due_buf * sizeof(uint32_t*));
                        uint32_t **tbk = realloc(backs, due_buf * sizeof(uint32_t*));
                        long *toffs = realloc(offsets, due_buf * sizeof(long));
                        if (!tfr || !tbk || !toffs) {
                                perror("realloc dues"); 
                                exit(1);
                        }
                        fronts = tfr;
                        backs = tbk;
                        offsets = toffs;
                }
        }
        fclose(slist);
        free(line);

        if (due_n == 0) {
                printf("No cards due\n"); 
                free(fronts);
                free(backs);
                free(offsets);
                exit(0);
        }

        printf("There %s %i card%s due today.\n", 
               due_n > 1 ? "are" : "is", due_n, due_n > 1 ? "s" : "");

        printf("QUIZ: enter if pass, any to fail\n\n");
        int *incr = malloc(due_n * sizeof(int));
        int len = 0;
        char ch, utf8[4];
        for (int i = 0; i < due_n; ++i) {
                for (int j = 0; fronts[i][j] != (uint32_t)'\0'; ++j) {
                        len = to_utf8(fronts[i][j], utf8);
                        write(1, utf8, len);
                }
                free(fronts[i]);
                write(1, "\n", 1);
                getrawch();
                for (int j = 0; backs[i][j] != (uint32_t)'\0'; ++j) {
                        len = to_utf8(backs[i][j], utf8);
                        write(1, utf8, len);
                }
                free(backs[i]);
                write(1, "\n", 1);
                ch = getrawch();
                incr[i] = (ch == '\n') ? 1 : 2;
                printf("%s\n\n", (ch == '\n') ? "PASS" : "FAIL");
        }

        free(fronts);
        free(backs);

        /* reopen slist, update the new dates */
        slist = fopen(path, "r+");
        if (!slist) {perror("2nd fopen"); exit(1);}
        for (int i = 0; i < due_n; ++i) {
                time_t ntime = now + incr[i] * 86400;
                struct tm *nlocal = localtime(&ntime);
                int nyear = nlocal->tm_year+1900;
                int nmonth = nlocal->tm_mon+1;
                int nday = nlocal->tm_mday;

                uint32_t ndate[DATE_LEN + 1]; // YYYY/MM/DD\0 = 11
                ndate[0] = nyear / 1000;
                ndate[1] = (nyear - ndate[0]*1000) / 100;
                ndate[2] = (nyear - ndate[0]*1000 - ndate[1]*100) / 10;
                ndate[3] = nyear - ndate[0]*1000 - ndate[1]*100 - ndate[2]*10;
                ndate[4] = (uint32_t)'/';
                ndate[5] = nmonth / 10 ? nmonth / 10 : 0;
                ndate[6] = nmonth - ndate[5];
                ndate[7] = (uint32_t)'/';
                ndate[8] = nday / 10 ? nday / 10 : 0;
                ndate[9] = nday - ndate[8] * 10;
                for (int i = 0; i < 10; ++i) {
                        if (i != 4 && i != 7) 
                                ndate[i] += '0';
                }
                ndate[10] = (uint32_t)'\0';

                /* note that the offsets are already in bytes
                 * no need to multiply by sizeof() */
                if(fseek(slist, offsets[i], SEEK_SET)) {
                        perror("fseek"); 
                        exit(1);
                }

                if(fwrite(ndate, sizeof(uint32_t), DATE_LEN, slist) 
                   != DATE_LEN) {
                        perror("fwrite ndate"); 
                        exit(1);
                }
        }

        free(incr);
        free(offsets);
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

int to_utf8(uint32_t ch, char *out)
{
    if (ch <= 0x7F) {
        out[0] = (char)ch;
        return 1;
    } else if (ch <= 0x7FF) {
        out[0] = (char)(0xC0 | ((ch >> 6) & 0x1F));
        out[1] = (char)(0x80 | (ch & 0x3F));
        return 2;
    } else if (ch <= 0xFFFF) {
        out[0] = (char)(0xE0 | ((ch >> 12) & 0x0F));
        out[1] = (char)(0x80 | ((ch >> 6) & 0x3F));
        out[2] = (char)(0x80 | (ch & 0x3F));
        return 3;
    } else if (ch <= 0x10FFFF) {
        out[0] = (char)(0xF0 | ((ch >> 18) & 0x07));
        out[1] = (char)(0x80 | ((ch >> 12) & 0x3F));
        out[2] = (char)(0x80 | ((ch >> 6) & 0x3F));
        out[3] = (char)(0x80 | (ch & 0x3F));
        return 4;
    } else {
        // Invalid Unicode code point
        out[0] = '?';
        return 1;
    }
}
