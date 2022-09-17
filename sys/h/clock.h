/*	clock.h	4.1	11/9/80	*/

/*
 * VAX clock registers
 */

#define	ICCS_RUN	0x1
#define	ICCS_TRANS	0x10
#define	ICCS_SS		0x20
#define	ICCS_IE		0x40
#define	ICCS_INT	0x80
#define	ICCS_ERR	0x80000000
	
#define	SECDAY		((unsigned)(24*60*60))		/* seconds per day */
#define	SECYR		((unsigned)(365*SECDAY))	/* per common year */
/*
 * TODRZERO is the what the TODR should contain when the ``year'' begins.
 * The TODR should always contain a number between 0 and SECYR+SECDAY.
 */
#define	TODRZERO	((unsigned)(1<<28))

#define	YRREF		1970
#define	LEAPYEAR(year)	((year)%4==0 && year != 2000)	/* good till 2100 */

/*
 * Start a 60 HZ clock.
 */
#define	clkstart() {\
	mtpr(NICR, -16667);	/* 16.667 milli-seconds */\
	mtpr(ICCS, ICCS_RUN+ICCS_IE+ICCS_TRANS+ICCS_INT+ICCS_ERR);\
}
#define	clkreld()	mtpr(ICCS, ICCS_RUN+ICCS_IE+ICCS_INT+ICCS_ERR)

#define	clkwrap()	(((unsigned)mfpr(TODR) - TODRZERO)/100 > SECYR+SECDAY)
