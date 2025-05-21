#define _POSIX_C_SOURCE 200809L /* make ninja shut up */

#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <math.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "config.h"

char 	get_instant_char(void);
char   *build_ccq_path(const char *suffix);
int 	cmp_key(int fd, const char *key, const int klen);
long	schedule(int g,int reps,double s,double d,double elap,double rd);
off_t 	search_sl(const char *sl_path, const char *key, int klen);
off_t  *search_db(const char *db_path, const char *key, int klen);
void	die(const char *fmt, ...);
void	handle_signal(int sig);
void	query(const char *key, const char *database_path, const char *study_list_path);
void	usage(void);
void 	review_list(const char *study_list_path, char order);
void   *err_malloc(const char *varname, size_t size);

const int  epoch_len = 10;
const int  fsrs_len  = 12;
const int  path_len  = 128;
volatile sig_atomic_t terminate = false;

/* fsrs constants */
static const double W[19]={
	0.40255,1.18385,3.17300,15.69105,7.19490,0.53450,1.46040,0.00460,
	1.54575,0.11920,1.01925,1.93950,0.11000,0.29605,2.26980,0.23150,
	2.98980,0.51655,0.66210
};
#define F (19.0/81.0)
#define C (-0.5)

int
main(int argc, char *argv[])
{
	setlocale(LC_ALL, "");

	bool    want_query = false;
	char   *database   = NULL;
	char   *key        = NULL;
	char   *study_list = NULL;
	int     ch         = 0;
	int     order      = 0;

	/* reviewing flags: n, o, r (mutually exclusive, optional) */
	/* query flags: q (necessary), s, d (optional) 		   */
	while ((ch = getopt(argc, argv, "norq:d:s:")) != -1) {
		switch (ch) {
		case 'n':
			order = 'n';
			break;
		case 'o':
			if (order)
				usage();
			order = 'o';
			break;
		case 'r':
			if (order)
				usage();
			order = 'r';
			break;
		case 'q':
			want_query = true;
			key = optarg;
			break;
		case 'd':
			database = optarg;
			break;
		case 's':
			if (study_list)
				usage();
			study_list = optarg;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;
	
	if (argc != 1) {
		printf("argc is %d\n", argc);
		usage();
	}

	struct sigaction sa;
	sa.sa_handler = handle_signal;
	sigemptyset(&sa.sa_mask); /* don't block other signals */
	sa.sa_flags = 0; /* no special flags */

	if (sigaction(SIGINT, &sa, NULL) == -1) {
		perror("can't handle SIGINT");
		return 1; 
	}

	if (want_query) {
		/* forbid mixing query and review flags */
		if (order)
			usage();

		if (!database)
			database = build_ccq_path(DEFAULT_DB);
		else
			database = build_ccq_path(database);

		if (!study_list)
			study_list = build_ccq_path(DEFAULT_SL);
		else
			study_list = build_ccq_path(study_list);

		query(key, database, study_list);
	} else {
		if (!order)
			order = DEFAULT_ORDER;

		if (!study_list)
			study_list = build_ccq_path(argv[0]);
		else
			study_list = build_ccq_path(study_list);

		review_list(study_list, order);
	}
	
	if (database)
		free(database);
	free(study_list);
	return 0;
}

void
review_list(const char *sl_path, const char order)
{
	int fd = open(sl_path, O_RDONLY);
	if (fd == -1) {
		fprintf(stderr, "couldn't open file at %s\n", sl_path);
		perror("");
		exit(1);
	}

	off_t pos           = 0;
	time_t 	now         = time(NULL);
	int 	due_cnt     = 0;
	int 	due_cnt_buf = 64;

	time_t *epoch_old = err_malloc("epoch_old", due_cnt_buf * sizeof(time_t));
	off_t  *fsrs_pos  = err_malloc("fsrs_pos", due_cnt_buf * sizeof(off_t));
	int    *front_len = err_malloc("front_len", due_cnt_buf * sizeof(int));
	int    *back_len  = err_malloc("back_len", due_cnt_buf * sizeof(int));

	for (;;) {
		ssize_t n 	 = 0;
		bool 	is_due   = false;
		bool	is_fsrs	 = false;
		bool	is_front = false;
		bool	is_back	 = false;
		char 	epoch_str[epoch_len];

		n = read(fd, epoch_str, epoch_len);
		if (n == 0) /* EOF before epoch */
			break;
		if (n != 10) 
			die("epoch field read error\n");
		pos += epoch_len;

		time_t epoch = 0;
		/* atoi with user input check */
		for (int i = 0; i < epoch_len; ++i) {
			if (epoch_str[i] < '0' || epoch_str[i] > '9')
				die("non digit char in epoch\n");
			epoch = epoch * 10 + (epoch_str[i] - '0');
		}

		if (epoch <= now) {
			if (due_cnt >= due_cnt_buf) {
				due_cnt_buf *= 2;
				time_t *tepo = realloc(epoch_old, due_cnt_buf * sizeof(time_t));
				off_t *tpos  = realloc(fsrs_pos, due_cnt_buf * sizeof(off_t));
				int *tfrlen  = realloc(front_len, due_cnt_buf * sizeof(int));
				int *tbklen  = realloc(back_len, due_cnt_buf * sizeof(int));
				if (!tepo || !tpos || !tfrlen || !tbklen)
					die("realloc fsrs_pos failed\n");
				epoch_old = tepo;
				fsrs_pos  = tpos;
				front_len = tfrlen;
				back_len  = tbklen;
			}
			epoch_old[due_cnt] = epoch;
			fsrs_pos[due_cnt]  = pos + 1; /* EPOCH|FSRS -> +1 to skip pipe */
			is_due             = true;
			++due_cnt;
		}

		/* skip rest of line
		 * when the card is due we want to store length of due fields
		 * these two local vars help us quickly calculate them */
		long front_start = 0;
		long back_start  = 0;
		int  has_newline = 0;
		int  ch          = 0;
		do {
			n = read(fd, &ch, 1);
			if (n == 0) { /* EOF inside record: last record */
				if (is_due)
					back_len[due_cnt-1] = pos - back_start;
				break;
			}
			if (n < 0)
				die("read error when draining line\n");
			++pos;

			if (ch == '\n') /* ensures we remove \n from pos count */
				has_newline = 1;

			if (!is_due)
				continue;

			if (ch == '|') {
				/* identify where we are: |FSRS|FRONT|BACK */
				if (!is_fsrs) {
					is_fsrs = 1;
					front_start = fsrs_pos[due_cnt-1] + fsrs_len + 1;
				} else if (!is_front) {
					is_front = 1;
				} else if (!is_back) {
					/* we are at |BACK, front ends 1 before */
					front_len[due_cnt-1] = pos - front_start - 1;
					back_start = pos;
					is_back = 1;
				} 
			}
		} while (ch != '\n');

		if (is_due) 
			back_len[due_cnt-1] = pos - back_start - has_newline;

		if (n == 0)  /* EOF after record */
			break;
	}


	if (due_cnt == 0)
		die("no cards due\n");

	/* ----------- */
	/* copy fields */
	/* ----------- */
	char **back  = err_malloc("back", due_cnt * sizeof(char*));
	char **front = err_malloc("front", due_cnt * sizeof(char*));
	char **fsrs  = err_malloc("fsrs", due_cnt * sizeof(char*));

	int ch = 0;
	for (int i = 0; i < due_cnt; ++i) {
		front[i] = err_malloc("front[i]", front_len[i]);
		back[i]  = err_malloc("back[i]", back_len[i]);
		fsrs[i]  = err_malloc("fsrs[i]", fsrs_len);

		if (lseek(fd, fsrs_pos[i], SEEK_SET) < 0)
			die("lseek to copy fields failed\n");

		/* copy fsrs */
		if (read(fd, fsrs[i], fsrs_len) != fsrs_len)
			die("read to copy fsrs failed\n");

		/* skip pipe */
		read(fd, &ch, 1);

		/* copy front */
		if (read(fd, front[i], front_len[i]) != front_len[i])
			die("read to copy fsrs failed\n");
		
		/* skip pipe */
		read(fd, &ch, 1);

		/* copy back */
		if (read(fd, back[i], back_len[i]) != back_len[i])
			die("read to copy fsrs failed\n");
	}

	close(fd);

	/* ------ */
	/* review */
	/* ------ */
	int *review_order = err_malloc("review_order", due_cnt * sizeof(int));

	/* oldest first order (line 0 to line n = order of creation) 
	 * this is the default to which we apply transformations
	 * i.e. this is the order == 'o' case */
	for (int i = 0; i < due_cnt; ++i)
		review_order[i] = i;

	/* newest first: reverse */
	if (order == 'n') {
		for (int i = 0; i < due_cnt / 2; ++i) {
			int tmp = review_order[i];
			review_order[i] = review_order[due_cnt - 1 - i];
			review_order[due_cnt - 1 - i] = tmp;
		}

	/* random: Fisher–Yates shuffle */
	} else if (order == 'r') {
		srand(now);
		for (int i = due_cnt - 1; i > 0; --i) {
			int j = rand() % (i + 1); /* [0,i] */
			int tmp = review_order[i];
			review_order[i] = review_order[j];
			review_order[j] = tmp;
		}
	}

	printf("ccq... \033[1mSTART!\033[22m  %d cards due\n\n", due_cnt);

	int reviewed_count = 0;
	char *res_arr = err_malloc("res_arr", due_cnt);
	int nel_passch = sizeof(PASS_CHARS) / sizeof(PASS_CHARS[0]);
	for (; reviewed_count < due_cnt; ++reviewed_count) {
		if (terminate)
			break;

		int k = review_order[reviewed_count];
		int ch = 0;

		/* show front */
		printf("[%d/%d]  ", reviewed_count+1, due_cnt);
		fflush(stdout);
		if (front[k] == NULL)
			die("front[%d] is NULL\n", k);
		write(1, front[k], front_len[k]);
		ch = get_instant_char();
		if (ch == EOF) {
			terminate = true;
			break;
		}

		/* show back, store user answer */
		if (back[k] == NULL)
			die("back[%d] is NULL\n", k);
		write(1, "\n", 1);
		write(1, back[k], back_len[k]);
		ch = get_instant_char();
		if (ch == EOF) {
			terminate = true;
			break;
		} 

		/* show result */
		bool is_passed = false;
		for (int i = 0; i < nel_passch; ++i) {
			if (ch == PASS_CHARS[i]) {
				is_passed = true;
				res_arr[k] = 1;
				write(1, "\nPASS\n\n", 7);
				break;
			}
		}

		if (!is_passed) {
			res_arr[k] = 0;
			write(1, "\nFAIL\n\n", 7);
		}
	}

	for (int i = 0; i < due_cnt; ++i) {
		free(front[i]);
		free(back[i]);
	}
	free(front);
	free(back);

	/* ------------- */
	/* update fields */
	/* ------------- */
	if (reviewed_count) {
		fd = open(sl_path, O_RDWR);
		if (fd < 0)
			die("second file open failed\n");
	}

	for (int i = 0; i < reviewed_count; ++i) {
		int k = review_order[i];

		/* scheduler args */
		int    grade = res_arr[k] ? 3 : 1; /* good : again */
		double stab  = strtol(fsrs[k] + 3, 0, 10) / 100.0; 
		double diff  = strtol(fsrs[k] + 8, 0, 10) / 100.0; 
		int    reps  = (fsrs[k][0]-'0')*10 + (fsrs[k][1]-'0');
		double elap  = (double)(now - epoch_old[k]) / 86400.0;

		/* interval to add, in seconds */
		double nxt   = schedule(grade, reps, stab, diff, elap, 0.9); 

		/* due date epoch */
		time_t new_epoch = now + (time_t)(nxt + 0.5);  /* round */

		/* updating fsrs vars for the card */
		int  new_reps = (grade == 1) ? 0 : (reps < 99 ? reps + 1 : 99);
		stab = (grade == 1) ? 2.50 : stab + stab / 4.0 + 0.25;
		if (stab > 99.99) stab = 99.99;
		diff = (grade == 1) ? 5.00 : (diff > 1.25 ? diff - 0.25 : 1.00);

		/* numbers to ASCII... a little verbose */
		char epoch_str[epoch_len];
		time_t t = new_epoch;
		for (int p = epoch_len - 1; p >= 0; --p) {
			epoch_str[p] = (t % 10) + '0';
			t /= 10;
		}
		char new_fsrs[fsrs_len];
		new_fsrs[0] = new_reps / 10 + '0';
		new_fsrs[1] = new_reps % 10 + '0';
		new_fsrs[2] = ';';
		int s100 = (int)(stab * 100 + 0.5);
		new_fsrs[3] = (s100 / 1000)       + '0';
		new_fsrs[4] = (s100 /  100) % 10  + '0';
		new_fsrs[5] = (s100 /   10) % 10  + '0';
		new_fsrs[6] =  s100 %   10        + '0';
		new_fsrs[7] = ';';
		int d100 = (int)(diff * 100 + 0.5);
		new_fsrs[8]  = (d100 / 1000)       + '0';
		new_fsrs[9]  = (d100 /  100) % 10  + '0';
		new_fsrs[10] = (d100 /   10) % 10  + '0';
		new_fsrs[11] =  d100 %   10        + '0';

		/* overwrite date */
		if (lseek(fd, fsrs_pos[k] - epoch_len - 1, SEEK_SET) < 0)
			die("lseek date");
		if (write(fd, epoch_str, epoch_len) != epoch_len)
			die("write date");

		/* overwrite the ‘|’ */
		if (write(fd, "|", 1) != 1)
			die("write pipe");

		/* overwrite fsrs */
		if (write(fd, new_fsrs, fsrs_len) != fsrs_len)
			die("write fsrs");
	}

	if (reviewed_count) close(fd);
	free(back_len);
	free(front_len);
	free(fsrs);
	free(fsrs_pos);
	free(res_arr);
	free(review_order);

	return;
}

void
query(const char *key, const char *db_path, const char *sl_path)
{
	/* check if card exists: if it does, print and exit */
	int klen = 0;
	while (key[klen])
		++klen;
	off_t pos = search_sl(sl_path, key, klen);
	if (pos != -1) {
		int fd = open(sl_path, O_RDONLY);
		lseek(fd, pos + klen + 1, SEEK_SET);
		int ch = 0;
		ssize_t n = 0;
		do {
			n = read(fd, &ch, 1);
			if (n == 1)
				continue;
			if (n == 0)
				die("reached sl EOF before reaching back\n");
			if (n < 0)
				die("read sl error\n");
		} while (ch != '|');

		int back_buf = 64;
		int back_len = 0;
		char *back = err_malloc("back", back_buf);
		for (;;) {
			if (back_len >= back_buf) {
				back_buf *= 2;
				char *tmp = realloc(back, back_buf);
				if (!tmp)
					die("back realloc failed\n");
				back = tmp;
			}
			n = read(fd, &ch, 1);
			if (n == 1) {
				if (ch == '\n')
					break;
				back[back_len++] = ch;
			}
			if (n == 0)
				die("reached sl EOF before reaching back\n");
			if (n < 0)
				die("read sl error\n");
		}

		char msg[] = "key found:\n";
		write(1, msg, sizeof(msg)-1);
		write(1, back, back_len);
		free(back);
		return;
	}

	/* if it doesn't, search the database and prompt user to add to sl */
	off_t *db_ret = search_db(db_path, key, klen);
	if (!db_ret)
		die("key not found\n");
	int fd = open(db_path, O_RDONLY);
	if (fd < 0)
		die("couldn't open db at %s\n", db_path);
	char **dname = err_malloc("dname", db_ret[1] * sizeof(char*));
	char **back = err_malloc("back",db_ret[1] * sizeof(char*));
	int *back_len = err_malloc("back_len", db_ret[1] * sizeof(int));

	int dictionary_max_len = 128;
	int back_buf = 128;
	for (int i = 0; i < db_ret[1]; ++i) { /* db_ret[1] = number of matches */
		back_len[i] = 0;
		dname[i] = err_malloc("dname[i]", dictionary_max_len);
		back[i] = err_malloc("back[i]", back_buf);
	}

	/* db_ret[0] = first match pos */
	if (lseek(fd, db_ret[0], SEEK_SET) < 0)
		die("lseek db failed\n");

	/* parse each matched line */
	/* format: key|dname|back */
	for (int i = 0; i < db_ret[1]; ++i) {
		/* skip key and pipe */
		char tmp[klen+1];
		if (read(fd, tmp, klen+1) < 0)
			die("read to skip key in add_card failed\n");

		/* copy dname */
		int idx = 0;
		int ch = 0;
		if (read(fd, &ch, 1) != 1) 
			die("early EOF in dname\n");
		do {
			if (ch == '|') break;
			if (idx + 1 == dictionary_max_len) 
				die("dname too long\n");
			dname[i][idx++] = ch;
		} while (read(fd, &ch, 1) == 1);
		if (ch != '|') 
			die("unexpected EOF in key\n");

		/* copy back */
		idx = 0;
		int cap = back_buf;
		while (read(fd, &ch, 1) == 1 && ch != '\n') {
			if (idx + 1 == cap) {
				cap *= 2;
				char *tmp = realloc(back[i], cap);
				if (!tmp) 
					die("realloc entry failed\n");
				back[i] = tmp;
			}
			back[i][idx++] = ch;
		}
		/* no ch != '|' check: EOF indicates last record and is valid */

		back_len[i] = idx;
	}

	close(fd);

	/* write the key in bold ahead of matches print */
	write(1, "\t\033[1m[", 6);
	write(1, key, klen);
	write(1, "]\n\033[0m", 6);

	/* user chooses definition */
	for (int i = 0; i < db_ret[1]; ++i) {
		/* print to stdout
		 * [index] [dictionary] back 
		 * using write() to avoid wchar.h */
		printf("[%d] ", i);
		fflush(stdout);

		int dname_len = 0;
		while(dname[i][dname_len])
			++dname_len;
		write(1, "[", 1);
		write(1, dname[i], dname_len);
		write(1, "] ", 2);

		write(1, back[i], back_len[i]);
		write(1, "\n", 1);
	}

	char msg[] = "choose index or quit (non digit keypress): ";
choose_index:
	write(1, msg, sizeof(msg)-1);

	int i = 0;
	int max_digits = 4;
	char *res_str = calloc(max_digits, sizeof(char));
	if (!res_str)
		die("calloc res_str failed\n");
	for (;;) {
		if (i >= max_digits) {
			fprintf(stderr, "max: 9999\n");
			free(res_str);
			goto choose_index;
		}
		char ch = 0;
		if (read(0, &ch, 1) != 1)
			die("read error in user idx prompt\n");
		if (ch == '\n') {
			if (i == 0) {
				/* user exits */
				fprintf(stderr, "user exits\n");
				exit(1);
			} else {
				/* user confirms their valid choice */
				break;
			}
		}
		if (ch < '0' || ch > '9') {
			die("non digit keypress\n");
		}
		res_str[i++] = ch;
	}
	int res = 0;
	for (int j = 0; j < i; ++j)
		res = 10*res + (res_str[j] - '0');
	free(res_str);

	if (res >= db_ret[1]) {
		fprintf(stderr, "value chosen is out bounds! choose again\n");
		goto choose_index;
	}

	/* final string format: epoch|fsrs_init|key|back\n */
	int final_len = epoch_len + fsrs_len + klen + back_len[res] + 4;
	char *final_str = err_malloc("final_str", final_len);
	pos = 0;

	/* epoch itoa */
	char epoch_str[epoch_len];
	time_t t = time(NULL);
	for (int p = epoch_len-1; p >= 0; --p) {
		epoch_str[p] = (t % 10) + '0';
		t /= 10;
	}

	/* fsrs string */
	char *fsrs_init = "00;0250;0500";

	/* build string manually */
	for (int i = 0; i < epoch_len; ++i)
		final_str[pos++] = epoch_str[i];
	final_str[pos++] = '|';
	for (int i = 0; i < fsrs_len; ++i)
		final_str[pos++] = fsrs_init[i];
	final_str[pos++] = '|';
	for (int i = 0; i < klen; ++i)
		final_str[pos++] = key[i];
	final_str[pos++] = '|';
	for (int i = 0; i < back_len[res]; ++i)
		final_str[pos++] = back[res][i];
	final_str[pos++] = '\n';

	/* free everything */
	for (int i = 0; i < db_ret[1]; ++i) {
		free(dname[i]);
		free(back[i]);
	}
	free(dname);
	free(back);
	free(back_len);
	free(db_ret);

	/* open sl, append, close */
	fd = open(sl_path, O_RDWR | O_APPEND);
	if (fd < 0)
		die("couldn't open sl to append card\n");
	write(fd, final_str, final_len);
	free(final_str);
	close(fd);
	return;
}

char
get_instant_char(void)
{
	if (terminate) 
		return EOF;

	struct termios oldt, newt;
	char ch = 0;
	int n = 0;

	tcgetattr(STDIN_FILENO, &oldt);
	newt = oldt;
	newt.c_lflag &= ~(ICANON | ECHO); 
	tcsetattr(STDIN_FILENO, TCSANOW, &newt);
	n = read(STDIN_FILENO, &ch, 1);
	tcsetattr(STDIN_FILENO, TCSANOW, &oldt);

	if (n == 1) {
		return ch;
	} else if (n == -1) {
		if (errno == EINTR && terminate)
			return EOF;
		else 
			die("read user input failed\n");
	} else if (n == 0) { /* EOF on stdin (e.g., Ctrl+D) */
		terminate = true;
		return EOF;
	}

	/* we should never reach this point */
	return EOF;
}

off_t
search_sl(const char *key, const char *sl_path, const int klen)
{
	char ch;
	char *buf;
	int fd, match;
	off_t offset;
	ssize_t n;

	const int skip_len = epoch_len + fsrs_len + 2;
	buf = err_malloc("buf", skip_len);

	fd = open(sl_path, O_RDONLY);
	if (fd < 0)
		die("couldn't open sl for search\n");

	offset = 0;
	for (;;) {
		offset = lseek(fd, 0, SEEK_CUR);
		if (offset < 0)
			die("lseek in search_sl failed\n");

		/* skip epoch|fsrs| */
		n = read(fd, buf, skip_len);
		if (n < 0)
			die("error skipping fsrs and epoch in search_sl\n");
		if (n < skip_len) {
			/* this condition gets triggered at EOF */
			close(fd);
			free(buf);
			return -1;
		} 

		/* match key */
		match = 1;
		for (int i = 0; i < klen; ++i) {
			/* we should never reach EOF in key */
			if (read(fd, &ch, 1) != 1) 
				die("error read key field in search_sl\n");
			if (ch != key[i]) {
				match = 0;
				break;
			}
		}

		if (match) {
			/* the key has been matched but we need to forbid partial matches */
			if (read(fd, &ch, 1) < 1)
				die("read error in search_sl delimiter check\n");
			if (ch != '|')
				return -1;

			/* now we are good */
			close(fd);
			free(buf);
			return offset;
		}

		/* no match: drain line and loop */
		n = -1;
		do {
			n = read(fd, &ch, 1);
			if (n == 0) { /* EOF */
				close(fd);
				free(buf);
				return -1;
			}
			if (n < 0)
				die("drain line read in search_sl failed\n");
		} while (ch != '\n');
	}
}

int
cmp_key(int fd, const char *key, const int klen)
{
	char ch = 0;
	for (int i = 0; i < klen; ++i) {
		if (read(fd, &ch, 1) < 1)
			die("read error in cmp_key comparison\n");
		if (ch == '|')
			return -1; /* search_string > db_string */
		if (ch != key[i])
			return (ch - key[i]);
	}

	/* the key has been matched but we need to forbid partial matches */
	if (read(fd, &ch, 1) < 1)
		die("read error in cmp_key delimiter check\n");
	if (ch != '|')
		return 1; /* db_string > search_string */

	return 0;
}

off_t *
search_db(const char *db_path, const char *key, const int klen)
{
	/* this was a PAIN to implement */
	char ch = 0;
	
	int cmp = 1;
	int cnt = 0;
	int fd = -1;

	off_t lo_line = 0;
	off_t hi_line = 0;
	off_t try = 0;
	off_t try_line = 0;
	off_t pos = 0;
	off_t next = 0;
	off_t cur = 0;
	off_t start =0;
	off_t cand = 0;

	fd = open(db_path, O_RDONLY);
	if (fd < 0) 
		die("open db failed in bsearch\n");

	/* go to EOF then scan backwards for line start = "hi" */
	pos = lseek(fd, 0, SEEK_END);
	if (pos < 0)
		die("lseek end failed in bsearch\n");
	while (ch != '\n' && pos > lo_line) {
		lseek(fd, pos - 1, SEEK_SET);
		if (pos < 0)
			die("lseek end failed in bsearch\n");
		read(fd, &ch, 1);
		--pos;
	}
	if (!pos)
		die("no newline in bsearch backward scan of last line\n");
	hi_line = pos + 1; /* go past \n */

	while (lo_line < hi_line) {
		/* go up midway */
		try = lo_line + (hi_line - lo_line) / 2;
		if (lseek(fd, try, SEEK_SET) < 0)
			die("lseek try in bsearch failed\n");
		try_line = try;
		while (try_line > lo_line) {
			if (lseek(fd, try_line - 1, SEEK_SET) < 0)
				die("lseek try_line - 1 in bsearch failed\n");
			read(fd, &ch, 1);
			if (ch=='\n') { 
				/* increment to store offset past the newline */
				++try_line; 
				break; 
			}
			--try_line;
		}
		--try_line; /* one byte too far */

		/* reset back to line start */
		if (lseek(fd, try_line, SEEK_SET) < 0)
			die("lseek reset after debug, before cmp failed\n");
		/* check if we have found key */
		cmp = cmp_key(fd, key, klen);
		/* move back to line start */
		if (lseek(fd, try_line, SEEK_SET) < 0)
			die("lseek reset after cmp failed\n");
		if (cmp < 0) {
			/* search key > db key -> move to next line
			 * this lets us avoid scanning the same bytes */
			ch = 0;
			next = try_line;
			while (ch != '\n') {
				ssize_t n = read(fd, &ch, 1);
				if (n < 0)
					die("read error moving up in db search\n");
				if (n == 0) {
					/* EOF */
					next = hi_line; 
					break; 
				}
				++next;
			} 
			++next; /* move past \n */
			lo_line = next;
		} else if (cmp > 0) {
			/* search key < db key -> move down */
			hi_line = try_line;
		} else {
			/* found one match */
			pos = try_line;
			break;
		}

		/* no movement → not found */
		if (lo_line >= hi_line) {
			close(fd);
			return NULL;
		}
	}

	if (cmp != 0) {
		close(fd);
		return NULL;
	}

	/* rewind to first matching line */
	start = pos;
	for (;;) {
		/* 1) find the newline that precedes this line */
		long scan = start - 1;
		char c;
		while (scan > 0) {
			lseek(fd, scan, SEEK_SET);
			read(fd, &c, 1);
			if (c == '\n') break;
			--scan;
		}
		if (scan == 0) break;          /* no more lines above */

		/* 2) find the newline *before* that one */
		long scan2 = scan - 1;
		while (scan2 > 0) {
			lseek(fd, scan2, SEEK_SET);
			read(fd, &c, 1);
			if (c == '\n') { scan2++; break; }
			--scan2;
		}
		/* now scan2 is 0 or the start of the previous line */
		cand = (scan2 == 0 ? 0 : scan2);

		/* if no earlier candidate, we’re done */
		if (cand >= start) break;

		/* test if that line matches */
		lseek(fd, cand, SEEK_SET);
		if (cmp_key(fd, key, klen) == 0) {
			start = cand;    /* move up to that earlier duplicate */
		} else {
			break;
		}
	}

	/* count matches forward from first */
	cur = start;
	for (;;) {
		/* position at the start of current line */
		if (lseek(fd, cur, SEEK_SET) < 0)
			die("lseek count start failed\n");

		/* compare key field */
		if (cmp_key(fd, key, klen))
			break; /* all matches found */
		++cnt;

		/* drain line */
		ch = 0;
		while (ch != '\n') {
			ssize_t n = read(fd, &ch, 1);
			if (n == 0) 
				die("EOF read while draining line in match count\n");
			if (n < 0)
				die("read failed while draining line in match count\n");
		}

		next = lseek(fd, 0, SEEK_CUR);
		if (next <= cur)
			die("match count loop stuck\n");
		cur = next;
	}

	long *db_ret = malloc(2 * sizeof(long));
	if (!db_ret)
		die("malloc db_ret failed in bsearch\n");
	db_ret[0] = start;
	db_ret[1] = cnt;

	close(fd);
	return db_ret;
}

void 
die(const char *fmt, ...) 
{
    va_list args;
    va_start(args, fmt);

    fprintf(stderr, "Error: ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");

    va_end(args);
    exit(1);
}

void *err_malloc
(const char *var, size_t size) 
{
    void *ptr = malloc(size);
    if (!ptr) {
        fprintf(stderr, "malloc for %s (%zu bytes) failed: \n", var, size);
        perror("");
        exit(1);
    }
    return ptr;
}

char*
build_ccq_path(const char *suffix)
{
	char *path = malloc(path_len);
	char *home = getenv("HOME");
	if (!home || home[0] == '\0')
		die("home envp not set or empty");

	int n = snprintf(path, path_len, "%s%s%s", home, CCQ_DATA_DIR, suffix);
	if (n >= path_len)
		die("path too long\n");

	return path;
}

void 
usage(void) 
{
	fprintf(stderr, "usage: ccq -nor <study_list>\n");
	fprintf(stderr, "       ccq -q <key> -d <database> -s <study_list>\n");
	exit(1);
}

void
handle_signal(int sig)
{
	(void)sig;
	terminate = true;
}



/* ----------------------------- */
/* FSRS scheduling code. boring! */
/* ----------------------------- */

/* retrievability */
double retr(double t,double s) { return pow(1.0+F*(t/s),C); }

/* interval (days) for desired retention rd */
double interval(double rd,double s) { return (s/F)*(pow(rd,1.0/C)-1.0); }

/* initial stability & difficulty */
double s0(int g){return W[g-1];}
double d0(int g)
{
	double d=W[4]-exp(W[5]*(g-1))+1.0;
	return fmin(fmax(d,1.0),10.0);
}

/* stability updates */
double
s_succ(double d,double s,double r,int g)
{
	double a=1.0+(11.0-d)*pow(s,-W[9])*(exp(W[10]*(1.0-r))-1.0)*
		((g==2)?W[15]:1.0)*((g==4)?W[16]:1.0)*exp(W[8]);
	return s*a;
}

double
s_fail(double d,double s,double r)
{
	double sf=W[11]*pow(d,-W[12])*(pow(s+1.0,W[13])-1.0)*exp(W[14]*(1.0-r));
	return fmin(sf,s);
}

double stab(double d,double s,double r,int g) { return (g==1)?s_fail(d,s,r):s_succ(d,s,r,g); }

/* difficulty update */
double dd(int g){return -W[6]*((double)g-3.0);}
double dp(double d,int g){return d+dd(g)*((10.0-d)/9.0);}
double
diff(double d,int g)
{
	double dn=W[7]*d0(4)+(1.0-W[7])*dp(d,g);
	return fmin(fmax(dn,1.0),10.0);
}

/* main scheduler
g: 1=again,2=hard,3=good,4=easy
reps: 0 if first review
s,d: current stability & difficulty
elap: days since last review
rd: desired retention (e.g. 0.9)
returns interval in seconds (rounded to nearest sec or whole‑day sec)
*/

long
schedule(int g,int reps,double s,double d,double elap,double rd)
{
	if(!reps){s=s0(g);d=d0(g);}
	double r=retr(elap,s);
	s=stab(d,s,r,g);
	d=diff(d,g);
	double next=interval(rd,s);          /* days   */
	double sec=next*86400.0;             /* exact  */
	double sec_day=ceil(next)*86400.0;   /* whole‑day */
	return ( fabs(sec-elap*86400.0) < fabs(sec_day-elap*86400.0) ) 
		? sec : sec_day;
}
