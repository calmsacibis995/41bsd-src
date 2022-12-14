static char *sccsid = "@(#)colcrt.c	4.1 (Berkeley) 10/1/80";
#include <stdio.h>
/*
 * colcrt - replaces col for crts with new nroff esp. when using tbl.
 * Bill Joy UCB July 14, 1977
 *
 * This filter uses a screen buffer, 267 half-lines by 132 columns.
 * It interprets the up and down sequences generated by the new
 * nroff when used with tbl and by \u \d and \r.
 * General overstriking doesn't work correctly.
 * Underlining is split onto multiple lines, etc.
 *
 * Option - suppresses all underlining.
 * Option -2 forces printing of all half lines.
 */

char	page[267][132];

int	outline = 1;
int	outcol;

char	buf[256];
char	suppresul;
char	printall;

char	*progname;
FILE	*f;

main(argc, argv)
	int argc;
	char *argv[];
{
	register c;
	register char *cp, *dp;

	argc--;
	progname = *argv++;
	while (argc > 0 && argv[0][0] == '-') {
		switch (argv[0][1]) {
			case 0:
				suppresul = 1;
				break;
			case '2':
				printall = 1;
				break;
			default:
				printf("usage: %s [ - ] [ -2 ] [ file ... ]\n", progname);
				fflush(stdout);
				exit(1);
		}
		argc--;
		argv++;
	}
	setbuf(stdout, buf);
	do {
		if (argc > 0) {
			close(0);
			if ((f=fopen(argv[0], "r")
) < 0) {
				fflush(stdout);
				perror(argv[0]);
				fflush(stdout);
				exit (1);
			}
			argc--;
			argv++;
		}
		for (;;) {
			c = getc(stdin);
			if (c == -1) {
				pflush(outline);
				fflush(stdout);
				break;
			}
			switch (c) {
				case '\n':
					if (outline >= 265)
						pflush(62);
					outline += 2;
					outcol = 0;
					continue;
				case '\016':
					case '\017':
					continue;
				case 033:
					c = getc(stdin);
					switch (c) {
						case '9':
							if (outline >= 266)
								pflush(62);
							outline++;
							continue;
						case '8':
							if (outline >= 1)
								outline--;
							continue;
						case '7':
							outline -= 2;
							if (outline < 0)
								outline = 0;
							continue;
						default:
							continue;
					}
				case '\b':
					if (outcol)
						outcol--;
					continue;
				case '\t':
					outcol += 8;
					outcol &= ~7;
					outcol--;
					c = ' ';
				default:
					if (outcol >= 132) {
						outcol++;
						continue;
					}
					cp = &page[outline][outcol];
					outcol++;
					if (c == '_') {
						if (suppresul)
							continue;
						cp += 132;
						c = '-';
					}
					if (*cp == 0) {
						*cp = c;
						dp = cp - outcol;
						for (cp--; cp >= dp && *cp == 0; cp--)
							*cp = ' ';
					} else
						if (plus(c, *cp) || plus(*cp, c))
							*cp = '+';
						else if (*cp == ' ' || *cp == 0)
							*cp = c;
					continue;
			}
		}
	} while (argc > 0);
	fflush(stdout);
	exit(0);
}

plus(c, d)
	char c, d;
{

	return (c == '|' && d == '-' || d == '_');
}

int first;

pflush(ol)
	int ol;
{
	register int i, j;
	register char *cp;
	char lastomit;
	int l;

	l = ol;
	lastomit = 0;
	if (l > 266)
		l = 266;
	else
		l |= 1;
	for (i = first | 1; i < l; i++) {
		move(i, i - 1);
		move(i, i + 1);
	}
	for (i = first; i < l; i++) {
		cp = page[i];
		if (printall == 0 && lastomit == 0 && *cp == 0) {
			lastomit = 1;
			continue;
		}
		lastomit = 0;
		printf("%s\n", cp);
	}
	copy(page, page[ol], (267 - ol) * 132);
	clear(page[267- ol], ol * 132);
	outline -= ol;
	outcol = 0;
	first = 1;
}
move(l, m)
	int l, m;
{
	register char *cp, *dp;

	for (cp = page[l], dp = page[m]; *cp; cp++, dp++) {
		switch (*cp) {
			case '|':
				if (*dp != ' ' && *dp != '|' && *dp != 0)
					return;
				break;
			case ' ':
				break;
			default:
				return;
		}
	}
	if (*cp == 0) {
		for (cp = page[l], dp = page[m]; *cp; cp++, dp++)
			if (*cp == '|')
				*dp = '|';
			else if (*dp == 0)
				*dp = ' ';
		page[l][0] = 0;
	}
}

copy(to, from, i)
	register char *to, *from;
	register int i;
{

	if (i > 0)
		do
			*to++ = *from++;
		while (--i);
}

clear(at, cnt)
	register char *at;
	register int cnt;
{

	if (cnt > 0)
		do
			*at++ = 0;
		while (--cnt);
}
