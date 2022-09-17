/*	vp.c	4.1	11/9/80	*/

#include "../conf/vp.h"
#if NVP > 0
/*
 * Versatec matrix printer/plotter
 * dma interface driver
 */
#include "../h/param.h"
#include "../h/dir.h"
#include "../h/user.h"
#include "../h/buf.h"
#include "../h/systm.h"
#include "../h/map.h"
#include "../h/pte.h"
#include "../h/uba.h"

int	vpbdp = 1;

unsigned minvpph();

#define	VPPRI	(PZERO-1)

struct	vpregs {
	short	plbcr;
	short	fill;
	short	prbcr;
	unsigned short pbaddr;
	short	plcsr;
	short	plbuf;
	short	prcsr;
	unsigned short prbuf;
};

#define	ERROR	0100000
#define	DTCINTR	040000
#define	DMAACT	020000
#define	READY	0200
#define	IENABLE	0100
#define	TERMCOM	040
#define	FFCOM	020
#define	EOTCOM	010
#define	CLRCOM	04
#define	RESET	02
#define	SPP	01

struct {
	int	vp_state;
	int	vp_count;
	int	vp_bufp;
	struct	buf *vp_bp;
} vp11;
int	vp_ubinfo;

struct	buf rvpbuf;

#define	VISOPEN	01
#define	CMNDS	076
#define	MODE	0700
#define	PRINT	0100
#define	PLOT	0200
#define	PPLOT	0400
#define	VBUSY	01000

vpopen()
{

	if (vp11.vp_state & VISOPEN) {
		u.u_error = ENXIO;
		return;
	}
	vp11.vp_state = VISOPEN | PRINT | CLRCOM | RESET;
	vp11.vp_count = 0;
	VPADDR->prcsr = IENABLE | DTCINTR;
	vptimo();
	while (vp11.vp_state & CMNDS) {
		(void) spl4();
		if (vperror(READY)) {
			vpclose();
			u.u_error = EIO;
			return;
		}
		vpstart();
		(void) spl0();
	}
}

vpstrategy(bp)
	register struct buf *bp;
{
	register int e;

	(void) spl4();
	while (vp11.vp_state & VBUSY)
		sleep((caddr_t)&vp11, VPPRI);
	vp11.vp_state |= VBUSY;
	vp11.vp_bp = bp;
	vp_ubinfo = ubasetup(bp, vpbdp);
	vp11.vp_bufp = vp_ubinfo & 0x3ffff;
	if (e = vperror(READY))
		goto brkout;
	vp11.vp_count = bp->b_bcount;
	vpstart();
	while ((vp11.vp_state&PLOT ? VPADDR->plcsr : VPADDR->prcsr) & DMAACT)
		sleep((caddr_t)&vp11, VPPRI);
	vp11.vp_count = 0;
	vp11.vp_bufp = 0;
	if ((vp11.vp_state&MODE) == PPLOT)
		vp11.vp_state = (vp11.vp_state &~ MODE) | PLOT;
	(void) spl0();
brkout:
	ubafree(vp_ubinfo), vp_ubinfo = 0;
	vp11.vp_state &= ~VBUSY;
	vp11.vp_bp = 0;
	iodone(bp);
	if (e)
		u.u_error = EIO;
	wakeup((caddr_t)&vp11);
}

int	vpblock = 16384;

unsigned
minvpph(bp)
struct buf *bp;
{

	if (bp->b_bcount > vpblock)
		bp->b_bcount = vpblock;
}

/*ARGSUSED*/
vpwrite(dev)
{

	physio(vpstrategy, &rvpbuf, dev, B_WRITE, minvpph);
}

vperror(bit)
{
	register int state, e;

	state = vp11.vp_state & PLOT;
	while ((e = (state ? VPADDR->plcsr : VPADDR->prcsr) & (bit|ERROR)) == 0)
		sleep((caddr_t)&vp11, VPPRI);
	return (e & ERROR);
}

vpstart()
{
	register short bit;

	if (vp11.vp_count) {
		VPADDR->pbaddr = vp11.vp_bufp;
		if (vp11.vp_state & (PRINT|PPLOT))
			VPADDR->prbcr = vp11.vp_count;
		else
			VPADDR->plbcr = vp11.vp_count;
		return;
	}
	for (bit = 1; bit != 0; bit <<= 1)
		if (vp11.vp_state&bit&CMNDS) {
			VPADDR->plcsr |= bit;
			vp11.vp_state &= ~bit;
			return;
		}
}

/*ARGSUSED*/
vpioctl(dev, cmd, addr, flag)
	register caddr_t addr;
{
	register int m;

	switch (cmd) {

	case ('v'<<8)+0:
		(void) suword(addr, vp11.vp_state);
		return;

	case ('v'<<8)+1:
		m = fuword(addr);
		if (m == -1) {
			u.u_error = EFAULT;
			return;
		}
		vp11.vp_state = (vp11.vp_state & ~MODE) | (m&(MODE|CMNDS));
		break;

	default:
		u.u_error = ENOTTY;
		return;
	}
	(void) spl4();
	(void) vperror(READY);
	if (vp11.vp_state&PPLOT)
		VPADDR->plcsr |= SPP;
	else
		VPADDR->plcsr &= ~SPP;
	vp11.vp_count = 0;
	while (CMNDS & vp11.vp_state) {
		(void) vperror(READY);
		vpstart();
	}
	(void) spl0();
}

vptimo()
{

	if (vp11.vp_state&VISOPEN)
		timeout(vptimo, (caddr_t)0, HZ/10);
	vpintr(0);
}

/*ARGSUSED*/
vpintr(dev)
{

	wakeup((caddr_t)&vp11);
}

vpclose()
{

	vp11.vp_state = 0;
	vp11.vp_count = 0;
	vp11.vp_bufp = 0;
	VPADDR->plcsr = 0;
}

vpreset()
{

	if ((vp11.vp_state & VISOPEN) == 0)
		return;
	printf(" vp");
	VPADDR->prcsr = IENABLE | DTCINTR;
	if ((vp11.vp_state & VBUSY) == 0)
		return;
	if (vp_ubinfo) {
		printf("<%d>", (vp_ubinfo>>28)&0xf);
		ubafree(vp_ubinfo), vp_ubinfo = 0;
	}
	vp11.vp_bufp = vp_ubinfo & 0x3ffff;
	vp11.vp_count = vp11.vp_bp->b_bcount;
	vpstart();
}
#endif
