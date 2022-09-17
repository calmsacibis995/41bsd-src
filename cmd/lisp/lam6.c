static char *sccsid = "@(#)lam6.c	34.2 10/6/80";

#include "global.h"
#include <signal.h>
FILE *
mkstFI(base,count,flag)
char *base;
char flag;
{
	register FILE *p = stderr;

	/* find free file descriptor */
	for(;p->_flag&(_IOREAD|_IOWRT);p++)
		if(p >= _iob + _NFILE)
			error("Too many open files to do readlist",FALSE);
	p->_flag = _IOSTRG | flag;
	p->_cnt = count;
	p->_base = base;
	p->_ptr = base;
	p->_file = -1;
	return(p);
}
lispval
Lreadli()
{
	register lispval work, handy;
	register FILE *p;
	register char *string;
	register struct argent *lbot, *np;
	struct argent *olbot;
	FILE *opiport = piport;
	lispval Lread();
	int count;

	if(lbot->val==nil) {		/*effectively, return(matom(""));*/
		strbuf[0] = 0;
		return(getatom());
	}
	chkarg(1,"readlist");
	count = 1;

	/* compute length of list */
	for(work = lbot->val; TYPE(work)==DTPR; work=work->d.cdr)
		count++;
	string = (char *) alloca(count);
	p = mkstFI(string, count - 1, _IOREAD);
	for(work = lbot->val; TYPE(work)==DTPR; work=work->d.cdr) {
		handy = work->d.car;
		switch(TYPE(handy)) {
		case SDOT:
		case INT:
			*string++=handy->i;
			break;
		case ATOM:
			*string++ = *(handy->a.pname);
			break;
		case STRNG:
			*string++ = *(char *)handy;
			break;
		default:
			error("Non atom or int to readlist",FALSE);
		}
	}
	*string = 0;
	olbot = lbot;
	lbot = np;
	protect(P(p));
	work = Lread();
	lbot = olbot;
	frstFI(p);
	return(work);
}
frstFI(p)
register FILE *p;
{
	p->_flag=0;
	p->_base=0;
	p->_cnt = 0;
	p->_ptr = 0;
	p->_file = 0;
}
lispval
Lgetenv()
{
	register struct argent *mylbot=lbot;
	snpand(1);
	if((TYPE(mylbot->val))!=ATOM)
		error("argument to getenv must be atom",FALSE);

	strcpy(strbuf,getenv(mylbot->val->a.pname));
	return(getatom());
}
lispval
Lboundp()
{
	register struct argent *mynp=lbot;
	register lispval result, handy;
	snpand(3);

	if((TYPE(mynp->val))!=ATOM)
		error("argument to boundp must be atom",FALSE);
	if( (handy = mynp->val)->a.clb==CNIL)
		result = nil;
	else
		(result = newdot())->d.cdr = handy->a.clb;
	return(result);
}
lispval
Lplist()
{	
	register lispval atm;
	snpand(1);
	/* get property list of an atom or disembodied property list */

	chkarg(1,"plist");
	atm = lbot->val;
	switch(TYPE(atm)) {
	case ATOM:
	case DTPR:
		break;
	default:
		error("Only Atoms and disembodied property lists allowed for plist",FALSE);
	}
	if(atm==nil) return(nilplist);
	return(atm->a.plist);
}
lispval
Lsetpli()
{	/* set the property list of the given atom to the given list */
	register lispval atm, vall;
	register lispval dum1, dum2;
	register struct argent *lbot, *np;

	chkarg(2,"setplist");
	atm = lbot->val;
	if (TYPE(atm) != ATOM) error("First argument must be an atom",FALSE);
	vall = (np-1)->val;
	if (TYPE(vall)!= DTPR && vall !=nil)
	    error("Second argument must be a list",FALSE);
	if (atm==nil)
		nilplist = vall;
	else
		atm->a.plist = vall;
	return(vall);
}

lispval
Lsignal()
{
	register struct argent *mylbot = lbot;
	int i; register lispval handy, old;

	snpand(3);
	if(lbot-np==1)protect(nil);
	chkarg(2,"signal");
	handy = mylbot[AD].val;
	if(TYPE(handy)!=INT)
		error("First arg to signal must be an int",FALSE);
	i = handy->i & 15;
	handy = mylbot[AD+1].val;
	if(TYPE(handy)!=ATOM)
		error("Second arg to signal must be an atom",FALSE);
	old = sigacts[i];
	if(old==0) old = nil;
	if(handy==nil)
		sigacts[i]=((lispval) 0);
	else
		sigacts[i]=handy;
	return(old);
}
lispval
Lassq()
{
	register lispval work, handy, dum1, dum2;
	register struct argent *lbot, *np;

	chkarg(2,"assq");
	for(work = lbot[AD+1].val;
		work->d.car->d.car!=lbot->val&& work!=nil;
		work = work->d.cdr);
	return(work->d.car);
}
lispval
Lkilcopy()
{
	if(fork()==0) {
		asm(".byte 0");
	}
}
lispval
Larg()
{
	register lispval handy; register offset, count;
	snpand(3);

	handy = lexpr_atom->a.clb;
	if(handy==CNIL || TYPE(handy)!=DTPR)
		error("Arg: not in context of Lexpr.",FALSE);
	count = ((long *)handy->d.cdr) -1 - (long *)handy->d.car;
	if(np==lbot || lbot->val==nil)
		return(inewint(count+1));
	if(TYPE(lbot->val)!=INT || (offset = lbot->val->i - 1) > count || offset < 0 )
		error("Out of bounds: arg to \"Arg\"",FALSE);
	return( ((struct argent *)handy->d.car)[offset].val);
}
lispval
Lsetarg()
{
	register lispval handy, work;
	register limit, index;
	register struct argent *lbot, *np;

	chkarg(2,"setarg");
	handy = lexpr_atom->a.clb;
	if(handy==CNIL || TYPE(handy)!=DTPR)
		error("Arg: not in context of Lexpr.",FALSE);
	limit = ((long *)handy->d.cdr) - 1 -  (long *)(work = handy->d.car);
	handy = lbot->val;
	if(TYPE(handy)!=INT)
		error("setarg: first argument not integer",FALSE);
	if((index = handy->i - 1) < 0 || index > limit)
		error("setarg: index out of range");
	return(((struct argent *) work)[index].val = lbot[1].val);
}
lispval
Lptime(){
	extern int GCtime;
	int lgctime = GCtime;
	static struct tbuf {
		long	mytime;
		long	allelse[3];
	} current;
	register lispval result, handy;

	snpand(2);
	times(&current);
	result = newdot();
	handy = result;
	protect(result);
	result->d.cdr = newdot();
	result->d.car = inewint(current.mytime);
	handy = result->d.cdr;
	handy->d.car = inewint(lgctime);
	handy->d.cdr = nil;
	if(GCtime==0)
		GCtime = 1;
	return(result);
}

/* (err [value] [flag]) 
   where if value is present, it is the value to throw to the errset.
   flag if present must evaluate to nil, as we always evaluate value
   before unwinding stack
 */

lispval Lerr()
{
	register lispval handy;
	lispval errorh();
	char *mesg = "call to err";  /* default message */

	snpand(1);
	if(np==lbot) protect(nil);

	if ((np >= lbot + 2) && ((lbot+1)->val != nil))
		error("Second arg to err must be nil",FALSE);
	if ((lbot->val != nil) && (TYPE(lbot->val) == ATOM))
	    mesg = lbot->val->a.pname;		/* new message if atom */
				
	return(errorh(Vererr,mesg,lbot->val,nil));
}
lispval
Ltyi()
{
	register FILE *port;
	char val;
	snpand(1);

	if(lbot-np==0)protect(nil);
	port = okport(lbot->val,okport(Vpiport->a.clb,stdin));


	fflush(stdout);		/* flush any pending output characters */
	val = getc(port);
	if(val==EOF)
	{
		clearerr(port);
		if(sigintcnt > 0) sigcall(SIGINT);  /* eof might mean int */
	}
	return(inewint(val));
}
lispval
Ltyipeek()
{
	register FILE *port;
	char val;
	snpand(1);

	if(lbot-np==0) protect(nil);
	port = okport(lbot->val,okport(Vpiport->a.clb,stdin));

	fflush(stdout);		/* flush any pending output characters */
	val = getc(port);
	if(val==EOF)
		clearerr(port);
	ungetc(val,port);
	return(inewint(val));
}
lispval
Ltyo()
{
	register FILE *port;
	register lispval handy, where;
	char val;

	snpand(3);

	switch(np-lbot) {
	case 1:
		protect(nil);
	case 2: break;
	default:
		argerr("tyo");
	}
	handy = lbot->val;
	if(TYPE(handy)!=INT)
		error("Tyo demands number for 1st arg",FALSE);
	val = handy->i;

	where = lbot[1].val;
	port = (FILE *) okport(where,okport(Vpoport->a.clb,stdout));
	putc(val,port);
	return(handy);
}

#include "chkrtab.h"

lispval
Imkrtab(current)
{
	extern struct rtab {
		char ctable[132];
	} initread;
	register lispval handy; extern lispval lastrtab;

	static int cycle = 0;
	static char *nextfree;

	if((cycle++)%3==0) {
		nextfree = (char *) csegment(str_name,512,FALSE);
	}
	handy = newarray();
	handy->ar.data = nextfree;
	if(current == 0)
		*(struct rtab *)nextfree = initread;
	else
		*(struct rtab *)nextfree = *(struct rtab *)ctable;
	handy->ar.delta = inewint(4);
	handy->ar.length = inewint(sizeof(struct rtab)/sizeof(int));
	handy->ar.accfun = handy->ar.aux  = nil;
	nextfree += sizeof(struct rtab);
	return(handy);
}

/* makereadtable - arg : t or nil
	returns a readtable, t means return a copy of the initial readtable

			     nil means return a copy of the current readtable
*/
lispval
Lmakertbl()
{
	lispval handy = Vreadtable->a.clb;
	chkrtab(handy);

	if(lbot==np) error("makereadtable: wrong number of args",FALSE);

	if(TYPE(lbot->val) != ATOM) 
		error("makereadtable: arg must be atom",FALSE);

	if(lbot->val == nil) return(Imkrtab(1));
	else return(Imkrtab(0));
}

lispval
Lcpy1()
{
	register lispval handy = lbot->val, result = handy;

top:
	switch(TYPE(handy))
	{
	case INT:
		result = inewint(handy->i);
		break;
	case VALUE:
		(result = newval())->l = handy->l;
		break;
	case DOUB:
		(result = newdoub())->r = handy->r;
		break;
	default:
		lbot->val =
		    errorh(Vermisc,"Bad arg to cpy1",nil,TRUE,67,handy);
		goto top;
	}
	return(result);
}

/* copyint* . This returns a copy of its integer argument.  The copy will
 *	 be a fresh integer cell, and will not point into the read only
 *	 small integer table.
 */
lispval
Lcopyint()
{
	register lispval handy = lbot->val;
	register lispval ret;

  	while (TYPE(handy) != INT)
	{ handy=errorh(Vermisc,"copyint* : non integer arg",nil,TRUE,0,handy);}
	(ret = newint())->i = handy->i;
	return(ret);
}


