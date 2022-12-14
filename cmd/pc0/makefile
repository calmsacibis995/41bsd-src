SCCSID = "@(#)pcmakefile 1.14 10/28/80"
WHOAMI = pc
INSTALLNAME = $(DESTDIR)/usr/lib/pc0
VERSION = 2.0

MKSTR = /usr/ucb/mkstr
EYACC = /usr/ucb/eyacc
RM = -rm -f
GET = touch

CFLAGS = -O -w
LDFLAGS = -z

LIBDIR = ${DESTDIR}/usr/lib
TMPDIR = tmp

ERRORSTRINGS = ${WHOAMI}${VERSION}strings

SRCS =	ato.c \
	call.c case.c clas.c const.c conv.c cset.c \
	error.c fdec.c flvalue.c func.c gen.c hash.c \
	lab.c lookup.c lval.c stklval.c \
	main.c nl.c proc.c put.c \
	rec.c rval.c stkrval.c\
	stat.c string.c subr.c \
	tree.c type.c var.c \
	TRdata.c \
	treen.c putn.c yycopy.c \
	yycosts.c yyerror.c yyget.c yyid.c yylex.c yymain.c yyoptions.c \
	yypanic.c yyparse.c yyprint.c yyput.c yyrecover.c yyseman.c yytree.c \
	p2put.c pcforop.c stab.c pcproc.c pcfunc.c pccaseop.c pclval.c

HDRS =	0.h OPnames.h align.h iorec.h objfmt.h pstab.h pc.h pcops.h \
	send.h tree.h whoami.h yy.h

OTHERS = pas.y opc.c version.c gram pic.c

OBJS =	ato.o \
	call.o case.o clas.o const.o conv.o cset.o \
	error.o fdec.o flvalue.o func.o gen.o hash.o \
	lab.o lookup.o lval.o stklval.o \
	main.o nl.o proc.o put.o \
	rec.o rval.o stkrval.o\
	stat.o string.o subr.o \
	tree.o type.o var.o \
	TRdata.o \
	treen.o putn.o yycopy.o \
	y.tab.o \
	yycosts.o yyerror.o yyget.o yyid.o yylex.o yymain.o yyoptions.o \
	yypanic.o yyparse.o yyprint.o yyput.o yyrecover.o yyseman.o yytree.o \
	p2put.o pcforop.o stab.o pcproc.o pcfunc.o pccaseop.o pclval.o

a.out: ${OBJS} version
	./version > Version.c
	${CC} ${CFLAGS} ${LDFLAGS} ${OBJS} Version.c

sources: ${SRCS} ${HDRS} ${OTHERS}
	
${SRCS} ${HDRS} ${OTHERS}:
	${GET} $@

.c.o:
	${RM} ${TMPDIR}/$*.c
	${MKSTR} - ${ERRORSTRINGS} ${TMPDIR}/ $*.c
	cd ${TMPDIR} ; ${CC} ${CFLAGS} -I.. -c $*.c ; mv $*.o  ../$*.o
	${RM} ${TMPDIR}/$*.c

y.tab.h: pas.y gram
	${EYACC} pas.y > /dev/null
	ex - y.tab.c <gram
	@echo that makes y.tab.h and y.tab.c

opcode.h: OPnames.h opc.c
	cc opc.c -o opc
	opc >opcode.h
	${RM} opc

pTree.h:
	echo "/* this is not pTree.h */" > pTree.h

version: version.c
	${CC} version.c -o version

clean:
	${RM} *.o ${TMPDIR}/*.c
	${RM} y.tab.h y.tab.c y.tab.out
	${RM} ${ERRORSTRINGS}
	${RM} version Version.c
	${RM} a.out core *.list *.bak
	${RM} opc pic tags

print:	sources
	@pr makefile READ_ME
	@ls -ls | pr
	@cc -o pic pic.c
	@pic | pr
	@rm pic
	@pr 0.h whoami.h main.c pas.y
	@pr OPnames.h opcode.h tree.h
	@pr pc.h
	@pr [a-ln-x]*.c
	@pr yy.h yy*.c

install: a.out
	cp ${ERRORSTRINGS} ${LIBDIR}/${ERRORSTRINGS}
	cp ${INSTALLNAME} ${INSTALLNAME}.bak
	cp a.out ${INSTALLNAME}

depend:	sources
	/bin/grep '^#[ 	]*include' *.h \
		| sed '/<.*>/d' \
		| sed 's/\(.*\):[^"]*"\([^"]*\)".*/\1: \2/' >makedep
	/bin/grep '^#[ 	]*include' *.c \
		| sed '/<.*>/d' \
		| sed 's/:[^"]*"\([^"]*\)".*/: \1/' \
		| sed 's/\.c/.o/' >>makedep
	echo '/^# DO NOT DELETE THIS LINE/+2,$$d' >eddep
	echo '$$r makedep' >>eddep
	echo 'w' >>eddep
	cp makefile makefile.bak
	ed - makefile < eddep
	rm eddep makedep
	echo '# DEPENDENCIES MUST END AT END OF FILE' >> makefile
	echo '# IF YOU PUT STUFF HERE IT WILL GO AWAY' >> makefile
	echo '# see make depend above' >> makefile

# DO NOT DELETE THIS LINE -- make depend uses it

0.h: pTree.h
yy.h: y.tab.h
TRdata.o: whoami.h
TRdata.o: 0.h
ato.o: whoami.h
ato.o: 0.h
call.o: whoami.h
call.o: 0.h
call.o: tree.h
call.o: opcode.h
call.o: objfmt.h
call.o: pc.h
call.o: pcops.h
case.o: whoami.h
case.o: 0.h
case.o: tree.h
case.o: opcode.h
clas.o: whoami.h
clas.o: 0.h
clas.o: tree.h
const.o: whoami.h
const.o: 0.h
const.o: tree.h
conv.o: whoami.h
conv.o: 0.h
conv.o: opcode.h
conv.o: pcops.h
cset.o: whoami.h
cset.o: 0.h
cset.o: tree.h
cset.o: opcode.h
cset.o: objfmt.h
cset.o: pc.h
cset.o: pcops.h
error.o: whoami.h
error.o: 0.h
error.o: yy.h
fdec.o: whoami.h
fdec.o: 0.h
fdec.o: tree.h
fdec.o: opcode.h
fdec.o: objfmt.h
fdec.o: align.h
fdec.o: pc.h
fdec.o: pcops.h
flvalue.o: whoami.h
flvalue.o: 0.h
flvalue.o: tree.h
flvalue.o: opcode.h
flvalue.o: objfmt.h
flvalue.o: pc.h
flvalue.o: pcops.h
func.o: whoami.h
func.o: 0.h
func.o: tree.h
func.o: opcode.h
gen.o: whoami.h
gen.o: 0.h
gen.o: tree.h
gen.o: opcode.h
gen.o: objfmt.h
hash.o: whoami.h
hash.o: 0.h
hash.o: yy.h
lab.o: whoami.h
lab.o: 0.h
lab.o: tree.h
lab.o: opcode.h
lab.o: objfmt.h
lab.o: pc.h
lab.o: pcops.h
lookup.o: whoami.h
lookup.o: 0.h
lval.o: whoami.h
lval.o: 0.h
lval.o: tree.h
lval.o: opcode.h
lval.o: objfmt.h
lval.o: pc.h
lval.o: pcops.h
main.o: whoami.h
main.o: 0.h
main.o: yy.h
main.o: objfmt.h
nl.o: whoami.h
nl.o: 0.h
nl.o: opcode.h
nl.o: objfmt.h
opc.o: OPnames.h
p2put.o: whoami.h
p2put.o: 0.h
p2put.o: pcops.h
p2put.o: pc.h
pccaseop.o: whoami.h
pccaseop.o: 0.h
pccaseop.o: tree.h
pccaseop.o: objfmt.h
pccaseop.o: pcops.h
pccaseop.o: pc.h
pcforop.o: whoami.h
pcforop.o: 0.h
pcforop.o: opcode.h
pcforop.o: tree.h
pcforop.o: pc.h
pcforop.o: pcops.h
pcfunc.o: whoami.h
pcfunc.o: 0.h
pcfunc.o: tree.h
pcfunc.o: opcode.h
pcfunc.o: pc.h
pcfunc.o: pcops.h
pclval.o: whoami.h
pclval.o: 0.h
pclval.o: tree.h
pclval.o: opcode.h
pclval.o: objfmt.h
pclval.o: pc.h
pclval.o: pcops.h
pcproc.o: whoami.h
pcproc.o: 0.h
pcproc.o: tree.h
pcproc.o: opcode.h
pcproc.o: pc.h
pcproc.o: pcops.h
pic.o: OPnames.h
proc.o: whoami.h
proc.o: 0.h
proc.o: tree.h
proc.o: opcode.h
proc.o: objfmt.h
put.o: whoami.h
put.o: opcode.h
put.o: 0.h
put.o: objfmt.h
put.o: pc.h
put.o: OPnames.h
rec.o: whoami.h
rec.o: 0.h
rec.o: tree.h
rec.o: opcode.h
rval.o: whoami.h
rval.o: 0.h
rval.o: tree.h
rval.o: opcode.h
rval.o: objfmt.h
rval.o: pc.h
rval.o: pcops.h
stab.o: whoami.h
stab.o: 0.h
stab.o: pstab.h
stab.o: pc.h
stat.o: whoami.h
stat.o: 0.h
stat.o: tree.h
stat.o: objfmt.h
stat.o: pcops.h
stat.o: pc.h
stat.o: opcode.h
stklval.o: whoami.h
stklval.o: 0.h
stklval.o: tree.h
stklval.o: opcode.h
stklval.o: objfmt.h
stkrval.o: whoami.h
stkrval.o: 0.h
stkrval.o: tree.h
stkrval.o: opcode.h
stkrval.o: objfmt.h
stkrval.o: pcops.h
string.o: whoami.h
string.o: 0.h
string.o: send.h
subr.o: whoami.h
subr.o: 0.h
tree.o: whoami.h
tree.o: 0.h
type.o: whoami.h
type.o: 0.h
type.o: tree.h
type.o: objfmt.h
var.o: whoami.h
var.o: 0.h
var.o: align.h
var.o: pc.h
var.o: pcops.h
var.o: iorec.h
y.tab.o: whoami.h
y.tab.o: 0.h
y.tab.o: yy.h
y.tab.o: tree.h
yycopy.o: 0.h
yycopy.o: yy.h
yycosts.o: whoami.h
yycosts.o: 0.h
yycosts.o: yy.h
yyerror.o: whoami.h
yyerror.o: 0.h
yyerror.o: yy.h
yyget.o: whoami.h
yyget.o: 0.h
yyget.o: yy.h
yyid.o: whoami.h
yyid.o: 0.h
yyid.o: yy.h
yylex.o: whoami.h
yylex.o: 0.h
yylex.o: yy.h
yymain.o: whoami.h
yymain.o: 0.h
yymain.o: yy.h
yymain.o: objfmt.h
yyoptions.o: whoami.h
yyoptions.o: 0.h
yyoptions.o: yy.h
yypanic.o: whoami.h
yypanic.o: 0.h
yypanic.o: yy.h
yyparse.o: whoami.h
yyparse.o: 0.h
yyparse.o: yy.h
yyprint.o: whoami.h
yyprint.o: 0.h
yyprint.o: yy.h
yyput.o: whoami.h
yyput.o: 0.h
yyput.o: tree.h
yyput.o: yy.h
yyrecover.o: whoami.h
yyrecover.o: 0.h
yyrecover.o: yy.h
yyseman.o: whoami.h
yyseman.o: 0.h
yyseman.o: yy.h
yytree.o: whoami.h
yytree.o: 0.h
yytree.o: tree.h
# DEPENDENCIES MUST END AT END OF FILE
# IF YOU PUT STUFF HERE IT WILL GO AWAY
# see make depend above
