#

#include "rcv.h"

/*
 * Mail -- a mail program
 *
 * Lexical processing of commands.
 */

/*
 * Interpret user commands one by one.  If standard input is not a tty,
 * print no prompt.
 */

int	*msgvec;

commands()
{
	int prompt, firstsw, stop();
	register int n;
	char linebuf[LINESIZE];

	msgvec = (int *) calloc((unsigned) (msgCount + 1), sizeof *msgvec);
	if (rcvmode)
		if (signal(SIGINT, SIG_IGN) == SIG_DFL)
			signal(SIGINT, stop);
	input = stdin;
	prompt = 1;
	if (!intty)
		prompt = 0;
	firstsw = 1;
	for (;;) {
		setexit();
		if (firstsw > 0) {
			firstsw = 0;
			source1(mailrc);
			if (!nosrc)
				source1(MASTER);
		}

		/*
		 * How's this for obscure:  after we
		 * finish sourcing for the first time,
		 * go off and print the headers!
		 */

		if (firstsw == 0 && !sourcing) {
			firstsw = -1;
			if (rcvmode)
				announce();
		}

		/*
		 * Print the prompt, if needed.  Clear out
		 * string space, and flush the output.
		 */

		if (!rcvmode && !sourcing)
			return;
		if (prompt && !sourcing)
			printf("_\r");
		flush();
		sreset();

		/*
		 * Read a line of commands from the current input
		 * and handle end of file specially.
		 */

		n = 0;
		for (;;) {
			if (readline(input, &linebuf[n]) <= 0) {
				if (n != 0)
					break;
				if (sourcing) {
					unstack();
					goto more;
				}
				if (!edit) {
					signal(SIGINT, SIG_IGN);
					return;
				}
				edstop();
				return;
			}
			if ((n = strlen(linebuf)) == 0)
				break;
			n--;
			if (linebuf[n] != '\\')
				break;
			linebuf[n++] = ' ';
		}
		if (execute(linebuf))
			return;
more:		;
	}
}

/*
 * Execute a single command.  If the command executed
 * is "quit," then return non-zero so that the caller
 * will know to return back to main, if he cares.
 */

execute(linebuf)
	char linebuf[];
{
	char word[LINESIZE];
	char *arglist[MAXARGC];
	struct cmd *com;
	register char *cp, *cp2;
	register int c;
	int edstop(), e;

	/*
	 * Strip the white space away from the beginning
	 * of the command, then scan out a word, which
	 * consists of anything except digits and white space.
	 *
	 * Handle ! escapes differently to get the correct
	 * lexical conventions.
	 */

	cp = linebuf;
	while (any(*cp, " \t"))
		cp++;
	if (*cp == '!') {
		if (sourcing) {
			printf("Can't \"!\" while sourcing\n");
			unstack();
			return(0);
		}
		shell(cp+1);
		return(0);
	}
	cp2 = word;
	while (*cp && !any(*cp, " \t0123456789$^.-+*'\""))
		*cp2++ = *cp++;
	*cp2 = '\0';

	/*
	 * Look up the command; if not found, bitch.
	 * Normally, a blank command would map to the
	 * first command in the table; while sourcing,
	 * however, we ignore blank lines to eliminate
	 * confusion.
	 */

	if (sourcing && equal(word, ""))
		return(0);
	com = lex(word);
	if (com == NONE) {
		printf("What?\n");
		if (sourcing)
			unstack();
		return(0);
	}

	/*
	 * Special case so that quit causes a return to
	 * main, who will call the quit code directly.
	 * If we are in a source file, just unstack.
	 */

	if (com->c_func == edstop && sourcing) {
		unstack();
		return(0);
	}
	if (!edit && com->c_func == edstop) {
		signal(SIGINT, SIG_IGN);
		return(1);
	}

	/*
	 * Process the arguments to the command, depending
	 * on the type he expects.  Default to an error.
	 * If we are sourcing an interactive command, it's
	 * an error.
	 */

	if (!rcvmode && (com->c_argtype & M) == 0) {
		printf("May not execute \"%s\" while sending\n",
		    com->c_name);
		unstack();
		return(0);
	}
	if (sourcing && com->c_argtype & I) {
		printf("May not execute \"%s\" while sourcing\n",
		    com->c_name);
		unstack();
		return(0);
	}
	e = 1;
	switch (com->c_argtype & ~(P|I|M)) {
	case MSGLIST:
		/*
		 * A message list defaulting to nearest forward
		 * legal message.
		 */
		if ((c = getmsglist(cp, msgvec, com->c_msgflag)) < 0)
			break;
		if (c  == 0) {
			*msgvec = first(com->c_msgflag,
				com->c_msgmask);
			msgvec[1] = NULL;
		}
		if (*msgvec == NULL) {
			printf("No applicable messages\n");
			break;
		}
		e = (*com->c_func)(msgvec);
		break;

	case NDMLIST:
		/*
		 * A message list with no defaults, but no error
		 * if none exist.
		 */
		if (getmsglist(cp, msgvec, com->c_msgflag) < 0)
			break;
		e = (*com->c_func)(msgvec);
		break;

	case STRLIST:
		/*
		 * Just the straight string, with
		 * leading blanks removed.
		 */
		while (any(*cp, " \t"))
			cp++;
		e = (*com->c_func)(cp);
		break;

	case RAWLIST:
		/*
		 * A vector of strings, in shell style.
		 */
		if ((c = getrawlist(cp, arglist)) < 0)
			break;
		if (c < com->c_minargs) {
			printf("%s requires at least %d arg(s)\n",
				com->c_name, com->c_minargs);
			break;
		}
		if (c > com->c_maxargs) {
			printf("%s takes no more than %d arg(s)\n",
				com->c_name, com->c_maxargs);
			break;
		}
		e = (*com->c_func)(arglist);
		break;

	case NOLIST:
		/*
		 * Just the constant zero, for exiting,
		 * eg.
		 */
		e = (*com->c_func)(0);
		break;

	default:
		panic("Unknown argtype");
	}

	/*
	 * Exit the current source file on
	 * error.
	 */

	if (e && sourcing)
		unstack();
	if (com->c_func == edstop)
		return(1);
	if (value("autoprint") != NOSTR && com->c_argtype & P)
		if ((dot->m_flag & MDELETED) == 0)
			print(dot);
	if (!sourcing)
		sawcom = 1;
	return(0);
}

/*
 * Find the correct command in the command table corresponding
 * to the passed command "word"
 */

struct cmd *
lex(word)
	char word[];
{
	register struct cmd *cp;
	extern struct cmd cmdtab[];

	for (cp = &cmdtab[0]; cp->c_name != NOSTR; cp++)
		if (isprefix(word, cp->c_name))
			return(cp);
	return(NONE);
}

/*
 * Determine if as1 is a valid prefix of as2.
 * Return true if yep.
 */

isprefix(as1, as2)
	char *as1, *as2;
{
	register char *s1, *s2;

	s1 = as1;
	s2 = as2;
	while (*s1++ == *s2)
		if (*s2++ == '\0')
			return(1);
	return(*--s1 == '\0');
}

/*
 * The following gets called on receipt of a rubout.  This is
 * to abort printout of a command, mainly.
 * Dispatching here when command() is inactive crashes rcv.
 * Close all open files except 0, 1, 2, and the temporary.
 * The special call to getuserid() is needed so it won't get
 * annoyed about losing its open file.
 * Also, unstack all source files.
 */

stop()
{
	register FILE *fp;

	noreset = 0;
	signal(SIGINT, SIG_IGN);
	sawcom++;
	while (sourcing)
		unstack();
	getuserid((char *) -1);
	for (fp = &_iob[0]; fp < &_iob[_NFILE]; fp++) {
		if (fp == stdin || fp == stdout)
			continue;
		if (fp == itf || fp == otf)
			continue;
		if (fp == stderr)
			continue;
		fclose(fp);
	}
	if (image >= 0) {
		close(image);
		image = -1;
	}
	clrbuf(stdout);
	printf("Interrupt\n");
	signal(SIGINT, stop);
	reset(0);
}

/*
 * Announce the presence of the current Mail version,
 * give the message count, and print a header listing.
 */

char	*greeting	= "Mail version 2.0 %s.  Type ? for help.\n";

announce()
{
	int vec[2];
	extern char *version;
	register struct message *mp;

	if (value("hold") != NOSTR)
		for (mp = &message[0]; mp < &message[msgCount]; mp++)
			mp->m_flag |= MPRESERVE;
	vec[0] = 1;
	vec[1] = 0;
	if (value("quiet") == NOSTR)
		printf(greeting, version);
	if (msgCount == 1)
		printf("1 message:\n");
	else
		printf("%d messages:\n", msgCount);
	headers(vec);
}

strace() {}

/*
 * Print the current version number.
 */

pversion(e)
{
	printf(greeting, version);
	return(0);
}
