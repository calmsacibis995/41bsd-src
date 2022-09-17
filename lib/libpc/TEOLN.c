/* Copyright (c) 1979 Regents of the University of California */

static char sccsid[] = "@(#)TEOLN.c 1.1 10/29/80";

#include "h00vars.h"
#include "h01errs.h"

TEOLN(filep)

	register struct iorec	*filep;
{
	if (filep->fblk >= MAXFILES || _actfile[filep->fblk] != filep) {
		ERROR(ENOFILE, 0);
		return;
	}
	IOSYNC(filep);
	if (filep->funit & EOLN)
		return TRUE;
	return FALSE;
}
