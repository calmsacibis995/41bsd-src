/*
 * sequential formatted external routines
 */

#include "fio.h"

/*
 * read sequential formatted external
 */

extern int rd_ed(),rd_ned();
int x_rnew(),x_getc(),x_tab();

s_rsfe(a) cilist *a; /* start */
{	int n;
	reading = YES;
	if(n=c_sfe(a,READ)) return(n);
	if(curunit->uwrt) nowreading(curunit);
	getn= x_getc;
	doed= rd_ed;
	doned= rd_ned;
	donewrec = dorevert = doend = x_rnew;
	dotab = x_tab;
	if(pars_f(fmtbuf)) err(errflag,100,"read sfe")
	fmt_bg();
	return(OK);
}

x_rnew()			/* find next record */
{	int ch;
	if(!curunit->uend)
		while((ch=getc(cf))!='\n' && ch!=EOF);
	cursor=recpos=reclen=0;
	return(OK);
}

x_getc()
{	int ch;
	if(curunit->uend) return(EOF);
	if((ch=getc(cf))!=EOF && ch!='\n')
	{	recpos++;
		return(ch);
	}
	if(ch=='\n')
	{	ungetc(ch,cf);
		return(ch);
	}
	if(feof(cf)) curunit->uend = YES;
	return(EOF);
}

e_rsfe()
{	int n;
	n=en_fio();
	fmtbuf=NULL;
	return(n);
}

c_sfe(a,flag) cilist *a; /* check */
{	unit *p;
	int n;
	external=sequential=formatted=FORMATTED;
	fmtbuf=a->cifmt;
	lfname = NULL;
	elist = NO;
	errflag = a->cierr;
	endflag = a->ciend;
	lunit = a->ciunit;
	if(not_legal(lunit)) err(errflag,101,"sfe");
	curunit = p = &units[lunit];
	if(!p->ufd && (n=fk_open(flag,SEQ,FMT,(ftnint)lunit)) )
		err(errflag,n,"sfe")
	cf = curunit->ufd;
	elist = YES;
	lfname = curunit->ufnm;
	if(!p->ufmt) err(errflag,102,"sfe")
	if(p->url) err(errflag,105,"sfe")
	cursor=recpos=scale=reclen=0;
	radix = 10;
	signit = YES;
	cblank = curunit->ublnk;
	cplus = NO;
	return(OK);
}

/*
 * write sequential formatted external
 */

extern int w_ed(),w_ned();
int x_putc(),pr_put(),x_wend(),x_wnew();
ioflag new;

s_wsfe(a) cilist *a;	/*start*/
{	int n;
	reading = NO;
	if(n=c_sfe(a,WRITE)) return(n);
	if(!curunit->uwrt) nowwriting(curunit);
	curunit->uend = NO;
	if (curunit->uprnt) putn = pr_put;
	else putn = x_putc;
	new = YES;
	doed= w_ed;
	doned= w_ned;
	doend = x_wend;
	dorevert = donewrec = x_wnew;
	dotab = x_tab;
	if(pars_f(fmtbuf)) err(errflag,100,"write sfe")
	fmt_bg();
	return(OK);
}

x_putc(c)
{
	if(c=='\n') recpos = reclen = cursor = 0;
	else recpos++;
	if (c) putc(c,cf);
	return(OK);
}

pr_put(c)
{
	if(c=='\n')
	{	new = YES;
		recpos = reclen = cursor = 0;
	}
	else if(new)
	{	new = NO;
		if(c=='0') c = '\n';
		else if(c=='1') c = '\f';
		else return(OK);
	}
	else recpos++;
	if (c) putc(c,cf);
	return(OK);
}

x_tab()
{	int n;
	if(reclen < recpos) reclen = recpos;
	if(curunit->useek)
	{	if((recpos+cursor) < 0) return(107);
		n = reclen - recpos;	/* distance to eor, n>=0 */
		if((cursor-n) > 0)
		{	fseek(cf,(long)n,1);  /* find current eor */
			recpos = reclen;
			cursor -= n;
		}
		else
		{	fseek(cf,(long)cursor,1);  /* do not pass go */
			recpos += cursor;
			return(cursor=0);
		}
	}
	else
		if(cursor < 0) return(120);	/* cant go back */
	while(cursor--)
	{	if(reading)
		{	n = (*getn)();
			if(n=='\n')
			{	(*ungetn)(n,cf);
				return(110);
			}
			if(n==EOF) return(EOF);
		}
		else	(*putn)(' ');	/* fill in the empty record */
	}
	return(cursor=0);
}

x_wnew()
{
	if(reclen>recpos) fseek(cf,(long)(reclen-recpos),1);
	return((*putn)('\n'));
}

x_wend(last) char last;
{
	if(reclen>recpos) fseek(cf,(long)(reclen-recpos),1);
	return((*putn)(last));
}

/*
/*xw_rev()
/*{
/*	if(workdone) x_wSL();
/*	return(workdone=0);
/*}
/*
*/
e_wsfe()
{	return(e_rsfe()); }
