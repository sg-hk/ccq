#include <fcntl.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define die(msg) do{write(2,msg,sizeof msg);exit(1);}while(0)

double schedule(int g,int reps,double s,double d,double elap,double rd);
void handle_signal(int sig);

volatile sig_atomic_t terminate;

int
main(int argc, char *argv[], char *envp[])
{
#ifdef ___OPENBSD___
	if (pledge("stdio rpath wpath", NULL) == -1)
		die("pledge\n");
#endif

	/* --------------- */
	/* initialize vars */
	/* --------------- */
	terminate = 0;
	const char *ccq_infix = "/.local/share/ccq/";
	const int  epoch_len  = 10;
	const int  fsrs_len   = 12;
	const int  path_len   = 128;

	char ch = 0;

	char *due_cnt_str;
	char *home;
	char *res_arr;
	char path[path_len];
	char tmpbuf[12]; /* for due_count itoa */
	char **back;
	char **front;
	char **fsrs;

	int due_cnt_buf = 100;
	int due_count = 0;
	int reviewed_count;
	int fd = -1;
	int is_due = 0, is_fsrs = 0, is_front = 0, is_back = 0;
	int newst_flag = 0, rand_flag = 0;
	int pos = 0; /* for due card offsets */
	int *review_order;

	long *back_len;
	long *front_len;
	long *fsrs_pos;

	ssize_t n = 0; /* for read return value checks */

	time_t now = 0; /* current epoch */
	time_t *epoch_old;

	epoch_old = malloc(due_cnt_buf * sizeof(time_t));
	fsrs_pos = malloc(due_cnt_buf * sizeof(long));
	front_len = malloc(due_cnt_buf * sizeof(long));
	back_len = malloc(due_cnt_buf * sizeof(long));
	if (!epoch_old || !fsrs_pos || !front_len || !back_len)
		die("initial mallocs failed\n");

	/* set up signal handler */
        struct sigaction sa = {0};
        sa.sa_handler = handle_signal;
        sigemptyset(&sa.sa_mask);
        if (sigaction(SIGINT,  &sa, NULL) == -1 ||
            sigaction(SIGTERM, &sa, NULL) == -1)
		die("signal action failed\n");

	/* ---------- */
	/* parse args */
	/* ---------- */

	if (argc == 2)
		goto parsed_args;
	else if (argc != 3)
		die("usage: argh (-r/-n) <study_list>\n");

	if (argv[1][1] == 'n') {
		newst_flag = 1;
	} else if (argv[1][1] == 'r') {
		rand_flag = 1;
	} else
		die("usage: argh (-r/-n) <study_list>\n");

parsed_args:

	/* ---------- */
	/* build path */
	/* ---------- */
	/* manual getenv() */
	home = 0;
	for (int i = 0; envp[i]; ++i) {
		if (envp[i][0] == 'H' && envp[i][1] == 'O' &&
		    envp[i][2] == 'M' && envp[i][3] == 'E' && envp[i][4] == '=') {
			home = envp[i] + 5;
			break;
		}
	}
	if (!home)
		die("home envp not found\n");


	/* manual snprintf */
	for (int i = 0; home[i] && pos < path_len-1; ++i)
		path[pos++] = home[i];


	for (int i = 0; ccq_infix[i] && pos < path_len-1; ++i)
		path[pos++] = ccq_infix[i];


	int sl_pos = (argc == 2) ? 1 : 2;
	for (int i = 0; argv[sl_pos][i] && pos < path_len-1; ++i)
		path[pos++] = argv[sl_pos][i];

	
	path[pos] = 0;

#ifdef ___OPENBSD___
    if (unveil(path, "rw") == -1)
        die("unveil path\n");
    unveil(NULL, NULL);
#endif

	fd = open(path, O_RDONLY);
	if (fd < 0)
		die("can't open file\n");



	/* ---------- */
	/* check dues */
	/* ---------- */
	now = time(NULL);
	pos = 0;
	for (;;) {
		char epoch_str[epoch_len];
		long epoch = 0;

		n = read(fd, epoch_str, epoch_len);
		if (n == 0) /* EOF before epoch */
			break;
		if (n != 10) 
			die("epoch field read error\n");

		pos += epoch_len;

		for (int i = 0; i < epoch_len; ++i) {
			if (epoch_str[i] < '0' || epoch_str[i] > '9')
				die("non digit char in epoch\n");
			epoch = epoch * 10 + (epoch_str[i] - '0');
		}

		if (epoch <= now) {
			if (due_count >= due_cnt_buf) {
				due_cnt_buf *= 2;
				long *tepo = realloc(epoch_old, due_cnt_buf * sizeof(time_t));
				long *tpos = realloc(fsrs_pos, due_cnt_buf * sizeof(long));
				long *tfrlen = realloc(front_len, due_cnt_buf * sizeof(long));
				long *tbklen = realloc(back_len, due_cnt_buf * sizeof(long));
				if (!tepo || !tpos || !tfrlen || !tbklen)
					die("realloc fsrs_pos failed\n");
				epoch_old = tepo;
				fsrs_pos = tpos;
				front_len = tfrlen;
				back_len = tbklen;
			}
			epoch_old[due_count] = epoch;
			fsrs_pos[due_count] = pos + 1; /* EPOCH|FSRS -> +1 to skip pipe */
			++due_count;
			is_due = 1;
		}

		/* skip rest of line
		 * when the card is due we want to store length of due fields
		 * these two local vars help us quickly calculate them */
		long front_start = 0;
		long back_start = 0;
		int has_newline = 0;
		do {
			n = read(fd, &ch, 1);
			if (n == 0) { /* EOF inside record: last record */
				if (is_due)
					back_len[due_count-1] = pos - back_start;
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
					front_start = fsrs_pos[due_count-1] + fsrs_len + 1;
				} else if (!is_front) {
					is_front = 1;
				} else if (!is_back) {
					/* we are at |BACK, front ends 1 before */
					front_len[due_count-1] = pos - front_start - 1;
					back_start = pos;
					is_back = 1;
				} 
			}
		} while (ch != '\n');

		if (is_due) 
			back_len[due_count-1] = pos - back_start - has_newline;

		is_due = is_fsrs = is_front = is_back = 0;

		if (n == 0)  /* EOF after record */
			break;
	}


	if (due_count == 0)
		die("no cards due\n");

	/* ----------- */
	/* copy fields */
	/* ----------- */
	back = malloc(due_count * sizeof(char*));
	front = malloc(due_count * sizeof(char*));
	fsrs = malloc(due_count * sizeof(char*));
	if (!back || !front || !fsrs)
		die("field array mallocs failed\n");


	for (int i = 0; i < due_count; ++i) {
		front[i] = malloc(front_len[i]);
		back[i] = malloc(back_len[i]);
		fsrs[i] = malloc(fsrs_len);
		if (!front[i] || !back[i] || !fsrs[i])
			die("field malloc failed\n");

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
	review_order = malloc(due_count * sizeof(int));
	if (!review_order)
		die("review_order malloc failed\n");

	/* default order 0, ..., due_count - 1 */
	for (int i = 0; i < due_count; ++i)
		review_order[i] = i;

	/* newest first: reverse */
	if (newst_flag) {
		for (int i = 0; i < due_count / 2; ++i) {
			int tmp = review_order[i];
			review_order[i] = review_order[due_count - 1 - i];
			review_order[due_count - 1 - i] = tmp;
		}

	/* random: Fisher–Yates shuffle */
	} else if (rand_flag) {
		srand(now);
		for (int i = due_count - 1; i > 0; --i) {
			int j = rand() % (i + 1); /* [0,i] */
			int tmp = review_order[i];
			review_order[i] = review_order[j];
			review_order[j] = tmp;
		}
	}

	res_arr = malloc(due_count);
	if (!res_arr)
		die("res_arr malloc failed\n");

	/* itoa */
	due_cnt_str = tmpbuf + sizeof tmpbuf;
	int tmp = due_count;
	do {
		*--due_cnt_str = (tmp % 10) + '0';
		tmp /= 10;
	} while (tmp);

	write(1, due_cnt_str, tmpbuf + sizeof tmpbuf - due_cnt_str);
	char start_msg[] = " cards due\nenter to show back; enter for PASS, other + enter for FAIL\n\n";
	write(1, start_msg, sizeof start_msg);

	reviewed_count = 0;
	for (; (reviewed_count < due_count) && !terminate; ++reviewed_count) {
		/* requiring user \n is a really hacky way to avoid using termios.h
		 * a new line gets printed since canonical mode requires \n for read 
		 * and it is the only character that gets read by read(0, ...)
		 * this way we achieve roughly the same behaviour as with ~(ICANON | ECHO) */

		int k = review_order[reviewed_count];
		printf("[%d/%d]  ", reviewed_count+1, due_count);
		fflush(stdout);
		write(1, front[k], front_len[k]);
		free(front[k]);

		if (read(0, &ch, 1) != 1)
			die("read user input failed\n");

		/* flush stdin in case user mistakenly didn't press \n */
		if (ch != '\n') {
			while (read(0, &ch, 1) == 1 && ch != '\n')
				;
		}

		write(1, back[k], back_len[k]);
		free(back[k]);

		if (read(0, &res_arr[k], 1) != 1)
			die("read user input failed\n");

		/* flush stdin in case user chose to fail card
		 * and print fail message */
		if (res_arr[k] != '\n') {
			while (read(0, &ch, 1) == 1 && ch != '\n')
				;
			write(1, "FAIL\n\n", 6);
		} else
			write(1, "PASS\n\n", 6);
	}

	if (terminate) {
		char msg[] = "\nuser terminated, updating reviewed cards\n";
		write(1, msg, sizeof msg);
		/* free skipped fields */
		for (int i = reviewed_count; i < due_count; ++i) {
			int k = review_order[i];
			free(front[k]);
			free(back[k]);
		}
	}

	free(front);
	free(back);

	/* ------------- */
	/* update fields */
	/* ------------- */
	fd = open(path, O_RDWR);
	if (fd < 0)
		die("second file open failed\n");

	for (int i = 0; i < reviewed_count; ++i) {
		int k = review_order[i];

		/* scheduler args */
		int    grade = (res_arr[k] == '\n') ? 3 /* good */ : 1 /* again */;
		double stab  = strtol(fsrs[k] + 3, 0, 10) / 100.0;  /* SSSS → double */
		double diff  = strtol(fsrs[k] + 8, 0, 10) / 100.0;  /* DDDD → double */
		int    reps  = (fsrs[k][0]-'0')*10 + (fsrs[k][1]-'0');
		double elap  = (double)(now - epoch_old[k]) / 86400.0;

		/* interval in seconds */
		double nxt   = schedule(grade, reps, stab, diff, elap, 0.9); 

		/* new epoch */
		time_t new_epoch = now + (time_t)(nxt + 0.5);  /* round */

		/* itoa epoch */
		char epoch_str[epoch_len];
		time_t t = new_epoch;
		for (int p = epoch_len - 1; p >= 0; --p) {
			epoch_str[p] = (t % 10) + '0';
			t /= 10;
		}

		/* i/ftoa fsrs */
		char new_fsrs[fsrs_len];
		int  new_reps = (grade == 1) ? 0 : (reps < 99 ? reps + 1 : 99);

		stab = (grade == 1) ? 2.50 : stab + stab / 4.0 + 0.25;
		if (stab > 99.99) stab = 99.99;

		diff = (grade == 1) ? 5.00 : (diff > 1.25 ? diff - 0.25 : 1.00);

		/* reps */
		new_fsrs[0] = new_reps / 10 + '0';
		new_fsrs[1] = new_reps % 10 + '0';
		new_fsrs[2] = ';';

		/* stability */
		int s100 = (int)(stab * 100 + 0.5);
		new_fsrs[3] = (s100 / 1000)       + '0';
		new_fsrs[4] = (s100 /  100) % 10  + '0';
		new_fsrs[5] = (s100 /   10) % 10  + '0';
		new_fsrs[6] =  s100 %   10        + '0';
		new_fsrs[7] = ';';

		/* difficulty */
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

	close(fd);
	free(back_len);
	free(front_len);
	free(fsrs);
	free(fsrs_pos);
	free(res_arr);
	free(review_order);

	return 0;
}

void
handle_signal(int sig)
{
	(void)sig;
	terminate = 1;
}
