From uucp Thu Jan 10 01:37:58 1980
>From dmr Thu Jan 10 04:25:49 1980 remote from research
The system has been changed so that if a file being executed
begins with the magic characters #! , the rest of the line is understood
to be the name of an interpreter for the executed file.
Previously (and in fact still) the shell did much of this job;
it automatically executed itself on a text file with executable mode
when the text file's name was typed as a command.
Putting the facility into the system gives the following
benefits.

1) It makes shell scripts more like real executable files,
because they can be the subject of 'exec.'

2) If you do a 'ps' while such a command is running, its real
name appears instead of 'sh'.
Likewise, accounting is done on the basis of the real name.

3) Shell scripts can be set-user-ID.

4) It is simpler to have alternate shells available;
e.g. if you like the Berkeley csh there is no question about
which shell is to interpret a file.

5) It will allow other interpreters to fit in more smoothly.

To take advantage of this wonderful opportunity,
put

	#! /bin/sh

at the left margin of the first line of your shell scripts.
Blanks after ! are OK.  Use a complete pathname (no search is done).
At the moment the whole line is restricted to 16 characters but
this limit will be raised.


From uucp Thu Jan 10 01:37:49 1980
>From dmr Thu Jan 10 04:23:53 1980 remote from research
#include "../h/param.h"
#include "../h/systm.h"
#include "../h/map.h"
#include "../h/dir.h"
#include "../h/user.h"
#include "../h/proc.h"
#include "../h/buf.h"
#include "../h/reg.h"
#include "../h/inode.h"
#include "../h/seg.h"
#include "../h/acct.h"


/*
 * exec system call, with and without environments.
 */
struct execa {
	char	*fname;
	char	**argp;
	char	**envp;
};

exec()
{
	((struct execa *)u.u_ap)->envp = NULL;
	exece();
}

exece()
{
	register nc;
	register char *cp;
	register struct buf *bp;
	register struct execa *uap;
	int na, ne, bno, ucp, ap, c, indir, uid, gid;
	struct inode *ip;

	bno = 0;
	bp = 0;
	indir = 0;
	if ((ip = namei(uchar, 0)) == NULL)
		return;
	uid = u.u_uid;
	gid = u.u_gid;
	if (ip->i_mode&ISUID)
		uid = ip->i_uid;
	if (ip->i_mode&ISGID)
		gid = ip->i_gid;
again:
	if(access(ip, IEXEC))
		goto bad;
	if((ip->i_mode & IFMT) != IFREG ||
	   (ip->i_mode & (IEXEC|(IEXEC>>3)|(IEXEC>>6))) == 0) {
		u.u_error = EACCES;
		goto bad;
	}
	/*
	 * read in first few bytes
	 * of file for segment
	 * types and sizes:
	 * ux_mag = 407/410/411/405
	 *  407 is plain executable
	 *  410 is RO text
	 *  411 is separated ID
	 *  405 is overlaid text
	 *
	 * Also, an ascii line beginning
	 * with '#!' is the file name of a shell.
	 */
	u.u_base = (caddr_t)&u.u_exdata;
	u.u_count = sizeof(u.u_exdata);
	u.u_offset = 0;
	u.u_segflg = 1;
	readi(ip);
	u.u_segflg = 0;
	if(u.u_error)
		goto bad;
	if (u.u_count > sizeof(u.u_exdata) - sizeof(u.u_exdata.A)
	  && u.u_exdata.S[0] != '#') {
		u.u_error = ENOEXEC;
		goto bad;
	}
	if(u.u_exdata.A.ux_mag == 0407)
		;
	else if (u.u_exdata.A.ux_mag == 0411)
		;
	else if (u.u_exdata.A.ux_mag == 0405)
		;
	else if (u.u_exdata.A.ux_mag == 0410)
		;
	else if (u.u_exdata.S[0]=='#' && u.u_exdata.S[1]=='!' && indir==0) {
		cp = &u.u_exdata.S[2];
		while (*cp==' ' && cp<&u.u_exdata.S[SHSIZ])
			cp++;
		u.u_dirp = cp;
		while (cp < &u.u_exdata.S[SHSIZ-1]  && *cp != '\n')
			cp++;
		*cp = '\0';
		indir++;
		iput(ip);
		ip = namei(schar, 0);
		if (ip==NULL)
			return;
		goto again;
	} else {
		u.u_error = ENOEXEC;
		goto bad;
	}
	/*
	 * Collect arguments on "file" in swap space.
	 */
	na = 0;
	ne = 0;
	nc = 0;
	uap = (struct execa *)u.u_ap;
	while ((bno = malloc(argmap,(NCARGS+BSIZE-1)/BSIZE)) == 0)
		sleep((caddr_t)&argmap, PRIBIO);
	if (uap->argp) for (;;) {
		ap = NULL;
		if (indir && na==1)
			ap = uap->fname;
		else if (uap->argp) {
			ap = fuword((caddr_t)uap->argp);
			uap->argp++;
		}
		if (ap==NULL && uap->envp) {
			uap->argp = NULL;
			if ((ap = fuword((caddr_t)uap->envp)) == NULL)
				break;
			uap->envp++;
			ne++;
		}
		if (ap==NULL)
			break;
		na++;
		if(ap == -1)
			u.u_error = EFAULT;
		do {
			if (nc >= NCARGS-1)
				u.u_error = E2BIG;
			if ((c = fubyte((caddr_t)ap++)) < 0)
				u.u_error = EFAULT;
			if (u.u_error)
				goto bad;
			if ((nc&BMASK) == 0) {
				if (bp)
					bawrite(bp);
				bp = getblk(swapdev, swplo+bno+(nc>>BSHIFT));
				cp = bp->b_un.b_addr;
			}
			nc++;
			*cp++ = c;
		} while (c>0);
	}
	if (bp)
		bawrite(bp);
	bp = 0;
	nc = (nc + NBPW-1) & ~(NBPW-1);
	getxfile(ip, nc, uid, gid);
	if (u.u_error || u.u_exdata.A.ux_mag==0405)
		goto bad;

	/*
	 * copy back arglist
	 */
	ucp = -nc - NBPW;
	ap = ucp - na*NBPW - 3*NBPW;
	u.u_ar0[R6] = ap;
	suword((caddr_t)ap, na-ne);
	nc = 0;
	for (;;) {
		ap += NBPW;
		if (na==ne) {
			suword((caddr_t)ap, 0);
			ap += NBPW;
		}
		if (--na < 0)
			break;
		suword((caddr_t)ap, ucp);
		do {
			if ((nc&BMASK) == 0) {
				if (bp)
					brelse(bp);
				bp = bread(swapdev, swplo+bno+(nc>>BSHIFT));
				bp->b_flags &= ~B_DELWRI;
				cp = bp->b_un.b_addr;
				if (nc==0 && indir)
					bcopy(cp, (caddr_t)u.u_dbuf, DIRSIZ);
			}
			subyte((caddr_t)ucp++, (c = *cp++));
			nc++;
		} while(c&0377);
	}
	suword((caddr_t)ap, 0);
	suword((caddr_t)ucp, 0);
	setregs();
bad:
	if (bp)
		brelse(bp);
	if(bno) {
		mfree(argmap, (NCARGS+BSIZE-1)/BSIZE, bno);
		wakeup((caddr_t)&argmap);
	}
	iput(ip);
}

/*
 * Read in and set up memory for executed file.
 */
getxfile(ip, nargc, uid, gid)
register struct inode *ip;
{
	register unsigned ds;
	register sep;
	register unsigned ts, ss;
	int i;
	long lsize;

	sep = 0;
	if(u.u_exdata.A.ux_mag == 0407) {
		lsize = (long)u.u_exdata.A.ux_dsize + u.u_exdata.A.ux_tsize;
		u.u_exdata.A.ux_dsize = lsize;
		if (lsize != u.u_exdata.A.ux_dsize) {	/* check overflow */
			u.u_error = ENOMEM;
			return;
		}
		u.u_exdata.A.ux_tsize = 0;
	} else if (u.u_exdata.A.ux_mag == 0411)
		sep++;
	if(u.u_exdata.A.ux_tsize!=0 && (ip->i_flag&ITEXT)==0 && ip->i_count!=1) {
		u.u_error = ETXTBSY;
		return;
	}

	/*
	 * find text and data sizes
	 * try them out for possible
	 * overflow of max sizes
	 */
	ts = btoc(u.u_exdata.A.ux_tsize);
	lsize = (long)u.u_exdata.A.ux_dsize + u.u_exdata.A.ux_bsize;
	if (lsize != (unsigned)lsize) {
		u.u_error = ENOMEM;
		return;
	}
	ds = btoc(lsize);
	ss = SSIZE + btoc(nargc);
	if (u.u_exdata.A.ux_mag==0405) {
		if (u.u_sep==0 && ctos(ts) != ctos(u.u_tsize) || nargc) {
			u.u_error = ENOMEM;
			return;
		}
		ds = u.u_dsize;
		ss = u.u_ssize;
		sep = u.u_sep;
		xfree();
		xalloc(ip);
		u.u_ar0[PC] = u.u_exdata.A.ux_entloc & ~01;
	} else {
		if(estabur(ts, ds, ss, sep, RO))
			return;
	
		/*
		 * allocate and clear core
		 * at this point, committed
		 * to the new image
		 */
		u.u_prof.pr_scale = 0;
		xfree();
		i = USIZE+ds+ss;
		expand(i);
		while(--i >= USIZE)
			clearseg(u.u_procp->p_addr+i);
		xalloc(ip);
	
		/*
		 * read in data segment
		 */
		estabur((unsigned)0, ds, (unsigned)0, 0, RO);
		u.u_base = 0;
		u.u_offset = sizeof(u.u_exdata.A)+u.u_exdata.A.ux_tsize;
		u.u_count = u.u_exdata.A.ux_dsize;
		readi(ip);
		/*
		 * set SUID/SGID protections, if no tracing
		 */
		if ((u.u_procp->p_flag&STRC)==0) {
			u.u_uid = uid;
			u.u_gid = gid;
		} else
			psignal(u.u_procp, SIGTRC);
	}
	u.u_tsize = ts;
	u.u_dsize = ds;
	u.u_ssize = ss;
	u.u_sep = sep;
	estabur(ts, ds, ss, sep, RO);
}

/*
 * Clear registers on exec
 */
setregs()
{
	register int *rp;
	register char *cp;
	register i;

	for(rp = &u.u_signal[0]; rp < &u.u_signal[NSIG]; rp++)
		if((*rp & 1) == 0)
			*rp = 0;
	for(cp = &regloc[0]; cp < &regloc[6];)
		u.u_ar0[*cp++] = 0;
	u.u_ar0[PC] = u.u_exdata.A.ux_entloc & ~01;
	for(rp = (int *)&u.u_fps; rp < (int *)&u.u_fps.u_fpregs[6];)
		*rp++ = 0;
	for(i=0; i<NOFILE; i++) {
		if (u.u_pofile[i]&EXCLOSE) {
			closef(u.u_ofile[i]);
			u.u_ofile[i] = NULL;
			u.u_pofile[i] &= ~EXCLOSE;
		}
	}
	/*
	 * Remember file name for accounting.
	 */
	u.u_acflag &= ~AFORK;
	bcopy((caddr_t)u.u_dbuf, (caddr_t)u.u_comm, DIRSIZ);
}

/*
 * exit system call:
 * pass back caller's arg
 */
rexit()
{
	register struct a {
		int	rval;
	} *uap;

	uap = (struct a *)u.u_ap;
	exit((uap->rval & 0377) << 8);
}

/*
 * Release resources.
 * Save u. area for parent to look at.
 * Enter zombie state.
 * Wake up parent and init processes,
 * and dispose of children.
 */
exit(rv)
{
	register int i;
	register struct proc *p, *q;
	register struct file *f;

	p = u.u_procp;
	p->p_flag &= ~(STRC|SULOCK);
	p->p_clktim = 0;
	for(i=0; i<NSIG; i++)
		u.u_signal[i] = 1;
	for(i=0; i<NOFILE; i++) {
		f = u.u_ofile[i];
		u.u_ofile[i] = NULL;
		closef(f);
	}
	plock(u.u_cdir);
	iput(u.u_cdir);
	if (u.u_rdir) {
		plock(u.u_rdir);
		iput(u.u_rdir);
	}
	xfree();
	acct();
	mfree(coremap, p->p_size, p->p_addr);
	p->p_stat = SZOMB;
	((struct xproc *)p)->xp_xstat = rv;
	((struct xproc *)p)->xp_utime = u.u_cutime + u.u_utime;
	((struct xproc *)p)->xp_stime = u.u_cstime + u.u_stime;
	for(q = &proc[0]; q < &proc[NPROC]; q++)
		if(q->p_ppid == p->p_pid) {
			wakeup((caddr_t)&proc[1]);
			q->p_ppid = 1;
			if (q->p_stat==SSTOP)
				setrun(q);
		}
	for(q = &proc[0]; q < &proc[NPROC]; q++)
		if(p->p_ppid == q->p_pid) {
			wakeup((caddr_t)q);
			swtch();
			/* no return */
		}
	swtch();
}

/*
 * Wait system call.
 * Search for a terminated (zombie) child,
 * finally lay it to rest, and collect its status.
 * Look also for stopped (traced) children,
 * and pass back status from them.
 */
wait()
{
	register f;
	register struct proc *p;

	f = 0;

loop:
	for(p = &proc[0]; p < &proc[NPROC]; p++)
	if(p->p_ppid == u.u_procp->p_pid) {
		f++;
		if(p->p_stat == SZOMB) {
			u.u_r.V.r_val1 = p->p_pid;
			u.u_r.V.r_val2 = ((struct xproc *)p)->xp_xstat;
			u.u_cutime += ((struct xproc *)p)->xp_utime;
			u.u_cstime += ((struct xproc *)p)->xp_stime;
			p->p_pid = 0;
			p->p_ppid = 0;
			p->p_pgrp = 0;
			p->p_sig = 0;
			p->p_flag = 0;
			p->p_wchan = 0;
			p->p_stat = NULL;
			return;
		}
		if(p->p_stat == SSTOP) {
			if((p->p_flag&SWTED) == 0) {
				p->p_flag |= SWTED;
				u.u_r.V.r_val1 = p->p_pid;
				u.u_r.V.r_val2 = (fsig(p)<<8) | 0177;
				return;
			}
			continue;
		}
	}
	if(f) {
		sleep((caddr_t)u.u_procp, PWAIT);
		goto loop;
	}
	u.u_error = ECHILD;
}

/*
 * fork system call.
 */
fork()
{
	register struct proc *p1, *p2;
	register a;

	/*
	 * Make sure there's enough swap space for max
	 * core image, thus reducing chances of running out
	 */
	if ((a = malloc(swapmap, ctod(MAXMEM))) == 0) {
		u.u_error = ENOMEM;
		goto out;
	}
	mfree(swapmap, ctod(MAXMEM), a);
	a = 0;
	p2 = NULL;
	for(p1 = &proc[0]; p1 < &proc[NPROC]; p1++) {
		if (p1->p_stat==NULL && p2==NULL)
			p2 = p1;
		else {
			if (p1->p_uid==u.u_uid && p1->p_stat!=NULL)
				a++;
		}
	}
	/*
	 * Disallow if
	 *  No processes at all;
	 *  not su and too many procs owned; or
	 *  not su and would take last slot.
	 */
	if (p2==NULL || (u.u_uid!=0 && (p2==&proc[NPROC-1] || a>MAXUPRC))) {
		u.u_error = EAGAIN;
		goto out;
	}
	p1 = u.u_procp;
	if(newproc()) {
		u.u_r.V.r_val1 = p1->p_pid;
		u.u_start = time;
		u.u_cstime = 0;
		u.u_stime = 0;
		u.u_cutime = 0;
		u.u_utime = 0;
		u.u_acflag = AFORK;
		return;
	}
	u.u_r.V.r_val1 = p2->p_pid;

out:
	u.u_ar0[R7] += NBPW;
}

/*
 * break system call.
 *  -- bad planning: "break" is a dirty word in C.
 */
sbreak()
{
	struct a {
		char	*nsiz;
	};
	register a, n, d;
	int i;

	/*
	 * set n to new data size
	 * set d to new-old
	 * set n to new total size
	 */

	n = btoc((int)((struct a *)u.u_ap)->nsiz);
	if(!u.u_sep)
		n -= ctos(u.u_tsize) * stoc(1);
	if(n < 0)
		n = 0;
	d = n - u.u_dsize;
	n += USIZE+u.u_ssize;
	if(estabur(u.u_tsize, u.u_dsize+d, u.u_ssize, u.u_sep, RO))
		return;
	u.u_dsize += d;
	if(d > 0)
		goto bigger;
	a = u.u_procp->p_addr + n - u.u_ssize;
	i = n;
	n = u.u_ssize;
	while(n--) {
		copyseg(a-d, a);
		a++;
	}
	expand(i);
	return;

bigger:
	expand(n);
	a = u.u_procp->p_addr + n;
	n = u.u_ssize;
	while(n--) {
		a--;
		copyseg(a-d, a);
	}
	while(d--)
		clearseg(--a);
}


