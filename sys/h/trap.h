/*	trap.h	4.1	11/9/80	*/

/*
 * Trap type values
 *
 * NB: OFFSETS HERE ARE ALSO DEFINED IN trap.m
 */

#define	RESADFLT	0		/* reserved addressing fault */
#define	PRIVINFLT	1		/* privileged instruction fault */
#define	BPTFLT		2		/* bpt instruction fault */
#define	XFCFLT		3		/* xfc instruction fault */
#define	RESOPFLT	4		/* reserved operand fault */
#define	SYSCALL		5		/* chmk instruction (syscall trap) */
#define	ARITHTRAP	6		/* arithmetic trap */
#define	RESCHED		7		/* software level 1 trap
					   (reschedule trap) */
#define	SEGFLT		8		/* segmentation fault */
#define	PROTFLT		9		/* protection fault */
#define	TRCTRAP		10		/* trace trap */
#define	COMPATFLT	11		/* compatibility mode fault */
#define	PAGEFLT		12		/* page fault */
#define	TABLEFLT	13		/* page table fault */
