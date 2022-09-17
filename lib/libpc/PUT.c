/* Copyright (c) 1979 Regents of the University of California */

static char sccsid[] = "@(#)PUT.c 1.1 10/29/80";

#include "h00vars.h"
#include "h01errs.h"

PUT(curfile)

	register struct iorec	*curfile;
{
	if (curfile->funit & FREAD) {
		ERROR(EWRITEIT, curfile->pfname);
		return;
	}
	fwrite(curfile->fileptr, curfile->fsize, 1, curfile->fbuf);
	if (ferror(curfile->fbuf)) {
		ERROR(EWRITE, curfile->pfname);
		return;
	}
}
