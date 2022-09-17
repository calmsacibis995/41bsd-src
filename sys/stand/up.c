/*	up.c	4.1	11/9/80	*/

#define	OLDUCODE

#include "../h/param.h"
#include "../h/inode.h"
#include "../h/pte.h"
#include "../h/uba.h"
#include "saio.h"

#define	EMULEX
#ifdef	EMULEX
#define DELAY(N)	{ register int d; d = N; while (--d > 0); }
#else
#define	DELAY(N)
#endif

struct upregs {
	short	upcs1;	/* Control and Status register 1 */
	short	upwc;	/* Word count register */
	short	upba;	/* UNIBUS address register */
	short	upda;	/* Desired address register */
	short	upcs2;	/* Control and Status register 2*/
	short	upds;	/* Drive Status */
	short	uper1;	/* Error register 1 */
	short	upas;	/* Attention Summary */
	short	upla;	/* Look ahead */
	short	updb;	/* Data buffer */
	short	upmr;	/* Maintenance register */
	short	updt;	/* Drive type */
	short	upsn;	/* Serial number */
	short	upof;	/* Offset register */
	short	upca;	/* Desired Cylinder address register*/
	short	upcc;	/* Current Cylinder */
	short	uper2;	/* Error register 2 */
	short	uper3;	/* Error register 3 */
	short	uppos;	/* Burst error bit position */
	short	uppat;	/* Burst error bit pattern */
	short	upbae;	/* 11/70 bus extension */
};

#ifdef	OLDUCODE
#define	SDELAY	500
#else
#define	SDELAY	25
#endif
int	sdelay = SDELAY;
int	idelay = 500;

#define	UPADDR ((struct upregs *)(PHYSUMEM + 0776700 - UNIBASE))

/* Drive commands, placed in upcs1 */
#define	GO	01		/* Go bit, set in all commands */
#define	PRESET	020		/* Preset drive at init or after errors */
#define	OFFSET	014		/* Offset heads to try to recover error */
#define	RTC	016		/* Return to center-line after OFFSET */
#define	SEARCH	030		/* Search for cylinder+sector */
#define	SEEK	04		/* Seek to cylinder */
#define	RECAL	06		/* Recalibrate, needed after seek error */
#define	DCLR	010		/* Drive clear, after error */
#define	WCOM	060		/* Write */
#define	RCOM	070		/* Read */

/* Other bits of upcs1 */
#define	IE	0100		/* Controller wide interrupt enable */
#define	TRE	040000		/* Transfer error */
#define	RDY	0200		/* Transfer terminated */

/* Drive status bits of upds */
#define	PIP	020000		/* Positioning in progress */
#define	ERR	040000		/* Error has occurred, DCLR necessary */
#define	VV	0100		/* Volume is valid, set by PRESET */
#define	DPR	0400		/* Drive has been preset */
#define	MOL	010000		/* Drive is online, heads loaded, etc */
#define	DRY	0200		/* Drive ready */

/* Bits of upcs2 */
#define	CLR	040		/* Controller clear */
/* Bits of uper1 */
#define	DCK	0100000		/* Ecc error occurred */
#define	ECH	0100		/* Ecc error was unrecoverable */
#define	WLE	04000		/* Attempt to write read-only drive */

/* Bits of upof; the offset bits above are also in this register */
#define	FMT22	010000		/* 16 bits/word, must be always set */

struct devsize {
	daddr_t	cyloff;
} up_sizes[] = {
	0, 27, 68, -1, -1, -1, -1, 82
};

upopen(io)
register struct iob *io;
{

	if (up_sizes[io->i_boff].cyloff == -1 ||
	    io->i_boff < 0 || io->i_boff > 7)
		_stop("up bad unit");
	io->i_boff = up_sizes[io->i_boff].cyloff * 32 * 19;
}

upstrategy(io, func)
register struct iob *io;
{
	int unit, nspc, ns, cn, tn, sn;
	daddr_t bn;
	int info;
	register short *rp;
	int occ = io->i_cc;
	register struct upregs *upaddr = UPADDR;

	unit = io->i_unit;
	bn = io->i_bn;
	nspc = 32 * 19;
	ns = 32;
	cn = bn/nspc;
	sn = bn%nspc;
	tn = sn/ns;
	sn = sn%ns;
	upaddr->upcs2 = unit;
	DELAY(sdelay);
	if ((upaddr->upds & VV) == 0) {
		upaddr->upcs1 = DCLR|GO;
		DELAY(idelay);
		upaddr->upcs1 = PRESET|GO;
		DELAY(idelay);
		upaddr->upof = FMT22;
	}
	if ((upaddr->upds & (DPR|MOL)) != (DPR|MOL)) {
		printf("after fmt22 upds %o\n", upaddr->upds);
		_stop("up !DPR || !MOL");
	}
	info = ubasetup(io, 1);
	rp = (short *) &upaddr->upda;
	upaddr->upca = cn;
	*rp = (tn << 8) + sn;
	*--rp = info;
	*--rp = -io->i_cc / sizeof (short);
	if (func == READ)
		*--rp = GO|RCOM;
	else
		*--rp = GO|WCOM;
	DELAY(sdelay);
	do {
		DELAY(25);
	} while ((upaddr->upcs1 & RDY) == 0);
	DELAY(200);
	if (upaddr->upcs1&ERR) {
		printf("disk error: cyl=%d track=%d sect=%d cs1=%X, er1=%X\n",
		    cn, tn, sn,
		    upaddr->upcs1, upaddr->uper1);
		return (-1);
	}
	if (io->i_cc != occ)
		printf("returned %d\n", io->i_cc);
	ubafree(info);
	return (io->i_cc);
}
