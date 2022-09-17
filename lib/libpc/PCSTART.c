/* Copyright (c) 1979 Regents of the University of California */

static char sccsid[] = "@(#)PCSTART.c 1.1 10/29/80";

#include "h00vars.h"

/*
 * program variables
 */
struct display	_disply[MAXLVL];
int		_argc;
char		**_argv;
long		_stlim = 500000;
long		_stcnt = 0;
char		*_minptr = (char *)0x7fffffff;
char		*_maxptr = (char *)0;

/*
 * file record variables
 */
long		_filefre = PREDEF;
struct iorechd	_fchain = {
	0, 0, 0, 0,		/* only use fchain field */
	INPUT			/* fchain  */
};
struct iorec	*_actfile[MAXFILES] = {
	INPUT,
	OUTPUT,
	ERR
};

/*
 * standard files
 */
char		_inwin, _outwin, _errwin;
struct iorechd	input = {
	&_inwin,		/* fileptr */
	0,			/* lcount  */
	0x7fffffff,		/* llimit  */
	&_iob[0],		/* fbuf    */
	OUTPUT,			/* fchain  */
	STDLVL,			/* flev    */
	"standard input",	/* pfname  */
	FTEXT | FREAD | SYNC,	/* funit   */
	0,			/* fblk    */
	1			/* fsize   */
};
struct iorechd	output = {
	&_outwin,		/* fileptr */
	0,			/* lcount  */
	0x7fffffff,		/* llimit  */
	&_iob[1],		/* fbuf    */
	ERR,			/* fchain  */
	STDLVL,			/* flev    */
	"standard output",	/* pfname  */
	FTEXT | FWRITE | EOFF,	/* funit   */
	1,			/* fblk    */
	1			/* fsize   */
};
struct iorechd	_err = {
	&_errwin,		/* fileptr */
	0,			/* lcount  */
	0x7fffffff,		/* llimit  */
	&_iob[2],		/* fbuf    */
	FILNIL,			/* fchain  */
	STDLVL,			/* flev    */
	"Message file",		/* pfname  */
	FTEXT | FWRITE | EOFF,	/* funit   */
	2,			/* fblk    */
	1			/* fsize   */
};

PCSTART()
{
	/*
	 * necessary only on systems which do not initialize
	 * memory to zero
	 */

	struct iorec	**ip;

	for (ip = &_actfile[3]; ip < &_actfile[MAXFILES]; *ip++ = FILNIL);
}
