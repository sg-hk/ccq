#include <fcntl.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <locale.h>

#define die(msg) do{write(2,msg,sizeof msg);exit(1);}while(0)

static const char ccq_infix[] = "/.local/share/ccq/";
static const char db_name[] = "db";

static const int epoch_len   = 10;
static const int fsrs_len    = 12;
static const int dname_max   = 64;
static const int init_buf    = 128;
static const int max_results = 999;

void add_card(char *path_db, char *path_sl, char *key);
void change_back(char *path_sl, char *key);
void delete_card(char *path_sl, char *key);

static int cmp_key(int fd, char *key, int klen);
long *bsearch_db(char *path_db, char *key, int klen);
long lsearch_sl(char *path_sl, char *key, int klen);

int
main(int argc, char *argv[])
{
	setlocale(LC_ALL, "");
	/* TODO flagless just search in db 
	 * TODO buffer lsearch drain line to reduce syscalls */

#ifdef ___OPENBSD___
    if (pledge("stdio rpath wpath", NULL) == -1)
        die("pledge\n");
#endif

	char *home = 0;
	char *key = 0;
	char *sl = 0;
	char flag = 0;
	char path_db[128] = {0};
	char path_sl[128] = {0};
	int want_add = 0, want_change = 0, want_delete = 0;
	unsigned long pos_sl = 0, pos_db = 0;

	if (argc != 4) 
		die("usage: fox -acd <study_list> <key>\n");

	flag = argv[1][1];
	sl   = argv[2];
	key  = argv[3];
	
	if (!flag || !sl || !key)
		die("usage: fox -acd <study_list> <key>\n");

	if (flag == 'a')
		want_add = 1;
	else if (flag == 'c')
		want_change = 1;
	else if (flag == 'd')
		want_delete = 1;
	else
		die("usage: fox -acd <study_list> <key>\n");

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

#ifdef ___OPENBSD___
	/* all branches need access to the study list */
	if (unveil(path_sl, "rw") == -1)
		die("unveil path_sl\n");

	/* only card creation requires the database */
	if (want_add) {
		if (unveil(path_db, "rw") == -1)
			die("unveil path_db\n");
	}

	/* no other files ever become accessible */
	unveil(NULL, NULL);
#endif

	if (want_add)
		add_card(path_db, path_sl, key);
	else if (want_change)
		change_back(path_sl, key);
	else if (want_delete)
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
	if (lsearch_sl(path_sl, key, klen) != -1)
		die("duplicate: key found in study list\n");

	/* check if key not in db */
	long *pos_arr = bsearch_db(path_db, key, klen);
	if (!pos_arr)
		die("no information: key not found in database\n");

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

	/* write the key in bold ahead of matches print */
	write(1, "\t\033[1m[", 6);
	write(1, key, klen);
	write(1, "]\n\033[0m", 6);

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
	// TODO declare those
	int res = 0;
	int count = 0;
	ch = 0;

	/* read up to nc digits or until newline */
	while (count < nc && read(0, &ch, 1) == 1) {
		if (ch == '\n')
			break;
		if (ch < '0' || ch > '9')
			die("non digit char in res_str atoi\n");
		res = res * 10 + (ch - '0');
		++count;
	}

	/* because we are in canonical mode (hacking our way out of termios.h)
	 * we need to flush fd=0 (stdin) until newline;
	 * there's bugs otherwise, where the \n is read instead of digit */
	while (ch != '\n' && read(0, &ch, 1) == 1)
		;

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

void 
change_back(char *path_sl, char *key) 
{
	char ch;
	char *old_back, *new_back, *tail;
	int klen, nlen, oldlen, bufold, bufnew, fd;
	long off, back_start, next, end, size;
	ssize_t n;

	klen = 0;
	while(key[klen])
		++klen;

	off = 0;
	off = lsearch_sl(path_sl, key, klen);
	if (off == -1)
		die("key not found in sl\n");

	/* get matched line's back */
	fd = open(path_sl, O_RDWR);
	if (fd < 0)
		die("error open sl in change back\n");

	back_start = off + epoch_len + fsrs_len + klen + 3;
	lseek(fd, back_start, SEEK_SET);
	bufold = 128;
	old_back = malloc(bufold);
	if (!old_back)
		die("malloc error to old_back in change back\n");
	ch = 0;
	while(ch!='\n') {
		n = read(fd, &ch, 1);
		if (n == 0) {
			/* EOF: last line in study list */
			break;
		} else if (n != 1)
			die("read error in change back's back copy\n");

		if (oldlen+1 >= bufold) {
			bufold *= 2;			
			char *tmp = realloc(old_back, bufold);
			if (!tmp)
				die("realloc failed for old_back in change back\n");
			old_back = tmp;
		}
		old_back[oldlen++]=ch;
	}

	/* print old back */
	write(1, "old back: ", 10);
	write(1, old_back, oldlen); 
	free(old_back);

	/* prompt new back */
	char msg_query[] = "new back: ";
	write(1, msg_query, sizeof msg_query);
	bufnew = 128;
	new_back = malloc(bufnew);
	if (!new_back)
		die("malloc error to new_back in change back\n");
	ch = 0, nlen = 0;
	while (ch != '\n') {
		if (nlen >= bufnew) {
			bufnew *= 2;
			char *tmp = realloc(new_back, bufnew);
			if (!tmp)
				die("realloc error for new_back in change back");
			new_back = tmp;
		}
		if (read(0, &ch, 1) != 1)
			die("read stdin error in change back");
		new_back[nlen++] = ch; /* this includes \n */
	}

	/* reopen file, get tail */
	next = lseek(fd, 0, SEEK_CUR);
	end = lseek(fd, 0, SEEK_END);
	size = end - next;
	tail = malloc(size);
	if (!tail)
		die("error malloc tail in change back\n");
	lseek(fd, next, SEEK_SET);
	if (read(fd, tail, size) != size)
		die("error read rest of file in change back\n");

	/* overwrite back */
	lseek(fd, back_start, SEEK_SET);
	if (write(fd, new_back, nlen) != nlen)
		die("write new_back error in change back\n");
	free(new_back);

	/* add rest of file and truncate */
	if (write(fd, tail, size) != size)
		die("write rest of file error in change back\n");
	free(tail);
	ftruncate(fd, back_start + nlen + size);
	close(fd);

	char msg_success[] = "back successfully changed\n";
	write(1, msg_success, sizeof msg_success);

	return;
}

void 
delete_card(char *path_sl, char *key) 
{
	char ch;
	char *tail;
	int klen, fd;
	long off, next, end, size;
	ssize_t n;

	klen = 0;
	while(key[klen])
		++klen;

	off = 0;
	off = lsearch_sl(path_sl, key, klen);
	if (off == -1)
		die("key not found in sl\n");

	ch = 0;
	next = 0;
	n = 0;

	fd = open(path_sl, O_RDWR);
	if (fd < 0)
		die("open sl error in delete card\n");

	/* skip to end of matched line */
	lseek(fd, off, SEEK_SET);
	while (ch != '\n') {
		n = read(fd, &ch, 1);
		if (n == 0) /* EOF: card is last line */
			break;
		if (n < 0)
			die("read '\\n' error in delete card\n");
	}

	/* store positions */
	next = lseek(fd, 0, SEEK_CUR);
	end = lseek(fd, 0, SEEK_END);

	if (next >= end) {
		/* matched line is the last */
		if (off == 0) /* and the first, i.e. the only one */
			ftruncate(fd, 0);
		else /* otherwise truncate to preceding one, removing \n */
			ftruncate(fd, off - 1);
		close(fd);
		return;
	}

	/* read the rest of the file */
	size = end - next;
	tail = malloc(size);
	if (!tail)
		die("malloc error in delete card\n");
	lseek(fd, next, SEEK_SET);
	if (read(fd, tail, size) != size)
		die("read rest of file error in delete card\n");

	/* overwrite starting at 'off' */
	lseek(fd, off, SEEK_SET);
	if (write(fd, tail, size) != size)
		die("write rest of file error in delete card\n");
	free(tail);

	/* shrink */
	ftruncate(fd, off + size);
	close(fd);


	char msg_pre[] = "card [";
	char msg_post[] = "] successfully deleted\n";
	write(1, msg_pre, sizeof msg_pre);
	write(1, key, klen);
	write(1, msg_post, sizeof msg_post);

	return;
}

/* search for key in sl, return offset or -1 */
long
lsearch_sl(char *path_sl, char *key, int klen)
{
	char ch;
	char *buf;
	int fd, match;
	long offset;
	ssize_t n;

	const int skip_len = epoch_len + fsrs_len + 2;
	buf = malloc(skip_len);
	if (!buf)
		die("error malloc to buf in lsearch\n");

	fd = open(path_sl, O_RDONLY);
	if (fd < 0)
		die("couldn't open sl for lsearch\n");

	offset = 0;
	for (;;) {
		offset = lseek(fd, 0, SEEK_CUR);
		if (offset < 0)
			die("lseek in lsearch failed\n");

		/* skip epoch|fsrs| */
		n = read(fd, buf, skip_len);
		if (n < 0)
			die("error skipping fsrs and epoch in lsearch\n");
		if (n < skip_len) {
			/* this condition gets triggered at EOF */
			close(fd);
			return -1;
		} 

		/* match key */
		match = 1;
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
			/* the key has been matched but we need to forbid partial matches */
			if (read(fd, &ch, 1) < 1)
				die("read error in lsearch delimiter check\n");
			if (ch != '|')
				return -1;

			/* now we are good */
			close(fd);
			return offset;
		}

		/* no match: drain line and loop */
		n = -1;
		do {
			n = read(fd, &ch, 1);
			if (n == 0) { /* EOF */
				close(fd);
				return -1;
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


//		/* DEBUG PROBE START */
//		if (lseek(fd, try_line, SEEK_SET) < 0)
//			die("lseek reset before debug\n");
//		/* a small stack buffer */
//		char buf[64];
//		int  blen = 0;
//		off_t save = lseek(fd, try_line, SEEK_SET);
//		if (save < 0) die("lseek before debug probe\n");
//
//		/* read up to ‘|’ or until buf is full */
//		while (blen < (int)sizeof(buf) - 1) {
//			char c;
//			ssize_t n = read(fd, &c, 1);
//			if (n != 1 || c == '|') break;
//			buf[blen++] = c;
//		}
//		/* dump it to stderr */
//		write(2, "probe=\"", 7);
//		write(2, buf, blen);
//		write(2, "\"\n", 2);
//
//		/* restore for the real compare */
//		if (lseek(fd, try_line, SEEK_SET) < 0)
//			die("lseek reset after debug probe\n");
//		/* DEBUG PROBE END */

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

	long *pos_arr = malloc(2 * sizeof(long));
	if (!pos_arr)
		die("malloc pos_arr failed in bsearch\n");
	pos_arr[0] = start;
	pos_arr[1] = cnt;

	close(fd);
	return pos_arr;
}
