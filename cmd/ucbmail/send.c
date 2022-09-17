#

#include "rcv.h"
#ifdef VMUNIX
#include <wait.h>
#endif

/*
 * Mail -- a mail program
 *
 * Mail to others.
 */

/*
 * Send message described by the passed pointer to the
 * passed output buffer.  Return -1 on error, but normally
 * the number of lines written.
 */

send(mailp, obuf)
	struct message *mailp;
	FILE *obuf;
{
	register struct message *mp;
	register int t;
	unsigned int c;
	FILE *ibuf;
	int lc;

	mp = mailp;
	ibuf = setinput(mp);
	c = msize(mp);
	lc = 0;
	while (c-- > 0) {
		putc(t = getc(ibuf), obuf);
		if (t == '\n')
			lc++;
		if (ferror(obuf))
			return(-1);
	}
	return(lc);
}

/*
 * Interface between the argument list and the mail1 routine
 * which does all the dirty work.
 */

mail(people)
	char **people;
{
	register char *cp2;
	register int s;
	char *buf, **ap;
	struct header head;

	for (s = 0, ap = people; *ap != (char *) -1; ap++)
		s += strlen(*ap) + 1;
	buf = salloc(s+1);
	cp2 = buf;
	for (ap = people; *ap != (char *) -1; ap++) {
		cp2 = copy(*ap, cp2);
		*cp2++ = ' ';
	}
	if (cp2 != buf)
		cp2--;
	*cp2 = '\0';
	head.h_to = buf;
	head.h_subject = NOSTR;
	head.h_cc = NOSTR;
	head.h_bcc = NOSTR;
	head.h_seq = 0;
	mail1(&head);
	return(0);
}


/*
 * Send mail to a bunch of user names.  The interface is through
 * the mail routine below.
 */

sendmail(str)
	char *str;
{
	register char **ap;
	char *bufp;
	register int t;
	struct header head;

	if (blankline(str))
		head.h_to = NOSTR;
	else
		head.h_to = str;
	head.h_subject = NOSTR;
	head.h_cc = NOSTR;
	head.h_bcc = NOSTR;
	head.h_seq = 0;
	mail1(&head);
	return(0);
}

/*
 * Mail a message on standard input to the people indicated
 * in the passed header.  (Internal interface).
 */

mail1(hp)
	struct header *hp;
{
	register char *cp;
	int pid, i, s, p, gotcha;
	char **namelist;
	struct name *to, *np;
	FILE *mtf, *postage;
	int remote = rflag != NOSTR || rmail;
	char **t;

	/*
	 * Collect user's mail from standard input.
	 * Get the result as mtf.
	 */

	pid = -1;
	if ((mtf = collect(hp)) == NULL)
		return(-1);
	hp->h_seq = 1;
	if (hp->h_subject == NOSTR)
		hp->h_subject = sflag;
	if (fsize(mtf) == 0 && hp->h_subject == NOSTR) {
		printf("No message !?!\n");
		goto out;
	}
	if (intty && value("askcc") != NOSTR)
		grabh(hp, GCC);
	else if (intty) {
		printf("EOT\n");
		flush();
	}

	/*
	 * Now, take the user names from the combined
	 * to and cc lists and do all the alias
	 * processing.
	 */

	senderr = 0;
	to = usermap(cat(extract(hp->h_bcc, GBCC),
	    cat(extract(hp->h_to, GTO), extract(hp->h_cc, GCC))));
	if (to == NIL) {
		printf("No recipients specified\n");
		goto topdog;
	}

	/*
	 * Look through the recipient list for names with /'s
	 * in them which we write to as files directly.
	 */

	to = outof(to, mtf, hp);
	rewind(mtf);
	to = verify(to);
	if (senderr && !remote) {
topdog:

		if (fsize(mtf) != 0) {
			remove(deadletter);
			exwrite(deadletter, mtf, 1);
			rewind(mtf);
		}
	}
	for (gotcha = 0, np = to; np != NIL; np = np->n_flink)
		if ((np->n_type & GDEL) == 0) {
			gotcha++;
			break;
		}
	if (!gotcha)
		goto out;
	to = elide(to);
	mechk(to);
	if (count(to) > 1)
		hp->h_seq++;
	if (hp->h_seq > 0 && !remote) {
		fixhead(hp, to);
		if (fsize(mtf) == 0)
			printf("Null message body; hope that's ok\n");
		if ((mtf = infix(hp, mtf)) == NULL) {
			fprintf(stderr, ". . . message lost, sorry.\n");
			return(-1);
		}
	}
	namelist = unpack(to);
	if (debug) {
		printf("Recipients of message:\n");
		for (t = namelist; *t != NOSTR; t++)
			printf(" \"%s\"", *t);
		printf("\n");
		fflush(stdout);
		return;
	}
	if ((cp = value("record")) != NOSTR)
		savemail(expand(cp), hp, mtf);

	/*
	 * Wait, to absorb a potential zombie, then
	 * fork, set up the temporary mail file as standard
	 * input for "mail" and exec with the user list we generated
	 * far above. Return the process id to caller in case he
	 * wants to await the completion of mail.
	 */

#ifdef VMUNIX
	while (wait3(&s, WNOHANG, 0) > 0)
		;
#else
	wait(&s);
#endif
	rewind(mtf);
	pid = fork();
	if (pid == -1) {
		perror("fork");
		remove(deadletter);
		exwrite(deadletter, mtf, 1);
		goto out;
	}
	if (pid == 0) {
#ifdef SIGTSTP
		if (remote == 0) {
			signal(SIGTSTP, SIG_IGN);
			signal(SIGTTIN, SIG_IGN);
			signal(SIGTTOU, SIG_IGN);
		}
#endif
		for (i = SIGHUP; i <= SIGQUIT; i++)
			signal(i, SIG_IGN);
		if ((postage = fopen("/crp/kurt/postage", "a")) != NULL) {
			fprintf(postage, "%s %d %d\n", myname,
			    count(to), fsize(mtf));
			fclose(postage);
		}
		s = fileno(mtf);
		for (i = 3; i < 15; i++)
			if (i != s)
				close(i);
		close(0);
		dup(s);
		close(s);
#ifdef CC
		submit(getpid());
#endif CC
#ifdef DELIVERMAIL
		execv(DELIVERMAIL, namelist);
#endif DELIVERMAIL
		execv(MAIL, namelist);
		perror(MAIL);
		exit(1);
	}

out:
	if (remote) {
		while ((p = wait(&s)) != pid && p != -1)
			;
		if (s != 0)
			senderr++;
		pid = 0;
	}
	fclose(mtf);
	return(pid);
}

/*
 * Fix the header by glopping all of the expanded names from
 * the distribution list into the appropriate fields.
 * If there are any ARPA net recipients in the message,
 * we must insert commas, alas.
 */

fixhead(hp, tolist)
	struct header *hp;
	struct name *tolist;
{
	register struct name *nlist;
	register int f;
	register struct name *np;

	for (f = 0, np = tolist; np != NIL; np = np->n_flink)
		if (any('@', np->n_name)) {
			f |= GCOMMA;
			break;
		}

	if (debug && f & GCOMMA)
		fprintf(stderr, "Should be inserting commas in recip lists\n");
	hp->h_to = detract(tolist, GTO|f);
	hp->h_cc = detract(tolist, GCC|f);
}

/*
 * Prepend a header in front of the collected stuff
 * and return the new file.
 */

FILE *
infix(hp, fi)
	struct header *hp;
	FILE *fi;
{
	extern char tempMail[];
	register FILE *nfo, *nfi;
	register int c;

	if ((nfo = fopen(tempMail, "w")) == NULL) {
		perror(tempMail);
		return(fi);
	}
	if ((nfi = fopen(tempMail, "r")) == NULL) {
		perror(tempMail);
		fclose(nfo);
		return(fi);
	}
	remove(tempMail);
	puthead(hp, nfo, GTO|GSUBJECT|GCC|GNL);
	rewind(fi);
	c = getc(fi);
	while (c != EOF) {
		putc(c, nfo);
		c = getc(fi);
	}
	if (ferror(fi)) {
		perror("read");
		fprintf(stderr, "Please notify Kurt Shoens\n");
		return(fi);
	}
	fflush(nfo);
	if (ferror(nfo)) {
		perror(tempMail);
		fclose(nfo);
		fclose(nfi);
		return(fi);
	}
	fclose(nfo);
	fclose(fi);
	rewind(nfi);
	return(nfi);
}

/*
 * Dump the to, subject, cc header on the
 * passed file buffer.
 */

puthead(hp, fo, w)
	struct header *hp;
	FILE *fo;
{
	register int gotcha;

	gotcha = 0;
	if (hp->h_to != NOSTR && w & GTO)
		fprintf(fo, "To: "), fmt(hp->h_to, fo), gotcha++;
	if (hp->h_subject != NOSTR && w & GSUBJECT)
		fprintf(fo, "Subject: %s\n", hp->h_subject), gotcha++;
	if (hp->h_cc != NOSTR && w & GCC)
		fprintf(fo, "Cc: "), fmt(hp->h_cc, fo), gotcha++;
	if (hp->h_bcc != NOSTR && w & GBCC)
		fprintf(fo, "Bcc: "), fmt(hp->h_bcc, fo), gotcha++;
	if (gotcha && w & GNL)
		putc('\n', fo);
	return(0);
}

/*
 * Format the given text to not exceed 72 characters.
 */

fmt(str, fo)
	register char *str;
	register FILE *fo;
{
	register int col;
	register char *cp;

	cp = str;
	col = 0;
	while (*cp) {
		if (*cp == ' ' && col > 65) {
			fprintf(fo, "\n    ");
			col = 4;
			cp++;
			continue;
		}
		putc(*cp++, fo);
		col++;
	}
	putc('\n', fo);
}

/*
 * Save the outgoing mail on the passed file.
 */

savemail(name, hp, fi)
	char name[];
	struct header *hp;
	FILE *fi;
{
	register FILE *fo;
	register int c;
	long now;
	char *n;

	if ((fo = fopen(name, "a")) == NULL) {
		perror(name);
		return(-1);
	}
	time(&now);
	n = rflag;
	if (n == NOSTR)
		n = myname;
	fprintf(fo, "From %s %s", n, ctime(&now));
	rewind(fi);
	for (c = getc(fi); c != EOF; c = getc(fi))
		putc(c, fo);
	fprintf(fo, "\n");
	fflush(fo);
	if (ferror(fo))
		perror(name);
	fclose(fo);
	return(0);
}
