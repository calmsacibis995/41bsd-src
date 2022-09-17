static	char *sccsid = "@(#)errormain.c	1.2 (Berkeley) 10/16/80";
#include <stdio.h>
#include <ctype.h>
#include <signal.h>
#include "error.h"

int	nerrors = 0;
struct	error_desc	*er_head;
struct	error_desc	**errors;	

int	nfiles = 0;
struct	error_desc	***files;	/* array of pointers into errors*/
int	language = INCC;

char	*currentfilename = "????";
char	*processname;
char	*im_on;			/* my tty name */

boolean	query = FALSE;		/* query the operator if touch files */
boolean	notouch = FALSE;	/* don't touch ANY files */
boolean	piflag	= FALSE;	/* this is not pi */

char	*suffixlist = ".*";	/* initially, can touch any file */

int	errorsort();
int	onintr();
/*
 *	error [-I ignorename] [-n] [-q] [-t suffixlist] [-s] [-v] [infile]
 *	
 *	-I:	the following name, `ignorename' contains a list of
 *		function names that are not to be treated as hard errors.
 *		Default: ~/.errorsrc
 *
 *	-n:	don't touch ANY files!
 *
 *	-q:	The user is to be queried before touching each
 *		file; if not specified, all files with hard, non
 *		ignorable errors are touched (assuming they can be).
 *
 *	-t:	touch only files ending with the list of suffices, each
 *		suffix preceded by a dot.
 *		eg, -t .c.y.l
 *		will touch only files ending with .c, .y or .l
 *
 *	-s:	print a summary of the error's categories.
 *
 *	-v:	after touching all files, overlay vi(1), ex(1) or ed(1)
 *		on top of error, entered in the first file with
 *		an error in it, with the appropriate editor
 *		set up to use the "next" command to get the other
 *		files containing errors.
 *
 *	-p:	(obsolete: for older versions of pi without bug
 *		fix regarding printing out the name of the main file
 *		with an error in it)
 *		Take the following argument and use it as the name of
 *		the pascal source file, suffix .p
 *
 *	-E:	show the errors in sorted order; intended for
 *		debugging.
 *
 *	-S:	show the errors in unsorted order
 *		(as they come from the error file)
 *
 *	infile:	The error messages come from this file.
 *		Default: stdin
 */
main(argc, argv)
	int	argc;
	char	*argv[];
{
	char	*cp;
	char	*ignorename = 0;
	int	ed_argc;
	char	**ed_argv;		/*return from touchfiles*/
	boolean	show_errors = FALSE;
	boolean	Show_Errors = FALSE;
	boolean	pr_summary = FALSE;
	boolean	edit_files = FALSE;

	processname = argv[0];

	errorfile = stdin;
	if (argc > 1){
		for(; (argc > 1) && (argv[1][0] == '-'); argc--, argv++){
			for (cp = argv[1] + 1; *cp; cp++){
				switch(*cp){
					default:
						fprintf(stderr, "%s: -%c: Unknown flag\n",
							processname, *cp);
						break;
					case 'n':	/* no touch */
						notouch = TRUE;
						break;
					case 'q':	/* query */
						query = TRUE;
						break;
					case 'S':
						Show_Errors = TRUE;
						break;
					case 's':	/* show summary */
						pr_summary = TRUE;
						break;
					case 'v':	/* edit files */
						edit_files = TRUE;
						break;
#ifndef ERNIE
					case 'p':
						*cp-- = 0; argv++; argc--;
						if (argc > 1){
							currentfilename=argv[1];
							piflag = TRUE;
						}
						break;
#endif
					case 't':
						*cp-- = 0; argv++; argc--;
						if (argc > 1){
							suffixlist = argv[1];
						}
						break;
					case 'I':	/*ignore file name*/
						*cp-- = 0; argv++; argc--;
						if (argc > 1)
							ignorename = argv[1];
						break;
				}	/*end of the argument switch*/
			}	/*end of loop to consume characters after '-'*/
		}	
	}	/* end of being at least one argument */
	if (notouch)
		suffixlist = 0;
	if (argc > 1){
		if (argc > 3){
			fprintf(stderr, "%s: Only takes 0 or 1 arguments\n",
				processname);
			exit(3);
		}
		if ( (errorfile = fopen(argv[1], "r")) == NULL){
			fprintf(stderr, "%s: %s: No such file or directory for reading errors.\n",
				processname, argv[1]);
			exit(4);
		}
	}
	im_on = "/dev/tty";
	if ( (queryfile = fopen(im_on, "r")) == NULL){
		fprintf(stderr,"%s: Can't open \"%s\" to query the user.\n",
			processname, im_on);
		exit(9);
	}
	if (signal(SIGINT, onintr) == SIG_IGN)
		signal(SIGINT, SIG_IGN);
	if (signal(SIGTERM, onintr) == SIG_IGN)
		signal(SIGTERM, SIG_IGN);
	getignored(ignorename);
	eaterrors(&nerrors, &errors);
	if (Show_Errors)
		printerrors(TRUE, nerrors, errors);
	qsort(errors, nerrors, sizeof (struct error_desc *), errorsort);
	if (show_errors)
		printerrors(FALSE, nerrors, errors);
	findfiles(nerrors, errors, &nfiles, &files);
#define P(msg, arg) fprintf(stdout, msg, arg)
	if (pr_summary){
	    if (nunknown)
	      P("%d Errors are unclassifiable.\n", nunknown);
	    if (nignore)
	      P("%d Errors are classifiable, but totally discarded.\n",nignore);
	    if (nsyncerrors)
	      P("%d Errors are synchronization errors.\n", nsyncerrors);
	    if (nignore)
	      P("%d Errors are discarded because they refer to sacrosinct files.\n", ndiscard);
	    if (nnulled)
	      P("%d Errors are nulled because they refer to specific functions.\n", nnulled);
	    if (nnonspec)
	      P("%d Errors are not specific to any file.\n", nnonspec);
	    if (nthisfile)
	      P("%d Errors are specific to a given file, but not to a line.\n", nthisfile);
	    if (ntrue)
	      P("%d Errors are true errors, and can be inserted into the files.\n", ntrue);
	}
	filenames(nfiles, files);
	fflush(stdout);
	if (touchfiles(nfiles, files, &ed_argc, &ed_argv) && edit_files){
		if (!query || 
			inquire("Do you still want to edit the files you touched? ")){
			/*
			 *	ed_agument's first argument is
			 *	a vi/ex compatabile search argument
			 *	to find the first occurance of ###
			 */
			try("vi", ed_argc, ed_argv);
			try("ex", ed_argc, ed_argv);
			try("ed", ed_argc-1, ed_argv+1);
			fprintf(stdout, "Can't find any editors.\n");
		}
	}
}

try(name, argc, argv)
	char	*name;
	int	argc;
	char	**argv;
{
	argv[0] = name;
	wordvprint(stdout, argc, argv);
	fprintf(stdout, "\n");
	fflush(stderr);
	fflush(stdout);
	sleep(2);
	if (freopen(im_on, "r", stdin) == NULL)
		return;
	if (freopen(im_on, "w", stdout) == NULL)
		return;
	execvp(name, argv);
}

int errorsort(epp1, epp2)
	struct	error_desc	**epp1, **epp2;
{
	register	struct	error_desc	*ep1, *ep2;
			int	order;
	/*
	 *	Sort by:
	 *	1)	synchronization, non specific, discarded errors first;
	 *	2)	nulled and true errors last
	 *		a)	grouped by similar file names
	 *			1)	grouped in ascending line number
	 */
	ep1 = *epp1; ep2 = *epp2;
	if (ep1 == 0 || ep2 == 0)
		return(0);
	if ( (NOTSORTABLE(ep1->error_e_class)) ^ (NOTSORTABLE(ep2->error_e_class))){
		return(NOTSORTABLE(ep1->error_e_class) ? -1 : 1);
	}
	if (NOTSORTABLE(ep1->error_e_class))	/* then both are */
		return(ep1->error_no - ep2->error_no);
	order = strcmp(ep1->error_text[0], ep2->error_text[0]);
	if (order == 0){
		return(ep1->error_line - ep2->error_line);
	}
	return(order);
}