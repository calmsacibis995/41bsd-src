    /* Copyright (c) 1980 Regents of the University of California */

static	char sccsid[] = "@(#)pc3.c 1.6 9/9/80";

    /*
     *	     Pc3 is a pass in the Berkeley Pascal compilation
     *	process that is performed just prior to linking Pascal
     *	object files.  Its purpose is to enforce the rules of
     *	separate compilation for Berkeley Pascal.  Pc3 is called
     *	with the same argument list of object files that is sent to
     *	the loader.  These checks are performed by pc3 by examining
     *	the symbol tables of the object files:
     *	(1)  All source and included files must be "up-to-date" with
     *	     the object files of which they are components.
     *	(2)  Each global Pascal symbol (label, constant, type,
     *	     variable, procedure, or function name) must be uniquely
     *	     declared, i.e. declared in only one included file or
     *	     source file.
     *	(3)  Each external function (or procedure) may be resolved
     *	     at most once in a source file which included the
     *	     external declaration of the function.
     *	
     *	     The symbol table of each object file is scanned and
     *	each global Pascal symbol is placed in a hashed symbol
     *	table.  The Pascal compiler has been modified to emit all
     *	Pascal global symbols to the object file symbol table.  The
     *	information stored in the symbol table for each such symbol
     *	is:
     *	
     *	   - the name of the symbol;
     *	   - a subtype descriptor;
     *	   - for file symbols, their last modify time;
     *	   - the file which logically contains the declaration of
     *	     the symbol (not an include file);
     *	   - the file which textually contains the declaration of
     *	     the symbol (possibly an include file);
     *	   - the line number at which the symbol is declared;
     *	   - the file which contains the resolution of the symbol.
     *	   - the line number at which the symbol is resolved;
     *	
     *	     If a symbol has been previously entered into the symbol
     *	table, a check is made that the current declaration is of
     *	the same type and from the same include file as the previous
     *	one.  Except for files and functions and procedures, it is
     *	an error for a symbol declaration to be encountered more
     *	than once, unless the re-declarations come from the same
     *	included file as the original.
     *	
     *	     As an include file symbol is encountered in a source
     *	file, the symbol table entry of each symbol declared in that
     *	include file is modified to reflect its new logical
     *	inclusion in the source file.  File symbols are also
     *	encountered as an included file ends, signaling the
     *	continuation of the enclosing file.
     *	
     *	     Functions and procedures which have been declared
     *	external may be resolved by declarations from source files
     *	which included the external declaration of the function.
     *	Functions and procedures may be resolved at most once across
     *	a set of object files.  The loader will complain if a
     *	function is not resolved at least once.
     */

char	program[] = "pc";

#include <sys/types.h>
#include <ar.h>
#include <stdio.h>
#include <ctype.h>
#include <a.out.h>
#include <stab.h>
#include <pagsiz.h>
#include <stat.h>
#include "pstab.h"
#include "pc3.h"

int	errors = 0;

    /*
     *	check each of the argument .o files (or archives of .o files).
     */
main( argc , argv )
    int		argc;
    char	**argv;
    {
	struct fileinfo	ofile;

	while ( ++argv , --argc ) {
#	    ifdef DEBUG
		fprintf( stderr , "[main] *argv = %s\n" , *argv );
#	    endif DEBUG
	    ofile.name = *argv;
	    checkfile( &ofile );
	}
	exit( errors );
    }

    /*
     *	check the namelist of a file, or all namelists of an archive.
     */
checkfile( ofilep )
    struct fileinfo	*ofilep;
    {
	union {
	    char	mag_armag[ SARMAG + 1 ];
	    struct exec	mag_exec;
	}		mag_un;
	int		red;
	struct stat	filestat;

	ofilep -> file = fopen( ofilep -> name , "r" );
	if ( ofilep -> file == NULL ) {
	    error( WARNING , "cannot open: %s" , ofilep -> name );
	    return;
	}
	fstat( fileno( ofilep -> file ) , &filestat );
	ofilep -> modtime = filestat.st_mtime;
	red = fread( (char *) &mag_un , 1 , sizeof mag_un , ofilep -> file );
	if ( red != sizeof mag_un ) {
	    error( WARNING , "cannot read header: %s" , ofilep -> name );
	    return;
	}
	if ( mag_un.mag_exec.a_magic == OARMAG ) {
	    error( WARNING , "old archive: %s" , ofilep -> name );
	    return;
	}
	if ( strncmp( mag_un.mag_armag , ARMAG , SARMAG ) == 0 ) {
		/* archive, iterate through elements */
#	    ifdef DEBUG
		fprintf( stderr , "[checkfile] archive %s\n" , ofilep -> name );
#	    endif DEBUG
	    ofilep -> nextoffset = SARMAG;
	    while ( nextelement( ofilep ) ) {
		checknl( ofilep );
	    }
	} else if ( N_BADMAG( mag_un.mag_exec ) ) {
		/* not a file.o */
	    error( WARNING , "bad format: %s" , ofilep -> name );
	    return;
	} else {
		/* a file.o */
#	    ifdef DEBUG
		fprintf( stderr , "[checkfile] .o file %s\n" , ofilep -> name );
#	    endif DEBUG
	    fseek( ofilep -> file , 0L , 0 );
	    ofilep -> nextoffset = filestat.st_size;
	    checknl( ofilep );
	}
	fclose( ofilep -> file );
    }

    /*
     *	check the namelist of this file for conflicts with
     *	previously entered symbols.
     */
checknl( ofilep )
    register struct fileinfo	*ofilep;
    {
    
	long			red;
	struct exec		oexec;
	off_t			symoff;
	long			numsyms;
	register struct nlist	*nlp;
	register char		*stringp;
	long			strsize;
	long			sym;

	red = fread( (char *) &oexec , 1 , sizeof oexec , ofilep -> file );
	if ( red != sizeof oexec ) {
	    error( WARNING , "error reading struct exec: %s"
		    , ofilep -> name );
	    return;
	}
	if ( N_BADMAG( oexec ) ) {
	    return;
	}
	symoff = N_SYMOFF( oexec ) - sizeof oexec;
	fseek( ofilep -> file , symoff , 1 );
	numsyms = oexec.a_syms / sizeof ( struct nlist );
	if ( numsyms == 0 ) {
	    error( WARNING , "no name list: %s" , ofilep -> name );
	    return;
	}
	nlp = (struct nlist *) calloc( numsyms , sizeof ( struct nlist ) );
	if ( nlp == 0 ) {
	    error( FATAL , "no room for %d nlists" , numsyms );
	}
	red = fread( ( char * ) nlp , numsyms , sizeof ( struct nlist )
		    , ofilep -> file );
	if (   ftell( ofilep -> file ) + sizeof ( off_t )
	    >= ofilep -> nextoffset ) {
	    error( WARNING , "no string table (old format .o?)"
		    , ofilep -> name );
	    return;
	}
	red = fread( (char *) &strsize , sizeof strsize , 1
		    , ofilep -> file );
	if ( red != 1 ) {
	    error( WARNING , "no string table (old format .o?)"
		    , ofilep -> name );
	    return;
	}
	stringp  = ( char * ) malloc( strsize );
	if ( stringp == 0 ) {
	    error( FATAL , "no room for %d bytes of strings" , strsize );
	}
	red = fread( stringp + sizeof strsize
		    , strsize - sizeof ( strsize ) , 1 , ofilep -> file );
	if ( red != 1 ) {
	    error( WARNING , "error reading string table: %s"
		    , ofilep -> name );
	}
#	ifdef DEBUG
	    fprintf( stderr , "[checknl] %s: %d symbols\n"
		    , ofilep -> name , numsyms );
#	endif DEBUG
	for ( sym = 0 ; sym < numsyms ; sym++) {
	    if ( nlp[ sym ].n_un.n_strx ) {
		nlp[ sym ].n_un.n_name = stringp + nlp[ sym ].n_un.n_strx;
	    } else {
		nlp[ sym ].n_un.n_name = "";
	    }
	    checksymbol( &nlp[ sym ] , ofilep );
	}
	if ( nlp ) {
	    free( nlp );
	}
	if ( stringp ) {
	    free( stringp );
	}
    }

    /*
     *	check a symbol.
     *	look it up in the hashed symbol table,
     *	entering it if necessary.
     *	this maintains a state of which .p and .i files
     *	it is currently in the midst from the nlist entries
     *	for source and included files.
     *	if we are inside a .p but not a .i, pfilep == ifilep.
     */
checksymbol( nlp , ofilep )
    struct nlist	*nlp;
    struct fileinfo	*ofilep;
    {
	static struct symbol	*pfilep = NIL;
	static struct symbol	*ifilep = NIL;
	register struct symbol	*symbolp;

#	ifdef DEBUG
	    if ( pfilep && ifilep ) {
		fprintf( stderr , "[checksymbol] pfile %s ifile %s\n"
			, pfilep -> name , ifilep -> name );
	    }
	    fprintf( stderr , "[checksymbol] ->name %s ->n_desc %x (%s)\n"
		    , nlp -> n_un.n_name , nlp -> n_desc
		    , classify( nlp -> n_desc ) );
#	endif DEBUG
	if ( nlp -> n_type != N_PC ) {
		/* don't care about the others */
	    return;
	}
	symbolp = entersymbol( nlp -> n_un.n_name );
	if ( symbolp -> lookup == NEW ) {
#	    ifdef DEBUG
		fprintf( stderr , "[checksymbol] ->name %s is NEW\n"
			, symbolp -> name );
#	    endif DEBUG
	    symbolp -> desc = nlp -> n_desc;
	    switch ( symbolp -> desc ) {
		case N_PGLABEL:
		case N_PGCONST:
		case N_PGTYPE:
		case N_PGVAR:
		case N_PGFUNC:
		case N_PGPROC:
			symbolp -> sym_un.sym_str.rfilep = ifilep;
			symbolp -> sym_un.sym_str.rline = nlp -> n_value;
			symbolp -> sym_un.sym_str.fromp = pfilep;
			symbolp -> sym_un.sym_str.fromi = ifilep;
			symbolp -> sym_un.sym_str.iline = nlp -> n_value;
			return;
		case N_PEFUNC:
		case N_PEPROC:
			symbolp -> sym_un.sym_str.rfilep = NIL;
			symbolp -> sym_un.sym_str.rline = 0;
			    /*
			     *	functions can only be declared external
			     *	in included files.
			     */
			if ( pfilep == ifilep ) {
			    error( WARNING
				    , "%s, line %d: %s %s must be declared in included file"
				    , pfilep -> name , nlp -> n_value
				    , classify( symbolp -> desc )
				    , symbolp -> name );
			}
			symbolp -> sym_un.sym_str.fromp = pfilep;
			symbolp -> sym_un.sym_str.fromi = ifilep;
			symbolp -> sym_un.sym_str.iline = nlp -> n_value;
			return;
		case N_PSO:
			pfilep = symbolp;
			/* and fall through */
		case N_PSOL:
			ifilep = symbolp;
			symbolp -> sym_un.modtime = mtime( symbolp -> name );
			if ( symbolp -> sym_un.modtime > ofilep -> modtime ) {
			    error( WARNING , "%s is out of date with %s"
				    , ofilep -> name , symbolp -> name );
			}
			return;
	    }
	} else {
#	    ifdef DEBUG
		fprintf( stderr , "[checksymbol] ->name %s is OLD\n"
			, symbolp -> name );
#	    endif DEBUG
	    switch ( symbolp -> desc ) {
		case N_PSO:
			    /*
			     *	finding a file again means you are back
			     *	in it after finishing an include file.
			     */
			pfilep = symbolp;
			/* and fall through */
		case N_PSOL:
			    /*
			     *	include files can be seen more than once,
			     *	but they still have to be timechecked.
			     *	(this will complain twice for out of date
			     *	include files which include other files.
			     *	sigh.)
			     */
			ifilep = symbolp;
			if ( symbolp -> sym_un.modtime > ofilep -> modtime ) {
			    error( WARNING , "%s is out of date with %s"
				    , ofilep -> name , symbolp -> name );
			}
			return;
		case N_PEFUNC:
		case N_PEPROC:
			    /*
			     *	we may see any number of external declarations,
			     *	but they all have to come
			     *	from the same include file.
			     */
			if (   nlp -> n_desc == N_PEFUNC
			    || nlp -> n_desc == N_PEPROC ) {
			    goto included;
			}
			    /*
			     *	an external function can be resolved by
			     *	the resolution of the function
			     *	if the resolving file
			     *	included the external declaration.
			     */
			if (    (  symbolp -> desc == N_PEFUNC
				&& nlp -> n_desc != N_PGFUNC )
			    ||  (  symbolp -> desc == N_PEPROC
				&& nlp -> n_desc != N_PGPROC )
			    || symbolp -> sym_un.sym_str.fromp != pfilep ) {
			    break;
			}
			    /*
			     *	an external function can only be resolved once.
			     */
			if ( symbolp -> sym_un.sym_str.rfilep != NIL ) {
			    break;
			}
			symbolp -> sym_un.sym_str.rfilep = ifilep;
			symbolp -> sym_un.sym_str.rline = nlp -> n_value;
			return;
		case N_PGFUNC:
		case N_PGPROC:
			    /*
			     *	functions may not be seen more than once.
			     *	the loader will complain about
			     *	`multiply defined', but we can, too.
			     */
			break;
		case N_PGLABEL:
		case N_PGCONST:
		case N_PGTYPE:
		case N_PGVAR:
			    /*
			     *	labels, constants, types, variables
			     *	and external declarations
			     *	may be seen as many times as they want,
			     *	as long as they come from the same include file.
			     *	make it look like they come from this .p file.
			     */
included:
			if (  nlp -> n_desc != symbolp -> desc
			   || symbolp -> sym_un.sym_str.fromi != ifilep ) {
			    break;
			}
			symbolp -> sym_un.sym_str.fromp = pfilep;
			return;
	    }
		/*
		 *	this is the breaks
		 */
	    error( WARNING , "%s, line %d: %s already defined (%s, line %d)."
		    , ifilep -> name , nlp -> n_value , nlp -> n_un.n_name
		    , symbolp -> sym_un.sym_str.rfilep -> name
		    , symbolp -> sym_un.sym_str.rline );
	}
    }

    /*
     *	quadratically hashed symbol table.
     *	things are never deleted from the hash symbol table.
     *	as more hash table is needed,
     *	a new one is alloc'ed and chained to the end.
     *	search is by rehashing within each table,
     *	traversing chains to next table if unsuccessful.
     */
struct symbol *
entersymbol( name )
    char	*name;
    {
	static struct symboltableinfo	*symboltable = NIL;
	char				*enteredname;
	long				hashindex;
	register struct symboltableinfo	*tablep;
	register struct symbol		**herep;
	register struct symbol		**limitp;
	register long			increment;

	enteredname = enterstring( name );
	hashindex = SHORT_ABS( ( long ) enteredname ) % SYMBOLPRIME;
	for ( tablep = symboltable ; /*return*/ ; tablep = tablep -> chain ) {
	    if ( tablep == NIL ) {
#		ifdef DEBUG
		    fprintf( stderr , "[entersymbol] calloc\n" );
#		endif DEBUG
		tablep = ( struct symboltableinfo * )
			    calloc( sizeof ( struct symboltableinfo ) , 1 );
		if ( tablep == NIL ) {
		    error( FATAL , "ran out of memory (entersymbol)" );
		}
		if ( symboltable == NIL ) {
		    symboltable = tablep;
		}
	    }
	    herep = &( tablep -> entry[ hashindex ] );
	    limitp = &( tablep -> entry[ SYMBOLPRIME ] );
	    increment = 1;
	    do {
#		ifdef DEBUG
		    fprintf( stderr , "[entersymbol] increment %d\n" 
			    , increment );
#		endif DEBUG
		if ( *herep == NIL ) {
			/* empty */
		    if ( tablep -> used > ( ( SYMBOLPRIME / 3 ) * 4 ) ) {
			    /* too full, break for next table */
			break;
		    }
		    tablep -> used++;
		    *herep = symbolalloc();
		    ( *herep ) -> name = enteredname;
		    ( *herep ) -> lookup = NEW;
#		    ifdef DEBUG
			fprintf( stderr , "[entersymbol] name %s NEW\n"
				, enteredname );
#		    endif DEBUG
		    return *herep;
		}
		    /* a find? */
		if ( ( *herep ) -> name == enteredname ) {
		    ( *herep ) -> lookup = OLD;
#		    ifdef DEBUG
			fprintf( stderr , "[entersymbol] name %s OLD\n"
				, enteredname );
#		    endif DEBUG
		    return *herep;
		}
		herep += increment;
		if ( herep >= limitp ) {
		    herep -= SYMBOLPRIME;
		}
		increment += 2;
	    } while ( increment < SYMBOLPRIME );
	}
    }

    /*
     *	allocate a symbol from the dynamically allocated symbol table.
     */
struct symbol *
symbolalloc()
    {
	static struct symbol	*nextsymbol = NIL;
	static long		symbolsleft = 0;
	struct symbol		*newsymbol;

	if ( symbolsleft <= 0 ) {
#	    ifdef DEBUG
		fprintf( stderr , "[symbolalloc] malloc\n" );
#	    endif DEBUG
	    nextsymbol = ( struct symbol * ) malloc( SYMBOLALLOC );
	    if ( nextsymbol == 0 ) {
		error( FATAL , "ran out of memory (symbolalloc)" );
	    }
	    symbolsleft = SYMBOLALLOC / sizeof( struct symbol );
	}
	newsymbol = nextsymbol;
	nextsymbol++;
	symbolsleft--;
	return newsymbol;
    }

    /*
     *	hash a string based on all of its characters.
     */
long
hashstring( string )
    char	*string;
    {
	register char	*cp;
	register long	value;

	value = 0;
	for ( cp = string ; *cp ; cp++ ) {
	    value = ( value * 2 ) + *cp;
	}
	return value;
    }

    /*
     *	quadratically hashed string table.
     *	things are never deleted from the hash string table.
     *	as more hash table is needed,
     *	a new one is alloc'ed and chained to the end.
     *	search is by rehashing within each table,
     *	traversing chains to next table if unsuccessful.
     */
char *
enterstring( string )
    char	*string;
    {
	static struct stringtableinfo	*stringtable = NIL;
	long				hashindex;
	register struct stringtableinfo	*tablep;
	register char			**herep;
	register char			**limitp;
	register long			increment;

	hashindex = SHORT_ABS( hashstring( string ) ) % STRINGPRIME;
	for ( tablep = stringtable ; /*return*/ ; tablep = tablep -> chain ) {
	    if ( tablep == NIL ) {
#		ifdef DEBUG
		    fprintf( stderr , "[enterstring] calloc\n" );
#		endif DEBUG
		tablep = ( struct stringtableinfo * )
			    calloc( sizeof ( struct stringtableinfo ) , 1 );
		if ( tablep == NIL ) {
		    error( FATAL , "ran out of memory (enterstring)" );
		}
		if ( stringtable == NIL ) {
		    stringtable = tablep;
		}
	    }
	    herep = &( tablep -> entry[ hashindex ] );
	    limitp = &( tablep -> entry[ STRINGPRIME ] );
	    increment = 1;
	    do {
#		ifdef DEBUG
		    fprintf( stderr , "[enterstring] increment %d\n" 
			    , increment );
#		endif DEBUG
		if ( *herep == NIL ) {
			/* empty */
		    if ( tablep -> used > ( ( STRINGPRIME / 3 ) * 4 ) ) {
			    /* too full, break for next table */
			break;
		    }
		    tablep -> used++;
		    *herep = charalloc( strlen( string ) );
		    strcpy( *herep , string );
#		    ifdef DEBUG
			fprintf( stderr , "[enterstring] string %s copied\n"
				, *herep );
#		    endif DEBUG
		    return *herep;
		}
		    /* quick, check the first chars and then the rest */
		if ( **herep == *string && strcmp( *herep , string ) == 0 ) {
#		    ifdef DEBUG
			fprintf( stderr , "[enterstring] string %s found\n"
				, *herep );
#		    endif DEBUG
		    return *herep;
		}
		herep += increment;
		if ( herep >= limitp ) {
		    herep -= STRINGPRIME;
		}
		increment += 2;
	    } while ( increment < STRINGPRIME );
	}
    }

    /*
     *	copy a string to the dynamically allocated character table.
     */
char *
charalloc( length )
    register long	length;
    {
	static char	*nextchar = NIL;
	static long	charsleft = 0;
	register long	lengthplus1 = length + 1;
	register long	askfor;
	char		*newstring;

	if ( charsleft < lengthplus1 ) {
	    askfor = lengthplus1 > CHARALLOC ? lengthplus1 : CHARALLOC;
#	    ifdef DEBUG
		fprintf( stderr , "[charalloc] malloc( %d )\n" 
			, askfor );
#	    endif DEBUG
	    nextchar = ( char * ) malloc( askfor );
	    if ( nextchar == 0 ) {
		error( FATAL , "no room for %d characters" , askfor );
	    }
	    charsleft = askfor;
	}
	newstring = nextchar;
	nextchar += lengthplus1;
	charsleft -= lengthplus1;
	return newstring;
    }

    /*
     *	read an archive header for the next element
     *	and find the offset of the one after this. 
     */
BOOL
nextelement( ofilep )
    struct fileinfo	*ofilep;
    {
	register char	*cp;
	register long	red;
	register off_t	arsize;
	struct ar_hdr	archdr;

	fseek( ofilep -> file , ofilep -> nextoffset , 0 );
	red = fread( (char *) &archdr , 1 , sizeof archdr , ofilep -> file );
	if ( red != sizeof archdr ) {
	    return FALSE;
	}
	    /* null terminate the blank-padded name */
	cp = &archdr.ar_name[ ( sizeof archdr.ar_name ) - 1 ];
	*cp = '\0';
	while ( *--cp == ' ' ) {
	    *cp = '\0';
	}
	    /* set up the address of the beginning of next element */
	arsize = atol( archdr.ar_size );
	    /* archive elements are aligned on 0 mod 2 boundaries */
	if ( arsize & 1 ) {
	    arsize += 1;
	}
	ofilep -> nextoffset = ftell( ofilep -> file ) + arsize;
	    /* say we had one */
	return TRUE;
    }

    /*
     *	variable number of arguments to error, like printf.
     */
error( fatal , message , arg1 , arg2 , arg3 , arg4 , arg5 , arg6 )
    int		fatal;
    char	*message;
    {
	fprintf( stderr , "%s: " , program );
	fprintf( stderr , message , arg1 , arg2 , arg3 , arg4 , arg5 , arg6 );
	fprintf( stderr , "\n" );
	if ( fatal == FATAL ) {
	    exit( 2 );
	}
	errors = 1;
    }

    /*
     *	find the last modify time of a file.
     *	on error, return the current time.
     */
time_t
mtime( filename )
    char	*filename;
    {
	struct stat	filestat;

#	ifdef DEBUG
	    fprintf( stderr , "[mtime] filename %s\n"
		    , filename );
#	endif DEBUG
	if ( stat( filename , &filestat ) != 0 ) {
	    error( WARNING , "%s: cannot open" , filename );
	    return ( (time_t) time( 0 ) );
	}
	return filestat.st_mtime;
    }

char *
classify( type )
    unsigned char	type;
    {
	switch ( type ) {
	    case N_PSO:
		return "source file";
	    case N_PSOL:
		return "include file";
	    case N_PGLABEL:
		return "label";
	    case N_PGCONST:
		return "constant";
	    case N_PGTYPE:
		return "type";
	    case N_PGVAR:
		return "variable";
	    case N_PGFUNC:
		return "function";
	    case N_PGPROC:
		return "procedure";
	    case N_PEFUNC:
		return "external function";
	    case N_PEPROC:
		return "external procedure";
	    default:
		return "unknown symbol";
	}
    }
