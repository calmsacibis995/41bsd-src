/*	trap.c	4.2	11/9/80	*/


#include "../h/param.h"
#include "../h/systm.h"
#include "../h/dir.h"
#include "../h/user.h"
#include "../h/proc.h"
#include "../h/reg.h"
#include "../h/seg.h"
#include "../h/trap.h"
#include "../h/psl.h"
#include "../h/pte.h"
#include "../h/inline.h"

#define	USER	040		/* user-mode flag added to type */

struct	sysent	sysent[128];

/*
 * Called from the trap handler when a processor trap occurs.
 */
/*ARGSUSED*/
trap(sp, type, code, pc, psl)
unsigned code;
{
	register int *locr0 = ((int *)&psl)-PS;
	register int i;
	register struct proc *p;
	time_t syst;

	syst = u.u_vm.vm_stime;
	if (USERMODE(locr0[PS])) {
		type |= USER;
		u.u_ar0 = locr0;
	}
	switch (type) {

	default:
		printf("trap type %d, code = %x\n", type, code);
		panic("trap");

	case PROTFLT + USER:	/* protection fault */
		i = SIGBUS;
		break;

	case PRIVINFLT + USER:	/* privileged instruction fault */
	case RESADFLT + USER:	/* reserved addressing fault */
	case RESOPFLT + USER:	/* resereved operand fault */
		i = SIGILL;
		break;

	case RESCHED + USER:	/* Allow process switch */
		goto out;

	case ARITHTRAP + USER:
		i = SIGFPE;
		break;

	/*
	 * If the user SP is above the stack segment,
	 * grow the stack automatically.
	 */
	case SEGFLT + USER: /* segmentation exception */
		if(grow((unsigned)locr0[SP]) || grow(code))
			goto out;
		i = SIGSEGV;
		break;

	case TABLEFLT:		/* allow page table faults in kernel mode */
	case TABLEFLT + USER:   /* page table fault */
		panic("page table fault");

	case PAGEFLT:		/* allow page faults in kernel mode */
	case PAGEFLT + USER:	/* page fault */
		i = u.u_error;
		pagein(code);   /* bring in page containing virtual addr */
		u.u_error = i;
/*
		if (type == PAGEFLT)
*/
			return;
/*
		goto out;
*/

	case BPTFLT + USER:	/* bpt instruction fault */
	case TRCTRAP + USER:	/* trace trap */
		locr0[PS] &= ~PSL_T;	/* turn off trace bit */
		i = SIGTRAP;
		break;

	case XFCFLT + USER:	/* xfc instruction fault */
		i = SIGEMT;
		break;

	case COMPATFLT + USER:	/* compatibility mode fault */
		u.u_cfcode = code;
		i = SIGILL;
		break;
	}
	psignal(u.u_procp, i);
out:
	p = u.u_procp;
	if (p->p_cursig || ISSIG(p))
		psig();
	p->p_pri = p->p_usrpri;
	if (runrun) {
		/*
		 * Since we are u.u_procp, clock will normally just change
		 * our priority without moving us from one queue to another
		 * (since the running process is not on a queue.)
		 * If that happened after we setrq ourselves but before we
		 * swtch()'ed, we might not be on the queue indicated by
		 * our priority.
		 */
		(void) spl6();
		setrq(p);
		swtch();
	}
	if (u.u_prof.pr_scale && (syst -= u.u_vm.vm_stime))
		addupc((caddr_t)locr0[PC], &u.u_prof, (int)-syst);
	curpri = p->p_pri;
}

/*
 * Called from the trap handler when a system call occurs
 */
/*ARGSUSED*/
syscall(sp, type, code, pc, psl)
unsigned code;
{
	register int *locr0 = ((int *)&psl)-PS;
	register caddr_t params;		/* known to be r10 below */
	register int i;				/* known to be r9 below */
	register struct sysent *callp;
	register struct proc *p;
	time_t syst;
	int opc;

	syst = u.u_vm.vm_stime;
	if (!USERMODE(locr0[PS]))
		panic("syscall");
	u.u_ar0 = locr0;
	params = (caddr_t)locr0[AP] + NBPW;
	u.u_error = 0;
	opc = pc - 2;
	if (code > 63)
		opc -= 2;
	callp = &sysent[code&0177];
	if (callp == sysent) {
		i = fuword(params);
		params += NBPW;
		callp = &sysent[i&0177];
	}
	if (i = callp->sy_narg * sizeof (int)) {
		asm("prober $3,r9,(r10)");		/* GROT */
		asm("bnequ ok");			/* GROT */
		u.u_error = EFAULT;			/* GROT */
		goto bad;				/* GROT */
asm("ok:");						/* GROT */
		asm("movc3 r9,(r10),_u+U_ARG");		/* GROT */
	}
	u.u_ap = u.u_arg;
	u.u_dirp = (caddr_t)u.u_arg[0];
	u.u_r.r_val1 = 0;
	u.u_r.r_val2 = locr0[R1];
	if (setjmp(u.u_qsav)) {
		if (u.u_error == 0 && u.u_eosys == JUSTRETURN)
			u.u_error = EINTR;
	} else {
		u.u_eosys = JUSTRETURN;
		(*(callp->sy_call))();
	}
	locr0[PS] &= ~PSL_C;
	if (u.u_eosys == RESTARTSYS)
		pc = opc;
	else if (u.u_eosys == SIMULATERTI)
		dorti();
	else if (u.u_error) {
bad:
		locr0[R0] = u.u_error;
		locr0[PS] |= PSL_C;	/* carry bit */
	} else {
		locr0[R0] = u.u_r.r_val1;
		locr0[R1] = u.u_r.r_val2;
	}
	p = u.u_procp;
	if (p->p_cursig || ISSIG(p))
		psig();
	p->p_pri = p->p_usrpri;
	if (runrun) {
		/*
		 * Since we are u.u_procp, clock will normally just change
		 * our priority without moving us from one queue to another
		 * (since the running process is not on a queue.)
		 * If that happened after we setrq ourselves but before we
		 * swtch()'ed, we might not be on the queue indicated by
		 * our priority.
		 */
		(void) spl6();
		setrq(p);
		swtch();
	}
	if (u.u_prof.pr_scale && (syst -= u.u_vm.vm_stime))
		addupc((caddr_t)locr0[PC], &u.u_prof, (int)-syst);
	curpri = p->p_pri;
}

/*
 * nonexistent system call-- set fatal error code.
 */
nosys()
{

	u.u_error = 100;
}

/*
 * Ignored system call
 */
nullsys()
{

}
