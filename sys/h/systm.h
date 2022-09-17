/*	systm.h	4.1	11/9/80	*/

/*
 * Random set of variables
 * used by more than one
 * routine.
 */
char	canonb[CANBSIZ];	/* buffer for erase and kill (#@) */
int	cpusid;			/* cpu SID register */
char	version[];		/* system version */
int	lbolt;			/* time of day in 60th not in time */
time_t	time;			/* time in sec from 1970 */
time_t	bootime;		/* time at which booted */

int	hand;			/* current index into coremap used by daemon */

/*
 * Nblkdev is the number of entries
 * (rows) in the block switch. It is
 * set in binit/bio.c by making
 * a pass over the switch.
 * Used in bounds checking on major
 * device numbers.
 */
int	nblkdev;

/*
 * Number of character switch entries.
 * Set by cinit/prim.c
 */
int	nchrdev;

int	nswdev;			/* number of swap devices */
int	mpid;			/* generic for unique process id's */
char	runin;			/* scheduling flag */
char	runout;			/* scheduling flag */
int	runrun;			/* scheduling flag */
char	kmapwnt;		/* kernel map want flag */
char	curpri;			/* more scheduling */

int	maxmem;			/* actual max memory per process */
int	physmem;		/* physical memory on this CPU */

int	nswap;			/* size of swap space */
int	updlock;		/* lock for sync */
daddr_t	rablock;		/* block to be read ahead */
char	msgbuf[MSGBUFS];	/* saved "printf" characters */
int	intstack[512];		/* stack for interrupts */
dev_t	rootdev;		/* device of the root */
dev_t	swapdev;		/* swapping device */
dev_t	argdev;
dev_t	pipedev;		/* pipe device */

extern	int icode[];		/* user init code */
extern	int szicode;		/* its size */

extern	int printsw;		/* debug print switch */
extern	int coresw;		/* switch to force action on core dumps */

dev_t	getmdev();
daddr_t	bmap();
unsigned max();
unsigned min();
int	memall();
int	uchar();
int	vmemall();
swblk_t	vtod();

/*
 * Structure of the system-entry table
 */
extern struct sysent
{
	char	sy_narg;		/* total number of arguments */
	char	sy_nrarg;		/* number of args in registers */
	int	(*sy_call)();		/* handler */
} sysent[];

char	vmmap[];		/* poor name! */
int	mcr[3];			/* memory controller registers */
int	umbabeg,umbaend;	/* where sensitive vm begins/ends */
int	noproc;			/* no one is running just now */
