/* to increase performance / reduce syscall overhead
 * buffered reads could be an idea, but we'd need
 * a global pos variable to correctly update offsets */

#include <fcntl.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>

#include <stdio.h>

#define die(msg) do{write(2,msg,sizeof msg);exit(1);}while(0)

static const char ccq_infix[] = "/.local/share/ccq/";
static const char db_name[] = "db";

static const int epoch_len   = 10;
static const int fsrs_len    = 12;
static const int dname_max   = 64;
static const int init_buf    = 128;
static const int max_results = 999;

void add_card(char *path_db, char *path_sl, char *key);
void change_card(char *path_sl, char *key);
void delete_card(char *path_sl, char *key);

static int cmp_key(int fd, char *key, int klen);
long *bsearch_db(char *path_db, char *key, int klen);
long lsearch_sl(char *path_sl, char *key, int klen);

int
main(int argc, char *argv[])
{
	char *home = 0;
	char *key = 0;
	char *sl = 0;
	char flag = 0;
	char path_db[128] = {0};
	char path_sl[128] = {0};
	int want_add = 0, want_change = 0, want_delete = 0;
	unsigned long pos_sl = 0, pos_db = 0;

	if (argc != 4) 
		die("usage: gene -acd <study_list> <key>\n");

	flag = argv[1][1];
	sl   = argv[2];
	key  = argv[3];
	
	if (!flag || !sl || !key)
		die("usage: gene -acd <study_list> <key>\n");

	if (flag == 'a')
		want_add = 1;
	else if (flag == 'c')
		want_change = 1;
	else if (flag == 'd')
		want_delete = 1;
	else
		die("usage: gene -acd <study_list> <key>\n");

	home = getenv("HOME");
	if (!home)
		die("home envp not set\n");

	/* build path_sl = HOME + ccq_infix + sl */
	for (long i = 0; home[i] && pos_sl < sizeof(path_sl); ++i)
		path_sl[pos_sl++] = home[i];
	for (long i = 0; ccq_infix[i] && pos_sl < sizeof(path_sl); ++i)
		path_sl[pos_sl++] = ccq_infix[i];
	for (long i = 0; sl[i] && pos_sl < sizeof(path_sl); ++i)
		path_sl[pos_sl++] = sl[i];
	if (pos_sl >= sizeof(path_sl))
		die("path_sl too long\n");

	/* build path_db = HOME + ccq_infix + db_name */
	for (long i = 0; home[i] && pos_db < sizeof(path_db); ++i)
		path_db[pos_db++] = home[i];
	for (long i = 0; ccq_infix[i] && pos_db < sizeof(path_db); ++i)
		path_db[pos_db++] = ccq_infix[i];
	for (long i = 0; db_name[i] && pos_db < sizeof(path_db); ++i)
		path_db[pos_db++] = db_name[i];
	if (pos_db >= sizeof(path_db))
		die("path_db too long\n");

	if (want_add)
		add_card(path_db, path_sl, key);
	if (want_change)
		change_card(path_sl, key);
	if (want_delete)
		delete_card(path_sl, key);

	return 0;
}

void
add_card(char *path_db, char *path_sl, char *key)
{
	/* initialize */
	char ch   = 0;
	int  cap  = 0;
	int  idx  = 0;
	int  klen = 0;
	int  nc   = 0;

	while(key[klen])
		++klen;

	/* check if key already in sl */
	if (lsearch_sl(path_sl, key, klen))
		die("duplicate: key found in sl\n");

	/* check if key not in db */
	long *pos_arr = bsearch_db(path_db, key, klen);
	if (!pos_arr)
		die("no information: key not found in db\n");

	int fd = open(path_db, O_RDONLY);
	if (fd < 0)
		die("couldn't open db in add_card\n");


	if (pos_arr[1] > max_results)
		die("too many matches from the bsearch\n");
	else if (pos_arr[1] > 99)
		nc = 3;
	else if (pos_arr[1] > 9)
		nc = 2;
	else if (pos_arr[1] > 0 && pos_arr[1] <= 9)
		nc = 1;

	char **dname = malloc(pos_arr[1] * sizeof(char*));
	char **back = malloc(pos_arr[1] * sizeof(char*));
	int *back_len = calloc(pos_arr[1], sizeof(int));
	if (!dname || !back || !back_len)
		die("dname/back/back_len malloc/calloc failed\n");
	cap = init_buf;
	for (int i = 0; i < pos_arr[1]; ++i) {
		dname[i] = calloc(dname_max, sizeof(char));
		back[i] = calloc(cap, sizeof(char));
		if (!dname[i] || !back[i])
			die("dname[i]/back[i] calloc failed\n");
	}

	if (lseek(fd, pos_arr[0], SEEK_SET) < 0)
		die("lseek db failed\n");

	/* parse each matched line */
	/* format: key|dname|back */
	for (int i = 0; i < pos_arr[1]; ++i) {
		/* skip key and pipe */
		char tmp[klen+1];
		if (read(fd, tmp, klen+1) < 0)
			die("read to skip key in add_card failed\n");

		/* copy dname */
		idx = 0;
		if (read(fd, &ch, 1) != 1) 
			die("early EOF in dname\n");
		do {
			if (ch == '|') break;
			if (idx + 1 == dname_max) 
				die("dname too long\n");
			dname[i][idx++] = ch;
		} while (read(fd, &ch, 1) == 1);
		if (ch != '|') 
			die("unexpected EOF in key\n");

		/* copy back */
		idx = 0;
		cap = init_buf;
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

	/* user chooses definition */
	for (int i = 0; i < pos_arr[1]; ++i) {
		char num[3]; 
		int  len = 0;

		if (i >= 100) {             
			num[0] = (i / 100) + '0';
			num[1] = (i / 10) % 10 + '0';
			num[2] = i % 10 + '0';
			len = 3;
		} else if (i >= 10) { 
			num[0] = (i / 10) + '0';
			num[1] = i % 10 + '0';
			len = 2;
		} else { 
			num[0] = i + '0';
			len = 1;
		}
		write(1, "[", 1);
		write(1, num, len);
		write(1, "] ", 2);

		int dname_len = 0;
		while(dname[i][dname_len])
			++dname_len;
		write(1, "[", 1);
		write(1, dname[i], dname_len);
		write(1, "] ", 2);

		write(1, back[i], back_len[i]);
		write(1, "\n", 1);
	}

	char msg[] = "choose index: ";
	write(1, msg, sizeof msg);

	/* res atoi */
	char res_str[nc];
	read(0, res_str, nc);
	int res = 0;
	for (int i = 0; i < nc; ++i) {
		char c = res_str[i];
		if (c < '0' || c > '9')
			die("non digit char in res_str atoi\n");
		res = res * 10 + (c - '0');
	}
	if (res >= pos_arr[1])
		die("value out of bounds\n");

	/* final string format: epoch|fsrs_init|key|back\n */
	int final_len = epoch_len + fsrs_len + klen + back_len[res] + 4;
	char final_str[final_len];
	int pos = 0;

	/* epoch itoa */
	char epoch_str[epoch_len];
	time_t t = time(NULL);
	for (int p = epoch_len-1; p >= 0; --p) {
		epoch_str[p] = (t % 10) + '0';
		t /= 10;
	}

	/* fsrs string */
	char *fsrs_init = "00;0250;0500";

	/* build string */
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
	for (int i = 0; i < pos_arr[1]; ++i) {
		free(dname[i]);
		free(back[i]);
	}
	free(dname);
	free(back);
	free(back_len);
	free(pos_arr);

	/* open sl, append, close */
	fd = open(path_sl, O_RDWR | O_APPEND);
	if (fd < 0)
		die("couldn't open sl to append card\n");
	write(fd, final_str, final_len);
	close(fd);
}

void change_card(char *path_sl, char *key) {
	int klen = 0;
	long off = 0;

	while(key[klen])
		++klen;
	off = lsearch_sl(path_sl, key, klen);
	if (!off)
		die("key not found in sl\n");
	return;
}

void delete_card(char *path_sl, char *key) {
	int klen = 0;
	long off = 0;

	while(key[klen])
		++klen;
	off = lsearch_sl(path_sl, key, klen);
	if (!off)
		die("key not found in sl\n");

	/* find next line's start offset off_2 */
	/* store bytes raw_file from that to EOF */
	/* write raw_file from off_1 onwards */

	return;
}

long
lsearch_sl(char *path_sl, char *key, int klen)
{
	int fd = open(path_sl, O_RDONLY);
	if (fd < 0)
		die("couldn't open sl for lsearch\n");

	const int skip_len = epoch_len + fsrs_len + 2;
	long offset = 0;
	char ch;

	for (;;) {
		offset = lseek(fd, 0, SEEK_CUR);
		if (offset < 0)
			die("lseek in lsearch failed\n");

		/* skip epoch|fsrs| */
		for (int i = 0; i < skip_len; ++i) {
			/* we might have reached EOF -> return 0 */
			if (read(fd, &ch, 1) != 1)  {
				close(fd);
				return 0;
			}
		}

		int match = 1;
		for (int i = 0; i < klen; ++i) {
			/* we should never reach EOF in key */
			if (read(fd, &ch, 1) != 1) 
				die("error read key field in lsearch\n");
			if (ch != key[i]) {
				match = 0;
				break;
			}
		}

		if (match) {
			close(fd);
			return offset;
		}

		/* no match: drain line and loop */
		do {
			// TODO buffer this to reduce amount of syscalls
			ssize_t n = read(fd, &ch, 1);
			if (n == 0) { /* EOF */
				close(fd);
				return 0;
			}
			if (n < 0)
				die("drain line read in lsearch failed\n");
		} while (ch != '\n');
	}
}

/* compare the first field at current fd position to key 
 * return 0 on success, otherwise return db_string[i] - search_string[i] */
static int
cmp_key(int fd, char *key, int klen)
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

long*
bsearch_db(char *path_db, char *key, int klen)
{
	char ch = 0;
	
	int cmp = 1;
	int cnt = 0;
	int fd = -1;

	long lo_line = 0;
	long hi_line = 0;
	long try = 0;
	long try_line = 0;
	long pos = 0;
	long next = 0;
	long cur = 0;
	long start =0;
	long cand = 0;

	fd = open(path_db, O_RDONLY);
	if (fd < 0) 
		die("open db failed in bsearch\n");

	/* find last line start offset */
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


//		/* DEBUG PROBE */
//		{
//		if (lseek(fd, try_line, SEEK_SET) < 0)
//			die("lseek reset before debug\n");
//			/* a small stack buffer */
//			char buf[64];
//			int  blen = 0;
//			off_t save = lseek(fd, try_line, SEEK_SET);
//			if (save < 0) die("lseek before debug probe\n");
//
//			/* read up to ‘|’ or until buf is full */
//			while (blen < (int)sizeof(buf) - 1) {
//				char c;
//				ssize_t n = read(fd, &c, 1);
//				if (n != 1 || c == '|') break;
//				buf[blen++] = c;
//			}
//			/* dump it to stderr */
//			write(2, "probe=\"", 7);
//			write(2, buf, blen);
//			write(2, "\"\n", 2);
//
//			/* restore for the real compare */
//			if (lseek(fd, try_line, SEEK_SET) < 0)
//				die("lseek reset after debug probe\n");
//		}

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

	write(1, key, klen);
	char msg[] = " found. counting matches\n";
	write(1, msg, sizeof msg);

	/* rewind to first matching line */
	start = pos;
	while (start > 0) {
		if (lseek(fd, start - 1, SEEK_SET) < 0)
			die("lseek rewind to first match failed\n");
		if (read(fd, &ch, 1) < 1)
			die("read rewind to first match failed\n");
		if (ch == '\n') {
			/* candidate line */
			cand = lseek(fd, 0, SEEK_CUR);
			if (cand < 0)
				die("lseek cand failed\n");

			/* only test if strictly earlier than start offset */
			if (cand >= start)
				break;

			fprintf(stderr, "[DBG] testing line at %ld\n", cand);
			if (cmp_key(fd, key, klen) == 0) {
				start = cand;
				fprintf(stderr, "[DBG] rewound to earlier match at %ld\n", start);
            
				continue;
			}
			break;
		}
		--start;
	}
	fprintf(stderr, "[DBG] first matching line at %ld\n", start);

	/* count matches forward from first */
	cur = start;
	for (;;) {
		/* position at the start of current line */
		if (lseek(fd, cur, SEEK_SET) < 0)
			die("lseek count start failed\n");
		fprintf(stderr, "[DBG] counting at %ld\n", cur);

		/* compare key field */
		if (cmp_key(fd, key, klen)) {
			fprintf(stderr, "[DBG] no match at %ld, stopping\n", cur);
			break; /* all matches found */
		}
		++cnt;
		fprintf(stderr, "[DBG] matched #%d at %ld\n", cnt, cur);

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

	long *pos_arr = malloc(2 * sizeof(long));
	if (!pos_arr)
		die("malloc pos_arr failed in bsearch\n");
	pos_arr[0] = start;
	pos_arr[1] = cnt;

	char msg2[] = "match count done, exiting bsearch fn\n";
	write(1, msg2, sizeof msg2);

	close(fd);
	return pos_arr;
}
