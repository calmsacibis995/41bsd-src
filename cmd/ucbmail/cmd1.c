#
#include "rcv.h"
#include <sys/stat.h>

/*
 * Mail -- a mail program
 *
 * User commands.
 */

/*
 * Print the current active headings.
 */

static int screen;

headers(msgvec)
	int *msgvec;
{
	register int n, mesg, flag;
	register struct message *mp;

	n = msgvec[0];
	if (n != 0)
		screen = (n-1)/SCREEN;
	if (screen < 0)
		screen = 0;
	mp = &message[screen * SCREEN];
	if (mp >= &message[msgCount])
		mp = &message[msgCount - SCREEN];
	if (mp < &message[0])
		mp = &message[0];
	flag = 0;
	mesg = mp - &message[0];
	dot = mp;
	for (; mp < &message[msgCount]; mp++) {
		mesg++;
		if (mp->m_flag & MDELETED)
			continue;
		if (flag++ >= SCREEN)
			break;
		printhead(mesg);
		sreset();
	}
	if (flag == 0) {
		printf("No more mail.\n");
		return(1);
	}
	return(0);
}

/*
 * Scroll to the next/previous screen
 */

scroll(arg)
	char arg[];
{
	register int s;
	int cur[1];

	cur[0] = 0;
	s = screen;
	switch (*arg) {
	case 0:
	case '+':
		s++;
		if (s*SCREEN > msgCount) {
			printf("On last screenful of messages\n");
			return(0);
		}
		screen = s;
		break;

	case '-':
		if (--s < 0) {
			printf("On first screenful of messages\n");
			return(0);
		}
		screen = s;
		break;

	default:
		printf("Unrecognized scrolling command \"%s\"\n", arg);
		return(1);
	}
	return(headers(cur));
}


/*
 * Print out the headlines for each message
 * in the passed message list.
 */

from(msgvec)
	int *msgvec;
{
	register int *ip;

	for (ip = msgvec; *ip != NULL; ip++) {
		printhead(*ip);
		sreset();
	}
	if (--ip >= msgvec)
		dot = &message[*ip - 1];
	return(0);
}

/*
 * Print out the header of a specific message.
 * This is a slight improvement to the standard one.
 */

printhead(mesg)
{
	struct message *mp;
	FILE *ibuf;
	char headline[LINESIZE], wcount[10], *subjline, dispc;
	char pbuf[BUFSIZ];
	int s;
	struct headline hl;
	register char *cp;

	mp = &message[mesg-1];
	ibuf = setinput(mp);
	readline(ibuf, headline);
	subjline = hfield("subject", mp);
	if (subjline == NOSTR)
		subjline = hfield("subj", mp);

	/*
	 * Bletch!
	 */

	if (subjline != NOSTR && strlen(subjline) > 28)
		subjline[29] = '\0';
	dispc = ' ';
	if (mp->m_flag & MSAVED)
		dispc = '*';
	if (mp->m_flag & MPRESERVE)
		dispc = 'P';
	parse(headline, &hl, pbuf);
	sprintf(wcount, " %d/%d", mp->m_lines, mp->m_size);
	s = strlen(wcount);
	cp = wcount + s;
	while (s < 7)
		s++, *cp++ = ' ';
	*cp = '\0';
	if (subjline != NOSTR)
		printf("%c%3d %-8s %16.16s %s \"%s\"\n", dispc, mesg,
		    nameof(mp), hl.l_date, wcount, subjline);
	else
		printf("%c%3d %-8s %16.16s %s\n", dispc, mesg,
		    nameof(mp), hl.l_date, wcount);
}

/*
 * Print out the value of dot.
 */

pdot()
{
	printf("%d\n", dot - &message[0] + 1);
	return(0);
}

/*
 * Print out all the possible commands.
 */

pcmdlist()
{
	register struct cmd *cp;
	register int cc;
	extern struct cmd cmdtab[];

	printf("Commands are:\n");
	for (cc = 0, cp = cmdtab; cp->c_name != NULL; cp++) {
		cc += strlen(cp->c_name) + 2;
		if (cc > 72) {
			printf("\n");
			cc = strlen(cp->c_name) + 2;
		}
		if ((cp+1)->c_name != NOSTR)
			printf("%s, ", cp->c_name);
		else
			printf("%s\n", cp->c_name);
	}
	return(0);
}

/*
 * Type out the messages requested.
 */

type(msgvec)
	int *msgvec;
{
	register *ip;
	register struct message *mp;
	register int mesg;
	int c;
	FILE *ibuf;

	for (ip = msgvec; *ip && ip-msgvec < msgCount; ip++) {
		mesg = *ip;
		touch(mesg);
		mp = &message[mesg-1];
		dot = mp;
		print(mp);
	}
	return(0);
}

/*
 * Print the indicated message on standard output.
 */

print(mp)
	register struct message *mp;
{

	if (value("quiet") == NOSTR)
		printf("Message %2d:\n", mp - &message[0] + 1);
	touch(mp - &message[0] + 1);
	send(mp, stdout);
}

/*
 * Print the top so many lines of each desired message.
 * The number of lines is taken from the variable "toplines"
 * and defaults to 5.
 */

top(msgvec)
	int *msgvec;
{
	register int *ip;
	register struct message *mp;
	register int mesg;
	int c, topl, lines, lineb;
	char *valtop, linebuf[LINESIZE];
	FILE *ibuf;

	topl = 5;
	valtop = value("toplines");
	if (valtop != NOSTR) {
		topl = atoi(valtop);
		if (topl < 0 || topl > 10000)
			topl = 5;
	}
	lineb = 1;
	for (ip = msgvec; *ip && ip-msgvec < msgCount; ip++) {
		mesg = *ip;
		touch(mesg);
		mp = &message[mesg-1];
		dot = mp;
		if (value("quiet") == NOSTR)
			printf("Message %2d:\n", mesg);
		ibuf = setinput(mp);
		c = mp->m_lines;
		if (!lineb)
			printf("\n");
		for (lines = 0; lines < c && lines <= topl; lines++) {
			if (readline(ibuf, linebuf) <= 0)
				break;
			puts(linebuf);
			lineb = blankline(linebuf);
		}
	}
	return(0);
}

/*
 * Touch all the given messages so that they will
 * get mboxed.
 */

stouch(msgvec)
	int msgvec[];
{
	register int *ip;

	for (ip = msgvec; *ip != 0; ip++) {
		touch(*ip);
		dot = &message[*ip-1];
		dot->m_flag &= ~MPRESERVE;
	}
	return(0);
}
