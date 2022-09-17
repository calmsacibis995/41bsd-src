#
# Copyright (c) 1979 Regents of the University of California
#
# char sccsid[] = "@(#)35iosubs.s 4.2 10/16/80";
#
# IO SUBROUTINES
#
	.globl	_getname	#activate a file
	.globl	_unit		#establish active file
	.globl	_iosync		#syncronize a file window
	.globl	_unsync		#push backed synced record for formatted read
	.globl	_pflush		#flush all active files
	.globl	_pclose		#close selected files
	.globl	_pmflush	#flush pxp buffer
#
# system I/O routines
#
	.globl	_fclose
	.globl	_fflush
	.globl	_fopen
	.globl	_fprintf
	.globl	_fputc
	.globl	_fread
	.globl	_fscanf
	.globl	_fwrite
	.globl	_mktemp
	.globl	_rewind
	.globl	_setbuf
	.globl	_ungetc
	.globl	_unlink
#
# standard files
#
# file records
#
	.set	fnamesze,76		#maximum size of file name
	.set	recsze,30+fnamesze	#file record size
	.set	FNAME,-recsze		#name of associated UNIX file
	.set	LCOUNT,-30		#number of lines of output
	.set	LLIMIT,-26		#maximum number of text lines
	.set	FBUF,-22		#FILE pointer
	.set	FCHAIN,-18		#chain to next file
	.set	FLEV,-14		#ptr to associated file variable
	.set	PFNAME,-10		#ptr to name of error msg file
	.set	FUNIT,-6		#file status flags
	.set	FSIZE,-4		#size of elements in the file
	.set	WINDOW,0		#file window element
#
# unit flags
#
	.set	FDEF,0x8000	#1 => reserved file name
	.set	FTEXT,0x4000	#1 => text file, process EOLN
	.set	FWRITE,0x2000	#1 => open for writing
	.set	FREAD,0x1000	#1 => open for reading
	.set	TEMP,0x0800	#1 => temporary file
	.set	SYNC,0x0400	#1 => window is out of sync
	.set	EOLN,0x0200	#1 => at end of line
	.set	EOF,0x0100	#1 => at end of file
#
# bit positions of unit flags
#
	.set	fDEF,15
	.set	fTEXT,14
	.set	fWRITE,13
	.set	fREAD,12
	.set	fTEMP,11
	.set	fSYNC,10
	.set	fEOLN,9
	.set	fEOF,8
#
# standard file buffers
#
	.set	ioEOF,4		#bit position flagging EOF
	.set	ioERR,5		#bit position flagging I/O error
	.set	FLAG,12		#record offset of error flag
#
# standard input file
#
	.data
	.space	fnamesze	#name of associated UNIX file
	.long	0		#line count
	.long	0		#line limit
	.long	__iob		#FILE pointer
	.long	stdout		#chain to next file
	.long	0		#ptr to associated file variable
	.long	sinnam		#ptr to name of error msg file
	.word	FTEXT+FREAD+SYNC  #file status flags
	.long	1		#size of elements in the file
stdin:
	.word	0		#file window element
#
# standard output file
#
	.space	fnamesze	#name of associated UNIX file
	.long	0		#line count
	.long	0		#line limit
	.long	__iob+16	#FILE pointer
	.long	stderr		#chain to next file
	.long	0		#ptr to associated file variable
	.long	soutnam		#ptr to name of error msg file
	.word	FTEXT+FWRITE+EOF  #file status flags
	.long	1		#size of elements in the file
stdout:
	.word	0		#file window element
#
# standard error file
#
	.space	fnamesze	#name of associated UNIX file
	.long	0		#line count
	.long	0		#line limit
	.long	__iob+32	#FILE pointer
	.long	0		#chain to next file
	.long	0		#ptr to associated file variable
	.long	msgnam		#ptr to name of error msg file
	.word	FTEXT+FWRITE	#file status flags
	.long	1		#size of elements in the file
stderr:
	.word	0		#file window element

tmpname:.byte	't,'m,'p,'.,'X,'X,'X,'X,'X,'X, 0
	.text
sinnam:	.byte	's,'t,'a,'n,'d,'a,'r,'d,' ,'i,'n,'p,'u,'t, 0
soutnam:.byte	's,'t,'a,'n,'d,'a,'r,'d,' ,'o,'u,'t,'p,'u,'t, 0
msgnam:	.byte	'M,'e,'s,'s,'a,'g,'e,' ,'f,'i,'l,'e, 0
monout:	.byte	'p,'m,'o,'n,'.,'o,'u,'t, 0
rdopen:	.byte	'r, 0
wtopen:	.byte	'w, 0
	.set	formfeed,12
	.set	linefeed,10
	.set	blank,' 
#
# getname
#
# takes the width of a string in the subopcode and
# returns a pointer to a file structure in r0
#
# there should be a string on the stack
# of length the contents of subopcode on top of
# a pointer to the file variable
#
# a new file structure is allocated if needed
# temporary names are generated, and given
# names are blank trimmed
#
# if a new file buffer is allocated, the address
# is stored in the file variable.
#
#
_getname: #(datasze, maxnamlen, name, fileptr)
#int		datasze;
#int		maxnamlen;
#char		*name;
#struct file	**fileptr;

	.word	R6|R7|R8|R9
	movl	4(ap),r6	#r6 has data size
	movl	12(ap), r8	# r8 points to file name
	movl	16(ap), r9	# r9 pts to file variable
	tstl	( r9)		#check for existing file record
	bneq	gotone
#
# allocate and initialize a new file record
#
	clrl	 r7		# r7 has status flags
	tstl	r6		#check for size
	bneq	l3502
	movw	$FTEXT, r7	#default to text file
	movl	$1,r6		#default size
l3502:
	addl3	$recsze,r6,-(sp)#size of record
	calls	$1,_palloc	#r0 points to allocated buffer
	addl2	$recsze,r0	#adjust to base of record
	clrl	LCOUNT(r0)	#set default line limits
	movl	_llimit,LLIMIT(r0)
	movw	 r7,FUNIT(r0)	#set flags
	movl	r6,FSIZE(r0)	#set size
	movl	 r9,FLEV(r0)	#set ptr to file variable
	movl	r0,( r9)	#set file var ptr
#
# link the new record into the file chain
#
	movl	$_fchain-FCHAIN,r6   #r6 pts to "previous" record
	movl	_fchain,r1	#r1 pts to "next" record
	brb	l3505
l3504:
	movl	r1,r6		#advance previous
	movl	FCHAIN(r1),r1	#get next
l3505:
	cmpl	FLEV(r1), r9	#check level
	blssu	l3504		#continue until greater

	movl	r1,FCHAIN(r0)	#link in new record
	movl	r0,FCHAIN(r6)

	movl	r0, r9		# r9 points to file record
	jbr	setname
#
# have a previous buffer, dispose of associated file
#
gotone:
	movl	( r9), r9		   # r9 points to file record
	bicw2	$~(FDEF+TEMP+FTEXT),FUNIT( r9)  #clear status flags
	bbc	$fDEF,FUNIT( r9),l3506	   #check for predefined file
	bicw2	$FDEF,FUNIT( r9)	   #clear predefined flag
	jbr	setname
l3506:
	pushl	FBUF( r9)		  #flush and close previous file
	calls	$1,_fclose
	movl	FBUF( r9),r0
	bbs	$ioERR,FLAG(r0),eclose
	bbc	$fTEMP,FUNIT( r9),setname #check for temp file
	tstl	 r8			  #remove renamed temp files
	beql	setname
	pushl	PFNAME( r9)
	calls	$1,_unlink
	tstl	r0		#check for remove error
	beql	setname
eunlink:
	movl	PFNAME( r9),_file
	movw	$EREMOVE,_perrno
	moval	error,PC(fp)	#error return
	ret
eclose:
	movl	PFNAME( r9),_file
	movw	$ECLOSE,_perrno
	moval	error,PC(fp)	#error return
	ret
#
# get the filename associated with the buffer
#
setname:
	tstl	 r8		#check for a given name
	bneq	l3508		#br => has a name
	tstb	FNAME( r9)	#check for no current name
	bneq	l3513		#br => had a previous name so use it
#
# no name given and no previous name, so generate
# a new one of the form tmp.xxxxxx
#
	bisw2	$TEMP,FUNIT( r9)#set status to temporary
	pushal	tmpname		#get a unique temp name
	calls	$1,_mktemp
	movl	r0, r8		#ptr to new temp name
	movl	$16,r2		#maximum name length
	brb	l3512
#
# put the new name into the structure
#
l3508:
	bicw2	$TEMP,FUNIT( r9)	#set permanent status
	locc	$blank,8(ap),( r8)	#check for trailing blanks
	subl3	r0,8(ap),r2		#deduct length of blanks
	cmpl	r2,$fnamesze		#check for name too long
	bgeq	enamsze
l3512:
	movc3	r2,( r8),FNAME( r9)	#move name into record
	clrb	(r3)			#place zero after name
	moval	FNAME( r9),PFNAME( r9)	#set pointer to name
l3513:
	movl	 r9,r0			#return ptr to file record
	ret
enamsze:
	movw	$ENAMESIZE,_perrno
	moval	error,PC(fp)		#error return
	ret
#
# unit establishes a new active file
#
_unit:
	.word	0
	movl	4(ap),r7
	beql	erefinaf
	bbs	$fDEF,FUNIT(r7),erefinaf
	movl	PFNAME(r7),_file
	ret
erefinaf:
	movw	$EREFINAF,_perrno
	moval	error,PC(fp)	#error return
	ret
#
# iosync insures that a useable image is in the buffer window
#
_iosync:
	.word	R6
	cvtwl	FUNIT(r7),r6	#r6 has FUNIT flags
	bbc	$fREAD,r6,eread	#error if not open for reading
	bbc	$fSYNC,r6,l3515	#check for already synced
	bbs	$fEOF,r6,epeof	#error if past EOF
	bicw2	$SYNC,r6	#clear unsynced flag
	pushl	FBUF(r7)	#stream
	pushl	$1		#number of items to read
	pushl	FSIZE(r7)	#data size
	pushl	r7		#ptr to input window
	calls	$4,_fread
	movl	FBUF(r7),r0	#check for EOF
	bbs	$ioERR,FLAG(r0),epeof
	bbs	$ioEOF,FLAG(r0),eof
	bbc	$fTEXT,r6,l3514	#check for text processing
	bicw2	$EOLN,r6	#check for EOLN
	cmpb	(r7),$linefeed
	bneq	l3514
	movb	$blank,(r7)	#blank out linefeed
	bisw2	$EOLN,r6
l3514:
	movw	r6,FUNIT(r7)	#update status flags
l3515:
	ret
eof:
	bisw2	$EOF,r6		#set EOF
	movw	r6,FUNIT(r7)	#update status flags
	pushr	$R2|R3|R4|R5	#save registers
	movc5	$0,(r0),$0,FSIZE(r7),(r7)  #clear buffer to undefined
	popr	$R2|R3|R4|R5	#restore registers
	ret
eread:
	movw	$EREADIT,_perrno
	moval	error,PC(fp)	#error return
	ret
epeof:
	movw	$EPASTEOF,_perrno
	moval	error,PC(fp)	#error return
	ret
#
# push back last char read to prepare for formatted read
#
_unsync:
	.word	0
	bbc	$fREAD,FUNIT(r7),eread	#error if not open for reading
	bbs	$fSYNC,FUNIT(r7),l3526	#push back window char
	pushl	FBUF(r7)
	cvtbl	(r7),-(sp)
	calls	$2,_ungetc
l3526:
	ret
#
# flush all active output files
#
_pflush:
	.word	R6
	movl	_fchain,r6
	beql	l3518
l3516:
	bbc	$fWRITE,FUNIT(r6),l3517
	pushl	FBUF(r6)
	calls	$1,_fflush
l3517:
	movl	FCHAIN(r6),r6
	bneq	l3516
l3518:
	ret
#
# close all active files down to the specified FLEV
#
_pclose:
	.word	R6|R9
	movl	_fchain, r9
	beql	l3520
l3519:
	cmpl	FLEV( r9),4(ap)
	bgtru	l3520
	bbs	$fDEF,FUNIT( r9),l3525
	movl	FBUF( r9),r6
	beql	l3525
	pushl	r6
	calls	$1,_fclose
	jbs	$ioERR,FLAG(r6),eclose
	bbc	$fTEMP,FUNIT( r9),l3525
	pushl	PFNAME( r9)		#remove TEMP files
	calls	$1,_unlink
	tstl	r0
	jneq	eunlink
l3525:
	subl3	$recsze, r9,-(sp)
	movl	FCHAIN( r9), r9
	calls	$1,_pfree
	tstl	 r9
	bneq	l3519
l3520:
	movl	r9,_fchain
	ret
#
# write out the pxp data
#
_pmflush:
	.word	R6
	tstl	_pxpbuf
	beql	l3521
	pushal	wtopen
	pushal	monout
	calls	$2,_fopen
	tstl	r0
	beql	l3522
	movl	r0,r6
	pushl	r6
	pushl	$1
	pushl	_pxpsize
	pushl	_pxpbuf
	calls	$4,_fwrite
	bbs	$ioERR,FLAG(r6),l3522
	pushl	r6
	calls	$1,_fclose
	bbs	$ioERR,FLAG(r6),l3522
l3521:
	ret
l3522:
	pushal	monout
	calls	$1,_perror
	ret
