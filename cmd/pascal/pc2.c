#include <stdio.h>
#include <ctype.h>
/*
 * The hash table must be at least twice as big as the number
 * of patterns, preferably bigger. It must also be a prime number
 */
#define HSHSIZ	101

struct pats {
	char	*name;
	char	*replace;
} ptab[] = {

	{ "ACTFILE\n",
"	movl	(sp)+,r1\n\
	movl	12(r1),r0\n" },

	{ "fgetc\n",
"	decl	*(sp)\n\
	jgeq	1f\n\
	calls	$1,__filbuf\n\
	jbr     2f\n\
1:\n\
	addl3	$4,(sp)+,r1\n\
	movzbl	*(r1),r0\n\
	incl	(r1)\n\
2:\n" },

	{ "fputc\n",
"	decl	*4(sp)\n\
	jgeq	1f\n\
	calls	$2,__flsbuf\n\
	jbr	2f\n\
1:\n\
	movl	(sp)+,r0\n\
	addl3	$4,(sp)+,r1\n\
	movb	r0,*(r1)\n\
	incl	(r1)\n\
2:\n" },

	{ "MOVC3\n",
"	movl	(sp)+,r5\n\
	movc3	r5,*(sp)+,*(sp)+\n" },

	{ "LOCC\n",
"	movl	(sp)+,r5\n\
	movl	(sp)+,r4\n\
	locc	r5,r4,*(sp)+\n" },

	{ "ROUND\n",
"	cvtrdl	(sp)+,r0\n" },

	{ "TRUNC\n",
"	cvtdl	(sp)+,r0\n" },

	{ "FCALL\n",
"	movl	(sp),r0\n\
	ashl	$3,4(r0),r1\n\
	movc3	r1,__disply+8,8(r0)[r1]\n\
	movl	(sp),r0\n\
	ashl	$3,4(r0),r1\n\
	movc3	r1,8(r0),__disply+8\n\
	movl	*(sp)+,r0\n" },

	{ "FRTN\n",
"	movl	(sp)+,r0\n\
	ashl	$3,4(r0),r1\n\
	movc3	r1,8(r0)[r1],__disply+8\n\
	movl	(sp)+,r0\n" },

	{ "FSAV\n",
"	movl	8(sp),r0\n\
	movl	(sp)+,(r0)\n\
	movl	(sp)+,4(r0)\n\
	ashl	$3,4(r0),r1\n\
	movc3	r1,__disply+8,8(r0)\n\
	movl	(sp)+,r0\n" },

	{ "RELEQ\n",
"	movl	(sp)+,r1\n\
	cmpc3	r1,*(sp)+,*(sp)+\n\
	beql	1f\n\
	clrl	r0\n\
	brb	2f\n\
1:\n\
	movl	$1,r0\n\
2:\n" },

	{ "RELNE\n",
"	movl	(sp)+,r1\n\
	cmpc3	r1,*(sp)+,*(sp)+\n\
	bneq	1f\n\
	clrl	r0\n\
	brb	2f\n\
1:\n\
	movl	$1,r0\n\
2:\n" },

	{ "RELSLT\n",
"	movl	(sp)+,r1\n\
	cmpc3	r1,*(sp)+,*(sp)+\n\
	blss	1f\n\
	clrl	r0\n\
	brb	2f\n\
1:\n\
	movl	$1,r0\n\
2:\n" },

	{ "RELSLE\n",
"	movl	(sp)+,r1\n\
	cmpc3	r1,*(sp)+,*(sp)+\n\
	bleq	1f\n\
	clrl	r0\n\
	brb	2f\n\
1:\n\
	movl	$1,r0\n\
2:\n" },

	{ "RELSGT\n",
"	movl	(sp)+,r1\n\
	cmpc3	r1,*(sp)+,*(sp)+\n\
	bgtr	1f\n\
	clrl	r0\n\
	brb	2f\n\
1:\n\
	movl	$1,r0\n\
2:\n" },

	{ "RELSGE\n",
"	movl	(sp)+,r1\n\
	cmpc3	r1,*(sp)+,*(sp)+\n\
	bgeq	1f\n\
	clrl	r0\n\
	brb	2f\n\
1:\n\
	movl	$1,r0\n\
2:\n" },

	{ "ADDT\n",
"	movl	(sp)+,r0\n\
	movl	(sp)+,r1\n\
	movl	(sp)+,r2\n\
	movl	r0,r3\n\
	movl	(sp)+,r4\n\
1:\n\
	bisl3	(r1)+,(r2)+,(r3)+\n\
	sobgtr	r4,1b\n" },

	{ "SUBT\n",
"	movl	(sp)+,r0\n\
	movl	(sp)+,r1\n\
	movl	(sp)+,r2\n\
	movl	r0,r3\n\
	movl	(sp)+,r4\n\
1:\n\
	bicl3	(r2)+,(r1)+,(r3)+\n\
	sobgtr	r4,1b\n" },

	{ "MULT\n",
"	movl	(sp)+,r0\n\
	movl	(sp)+,r1\n\
	movl	(sp)+,r2\n\
	movl	r0,r3\n\
	movl	(sp)+,r4\n\
1:\n\
	mcoml	(r1)+,r5\n\
	bicl3	r5,(r2)+,(r3)+\n\
	sobgtr	r4,1b\n" },

	{ "IN\n",
"	clrl	r0\n\
	movl	(sp)+,r1\n\
	subl2	(sp)+,r1\n\
	cmpl	r1,(sp)+\n\
	bgtru	1f\n\
	bbc	r1,*(sp)+,1f\n\
	movl	$1,r0\n\
1:\n" }
};

struct pats		*htbl[HSHSIZ];


#define	CHK(c)	if (*cp++ != c) goto copy;

#define HASH(cp, hp) {\
	hash = 0; rehash = 1; ccp = cp; \
	do	{ \
		hash *= (int)*ccp++; \
	} while (*ccp && *ccp != '\n'); \
	hash >>= 7; hash %= HSHSIZ; hp = &htbl[hash]; size = ccp - cp + 1; \
	}

#define REHASH(hp) {\
	hp += rehash; rehash += 2; \
	if (hp >= &htbl[HSHSIZ]) \
		hp -= HSHSIZ; \
	}


main(argc, argv)

	int	argc;
	char	**argv;
{
	register struct pats	*pp;
	register struct pats	**hp;
	register char		*cp, *ccp;
	register int		hash, rehash, size;
	char			line[BUFSIZ];

	if (argc > 1)
		freopen(argv[1], "r", stdin);
	if (argc > 2)
		freopen(argv[2], "w", stdout);
	/*
	 * set up the hash table
	 */
	for(pp = ptab; pp < &ptab[sizeof ptab/sizeof ptab[0]]; pp++) {
		HASH(pp->name, hp);
		while (*hp)
			REHASH(hp);
		*hp = pp;
	}
	/*
	 * check each line and replace as appropriate
	 */
	while (fgets(line, BUFSIZ, stdin)) {
		for (cp = line; *cp && *cp == '\t'; )
			cp++;
		CHK('c'); CHK('a'); CHK('l'); CHK('l'); CHK('s'); CHK('\t');
		CHK('$'); if (!isdigit(*cp++)) goto copy; CHK(','); CHK('_');
		HASH(cp, hp);
		while (*hp) {
			if (RELEQ(size, (*hp)->name, cp)) {
				fputs((*hp)->replace, stdout);
				goto nextline;
			}
			REHASH(hp);
		}
copy:
		fputs(line, stdout);
nextline:;
	}
	exit(0);
}
