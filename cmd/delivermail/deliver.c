# include <stdio.h>
# include <pwd.h>
# include <signal.h>
# include "dlvrmail.h"
# ifdef LOG
# include <log.h>
# endif LOG

static char SccsId[] = "@(#)deliver.c	1.11	10/27/80";

/*
**  DELIVER -- Deliver a message to a particular address.
**
**	Algorithm:
**		Compute receiving network (i.e., mailer), host, & user.
**		If local, see if this is really a program name.
**		Build argument for the mailer.
**		Create pipe through edit fcn if appropriate.
**		Fork.
**			Child: call mailer
**		Parent: call editfcn if specified.
**		Wait for mailer to finish.
**		Interpret exit status.
**
**	Parameters:
**		to -- the address to deliver the message to.
**		editfcn -- if non-NULL, we want to call this function
**			to output the letter (instead of just out-
**			putting it raw).
**
**	Returns:
**		zero -- successfully delivered.
**		else -- some failure, see ExitStat for more info.
**
**	Side Effects:
**		The standard input is passed off to someone.
**
**	WARNING:
**		The standard input is shared amongst all children,
**		including the file pointer.  It is critical that the
**		parent waits for the child to finish before forking
**		another child.
**
**	Called By:
**		main
**		savemail
**
**	Files:
**		standard input -- must be opened to the message to
**			deliver.
*/

deliver(to, editfcn)
	addrq *to;
	int (*editfcn)();
{
	register struct mailer *m;
	char *host;
	char *user;
	extern struct passwd *getpwnam();
	char **pvp;
	extern char **buildargv();
	auto int st;
	register int i;
	register char *p;
	int pid;
	int pvect[2];
	extern FILE *fdopen();
	extern int errno;
	FILE *mfile;
	extern putheader();
	extern pipesig();

	/*
	**  Compute receiving mailer, host, and to addreses.
	**	Do some initialization first.  To is the to address
	**	for error messages.
	*/

	To = to->q_paddr;
	m = to->q_mailer;
	user = to->q_user;
	host = to->q_host;
	Errors = 0;
	errno = 0;
# ifdef DEBUG
	if (Debug)
		printf("deliver(%s [%d, `%s', `%s'])\n", To, m, host, user);
# endif DEBUG

	/*
	**  Remove quote bits from user/host.
	*/

	for (p = user; (*p++ &= 0177) != '\0'; )
		continue;
	if (host != NULL)
		for (p = host; (*p++ &= 0177) != '\0'; )
			continue;
	
	/*
	**  Strip quote bits from names if the mailer wants it.
	*/

	if (flagset(M_STRIPQ, m->m_flags))
	{
		stripquotes(user);
		stripquotes(host);
	}

	/*
	**  See if this user name is "special".
	**	If the user is a program, diddle with the mailer spec.
	**	If the user name has a slash in it, assume that this
	**		is a file -- send it off without further ado.
	**		Note that this means that editfcn's will not
	**		be applied to the message.
	*/

	if (m == &Mailer[0])
	{
		if (*user == '|')
		{
			user++;
			m = &Mailer[1];
		}
		else
		{
			if (index(user, '/') != NULL)
			{
				i = mailfile(user);
				giveresponse(i, TRUE, m);
				return (i);
			}
		}
	}

	/*
	**  See if the user exists.
	**	Strictly, this is only needed to print a pretty
	**	error message.
	**
	**	>>>>>>>>>> This clause assumes that the local mailer
	**	>> NOTE >> cannot do any further aliasing; that
	**	>>>>>>>>>> function is subsumed by delivermail.
	*/

	if (m == &Mailer[0])
	{
		if (getpwnam(user) == NULL)
		{
			giveresponse(EX_NOUSER, TRUE, m);
			return (EX_NOUSER);
		}
	}

	/*
	**  If the mailer wants a From line, insert a new editfcn.
	*/

	if (flagset(M_HDR, m->m_flags) && editfcn == NULL)
		editfcn = putheader;

	/*
	**  Call the mailer.
	**	The argument vector gets built, pipes through 'editfcn'
	**	are created as necessary, and we fork & exec as
	**	appropriate.  In the parent, we call 'editfcn'.
	*/

	pvp = buildargv(m->m_argv, m->m_flags, host, user, From.q_paddr);
	if (pvp == NULL)
	{
		usrerr("name too long");
		return (-1);
	}
	rewind(stdin);

	/* create a pipe if we will need one */
	if (editfcn != NULL && pipe(pvect) < 0)
	{
		syserr("pipe");
		return (-1);
	}
# ifdef VFORK
	pid = vfork();
# else
	pid = fork();
# endif
	if (pid < 0)
	{
		syserr("Cannot fork");
		if (editfcn != NULL)
		{
			close(pvect[0]);
			close(pvect[1]);
		}
		return (-1);
	}
	else if (pid == 0)
	{
		/* child -- set up input & exec mailer */
		/* make diagnostic output be standard output */
		close(2);
		dup(1);
		signal(SIGINT, SIG_IGN);
		if (editfcn != NULL)
		{
			close(0);
			if (dup(pvect[0]) < 0)
			{
				syserr("Cannot dup to zero!");
				_exit(EX_OSERR);
			}
			close(pvect[0]);
			close(pvect[1]);
		}
		if (!flagset(M_RESTR, m->m_flags))
			setuid(getuid());
# ifdef LOG
		initlog(NULL, 0, LOG_CLOSE);
# endif LOG
# ifndef VFORK
		/*
		 * We have to be careful with vfork - we can't mung up the
		 * memory but we don't want the mailer to inherit any extra
		 * open files.  Chances are the mailer won't
		 * care about an extra file, but then again you never know.
		 * Actually, we would like to close(fileno(pwf)), but it's
		 * declared static so we can't.  But if we fclose(pwf), which
		 * is what endpwent does, it closes it in the parent too and
		 * the next getpwnam will be slower.  If you have a weird mailer
		 * that chokes on the extra file you should do the endpwent().
		 */
		endpwent();
# endif
		execv(m->m_mailer, pvp);
		/* syserr fails because log is closed */
		/* syserr("Cannot exec %s", m->m_mailer); */
		_exit(EX_UNAVAILABLE);
	}

	/* arrange to write out header message if error */
	if (editfcn != NULL)
	{
		close(pvect[0]);
		signal(SIGPIPE, pipesig);
		mfile = fdopen(pvect[1], "w");
		(*editfcn)(mfile);
		fclose(mfile);
	}

	/*
	**  Wait for child to die and report status.
	**	We should never get fatal errors (e.g., segmentation
	**	violation), so we report those specially.  For other
	**	errors, we choose a status message (into statmsg),
	**	and if it represents an error, we print it.
	*/

	while ((i = wait(&st)) > 0 && i != pid)
		continue;
	if (i < 0)
	{
		syserr("wait");
		return (-1);
	}
	if ((st & 0377) != 0)
	{
		syserr("%s: stat %o", pvp[0], st);
		ExitStat = EX_UNAVAILABLE;
		return (-1);
	}
	i = (st >> 8) & 0377;
	giveresponse(i, FALSE, m);
	return (i);
}
/*
**  GIVERESPONSE -- Interpret an error response from a mailer
**
**	Parameters:
**		stat -- the status code from the mailer (high byte
**			only; core dumps must have been taken care of
**			already).
**		force -- if set, force an error message output, even
**			if the mailer seems to like to print its own
**			messages.
**		m -- the mailer descriptor for this mailer.
**
**	Returns:
**		none.
**
**	Side Effects:
**		Errors may be incremented.
**		ExitStat may be set.
**
**	Called By:
**		deliver
*/

giveresponse(stat, force, m)
	int stat;
	int force;
	register struct mailer *m;
{
	register char *statmsg;
	extern char *SysExMsg[];
	register int i;
	extern int N_SysEx;
	extern long MsgSize;
	char buf[30];

	i = stat - EX__BASE;
	if (i < 0 || i > N_SysEx)
		statmsg = NULL;
	else
		statmsg = SysExMsg[i];
	if (stat == 0)
		statmsg = "ok";
	else
	{
		Errors++;
		if (statmsg == NULL && m->m_badstat != 0)
		{
			stat = m->m_badstat;
			i = stat - EX__BASE;
# ifdef DEBUG
			if (i < 0 || i >= N_SysEx)
				syserr("Bad m_badstat %d", stat);
			else
# endif DEBUG
			statmsg = SysExMsg[i];
		}
		if (statmsg == NULL)
			usrerr("unknown mailer response %d", stat);
		else if (force || !flagset(M_QUIET, m->m_flags))
			usrerr("%s", statmsg);
	}

	/*
	**  Final cleanup.
	**	Log a record of the transaction.  Compute the new
	**	ExitStat -- if we already had an error, stick with
	**	that.
	*/

	if (statmsg == NULL)
	{
		sprintf(buf, "error %d", stat);
		statmsg = buf;
	}

# ifdef LOG
	logmsg(LOG_INFO, "%s->%s: %ld: %s", From.q_paddr, To, MsgSize, statmsg);
# endif LOG
	setstat(stat);
	return (stat);
}
/*
**  PUTHEADER -- insert the From header into some mail
**
**	For mailers such as 'msgs' that want the header inserted
**	into the mail, this edit filter inserts the From line and
**	then passes the rest of the message through.
**
**	Parameters:
**		fp -- the file pointer for the output.
**
**	Returns:
**		none
**
**	Side Effects:
**		Puts a "From" line in UNIX format, and then
**			outputs the rest of the message.
**
**	Called By:
**		deliver
*/

putheader(fp)
	register FILE *fp;
{
	char buf[MAXLINE + 1];
	long tim;
	extern char *ctime();

	time(&tim);
	fprintf(fp, "From %s %s", From.q_paddr, ctime(&tim));
	while (fgets(buf, sizeof buf, stdin) != NULL && !ferror(fp))
		fputs(buf, fp);
	if (ferror(fp))
	{
		syserr("putheader: write error");
		setstat(EX_IOERR);
	}
}
/*
**  PIPESIG -- Handle broken pipe signals
**
**	This just logs an error.
**
**	Parameters:
**		none
**
**	Returns:
**		none
**
**	Side Effects:
**		logs an error message.
*/

pipesig()
{
	syserr("Broken pipe");
	signal(SIGPIPE, SIG_IGN);
}
/*
**  SENDTO -- Designate a send list.
**
**	The parameter is a comma-separated list of people to send to.
**	This routine arranges to send to all of them.
**
**	Parameters:
**		list -- the send list.
**		copyf -- the copy flag; passed to parse.
**
**	Returns:
**		none
**
**	Side Effects:
**		none.
**
**	Called By:
**		main
**		alias
*/

sendto(list, copyf)
	char *list;
	int copyf;
{
	register char *p;
	register char *q;
	register char c;
	addrq *a;
	extern addrq *parse();
	bool more;

	/* more keeps track of what the previous delimiter was */
	more = TRUE;
	for (p = list; more; )
	{
		/* find the end of this address */
		q = p;
		while ((c = *p++) != '\0' && c != ',' && c != '\n')
			continue;
		more = c != '\0';
		*--p = '\0';
		if (more)
			p++;

		/* parse the address */
		if ((a = parse(q, (addrq *) NULL, copyf)) == NULL)
			continue;

		/* arrange to send to this person */
		recipient(a, &SendQ);
	}
	To = NULL;
}
/*
**  RECIPIENT -- Designate a message recipient
**
**	Saves the named person for future mailing.
**
**	Designates a person as a recipient.  This routine
**	does the initial parsing, and checks to see if
**	this person has already received the mail.
**	It also supresses local network names and turns them into
**	local names.
**
**	Parameters:
**		a -- the (preparsed) address header for the recipient.
**		targetq -- the queue to add the name to.
**
**	Returns:
**		none.
**
**	Side Effects:
**		none.
**
**	Called By:
**		sendto
**		main
*/

recipient(a, targetq)
	register addrq *a;
	addrq *targetq;
{
	register addrq *q;
	register struct mailer *m;
	register char **pvp;
	extern char *xalloc();
	extern bool forward();
	extern int errno;
	extern bool sameaddr();

	To = a->q_paddr;
	m = a->q_mailer;
	errno = 0;
# ifdef DEBUG
	if (Debug)
		printf("recipient(%s)\n", To);
# endif DEBUG

	/*
	**  Look up this person in the recipient list.  If they
	**  are there already, return, otherwise continue.
	*/

	if (!ForceMail)
	{
		for (q = &SendQ; (q = nxtinq(q)) != NULL; )
			if (sameaddr(q, a, FALSE))
			{
# ifdef DEBUG
				if (Debug)
					printf("(%s in SendQ)\n", a->q_paddr);
# endif DEBUG
				return;
			}
		for (q = &AliasQ; (q = nxtinq(q)) != NULL; )
			if (sameaddr(q, a, FALSE))
			{
# ifdef DEBUG
				if (Debug)
					printf("(%s in AliasQ)\n", a->q_paddr);
# endif DEBUG
				return;
			}
	}

	/*
	**  See if the user wants hir mail forwarded.
	**	`Forward' must do the forwarding recursively.
	*/

	if (m == &Mailer[0] && !NoAlias && targetq == &SendQ && forward(a))
		return;

	/*
	**  Put the user onto the target queue.
	*/

	if (targetq != NULL)
	{
		putonq(a, targetq);
	}

	return;
}
/*
**  BUILDARGV -- Build an argument vector for a mail server.
**
**	Using a template defined in config.c, an argv is built.
**	The format of the template is already a vector.  The
**	items of this vector are copied, unless a dollar sign
**	is encountered.  In this case, the next character
**	specifies something else to copy in.  These can be
**		$f	The from address.
**		$h	The host.
**		$u	The user.
**		$c	The hop count.
**	The vector is built in a local buffer.  A pointer to
**	the static argv is returned.
**
**	Parameters:
**		tmplt -- a template for an argument vector.
**		flags -- the flags for this server.
**		host -- the host name to send to.
**		user -- the user name to send to.
**		from -- the person this mail is from.
**
**	Returns:
**		A pointer to an argv.
**
**	Side Effects:
**		none
**
**	WARNING:
**		Since the argv is staticly allocated, any subsequent
**		calls will clobber the old argv.
**
**	Called By:
**		deliver
*/

char **
buildargv(tmplt, flags, host, user, from)
	char **tmplt;
	int flags;
	char *host;
	char *user;
	char *from;
{
	register char *p;
	register char *q;
	static char *pv[MAXPV+1];
	char **pvp;
	char **mvp;
	static char buf[512];
	register char *bp;
	char pbuf[30];

	/*
	**  Do initial argv setup.
	**	Insert the mailer name.  Notice that $x expansion is
	**	NOT done on the mailer name.  Then, if the mailer has
	**	a picky -f flag, we insert it as appropriate.  This
	**	code does not check for 'pv' overflow; this places a
	**	manifest lower limit of 4 for MAXPV.
	*/

	pvp = pv;
	bp = buf;

	*pvp++ = tmplt[0];

	/* insert -f or -r flag as appropriate */
	if (flagset(M_FOPT|M_ROPT, flags) && FromFlag)
	{
		if (flagset(M_FOPT, flags))
			*pvp++ = "-f";
		else
			*pvp++ = "-r";
		*pvp++ = From.q_paddr;
	}

	/*
	**  Build the rest of argv.
	**	For each prototype parameter, the prototype is
	**	scanned character at a time.  If a dollar-sign is
	**	found, 'q' is set to the appropriate expansion,
	**	otherwise it is null.  Then either the string
	**	pointed to by q, or the original character, is
	**	interpolated into the buffer.  Buffer overflow is
	**	checked.
	*/

	for (mvp = tmplt; (p = *++mvp) != NULL; )
	{
		if (pvp >= &pv[MAXPV])
		{
			syserr("Too many parameters to %s", pv[0]);
			return (NULL);
		}
		*pvp++ = bp;
		for (; *p != '\0'; p++)
		{
			/* q will be the interpolated quantity */
			q = NULL;
			if (*p == '$')
			{
				switch (*++p)
				{
				  case 'f':	/* from person */
					q = from;
					break;

				  case 'u':	/* user */
					q = user;
					break;

				  case 'h':	/* host */
					q = host;
					break;

				  case 'c':	/* hop count */
					sprintf(pbuf, "%d", HopCount);
					q = pbuf;
					break;
				}
			}

			/*
			**  Interpolate q or output one character
			**	Strip quote bits as we proceed.....
			*/

			if (q != NULL)
			{
				while (bp < &buf[sizeof buf - 1] && (*bp++ = *q++) != '\0')
					continue;
				bp--;
			}
			else if (bp < &buf[sizeof buf - 1])
				*bp++ = *p;
		}
		*bp++ = '\0';
		if (bp >= &buf[sizeof buf - 1])
			return (NULL);
	}
	*pvp = NULL;

# ifdef DEBUG
	if (Debug)
	{
		printf("Interpolated argv is:\n");
		for (mvp = pv; *mvp != NULL; mvp++)
			printf("\t%s\n", *mvp);
	}
# endif DEBUG

	return (pv);
}
/*
**  MAILFILE -- Send a message to a file.
**
**	Parameters:
**		filename -- the name of the file to send to.
**
**	Returns:
**		The exit code associated with the operation.
**
**	Side Effects:
**		none.
**
**	Called By:
**		deliver
*/

mailfile(filename)
	char *filename;
{
	char buf[MAXLINE];
	register FILE *f;
	auto long tim;
	extern char *ctime();

	f = fopen(filename, "a");
	if (f == NULL)
		return (EX_CANTCREAT);
	
	/* output the timestamp */
	time(&tim);
	fprintf(f, "From %s %s", From.q_paddr, ctime(&tim));
	rewind(stdin);
	while (fgets(buf, sizeof buf, stdin) != NULL)
	{
		fputs(buf, f);
		if (ferror(f))
		{
			fclose(f);
			return (EX_IOERR);
		}
	}
	fputs("\n", f);
	fclose(f);
	return (EX_OK);
}
