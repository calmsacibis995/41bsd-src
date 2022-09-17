static	char *Sccsid = "@(#)colrm.c	4.2 (Berkeley) 10/9/80";
#include <stdio.h>
/*
COLRM removes unwanted columns from a file
	Jeff Schriebman  UC Berkeley 11-74
*/


main(argc,argv)
char **argv;
{
	int first;
	register ct,last;
	register char c;
	char buffer[BUFSIZ];

	setbuf(stdout, buffer);
	first = 20000;
	last  = -1;
	if (argc>1) {
		first = getn(*++argv);
		last = 20000;
	}
	if (argc>2)
		last = getn(*++argv);

start:
	ct = 0;
loop1:
	if ((c=getc(stdin))<0)
		goto fin;
	if (c == '\t')
		ct = (ct + 8) &~ 7;
	else if (c == '\b')
		ct = ct ? ct - 1 : 0;
	else
		ct++;
	if (c=='\n') {
		putc(c,stdout);
		goto start;
	}
	if (ct<first) {
		putc(c,stdout);
		goto loop1;
	}

/* Loop getting rid of characters */
	for (;ct<last;ct++) {
		if ((c=getc(stdin))<0)
			goto fin;
		if (c=='\n') {
			putc(c,stdout);
			goto start;
		}
	}

/* Output last of the line */
	while ((c=getc(stdin))>0) {
		putc(c,stdout);
		if (c=='\n')
			goto start;
	}
fin:
	fflush(stdout);
}

getn(ap)
char *ap;
{
	register int n,c;
	register char *p;

	p = ap;
	n = 0;
	while ((c = *p++) >= '0' && c <= '9')
		n = n*10 + c - '0';
	return(n);
}
