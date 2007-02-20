case $CONFIG in
'')
	if test -f config.sh; then TOP=.;
	elif test -f ../config.sh; then TOP=..;
	elif test -f ../../config.sh; then TOP=../..;
	elif test -f ../../../config.sh; then TOP=../../..;
	elif test -f ../../../../config.sh; then TOP=../../../..;
	else
		echo "Can't find config.sh."; exit 1
	fi
	. $TOP/config.sh
	;;
esac
case "$0" in
*/*) cd `expr X$0 : 'X\(.*\)/'` ;;
esac
echo "Extracting config.h (with variable substitutions)"
sed <<!GROK!THIS! >config.h -e 's!^#undef\(.*/\)\*!/\*#define\1 \*!' -e 's!^#un-def!#undef!'
/*
 * This file was produced by running the config_h.SH script, which
 * gets its values from config.sh, which is generally produced by
 * running Configure.
 *
 * Feel free to modify any of this as the need arises.  Note, however,
 * that running config_h.SH again will wipe out any changes you've made.
 * For a more permanent change edit config.sh and rerun config_h.SH.
 *
 * \$Id: Config_h.U,v 3.0.1.5 1997/02/28 14:57:43 ram Exp $
 */

/*
 * Package name      : $package
 * Source directory  : $src
 * Configuration time: $cf_time
 * Configured by     : $cf_by
 * Target system     : $myuname
 */

#ifndef _config_h_
#define _config_h_

/* getdtablesize:
 *	This catches use of the getdtablesize() subroutine, and remaps it
 *	to either ulimit(4,0) or NOFILE, if getdtablesize() isn't available.
 */
#$d_gettblsz getdtablesize() $tablesize	/**/

/* HAS_BCOPY:
 *	This symbol is defined if the bcopy() routine is available to
 *	copy blocks of memory.
 */
#$d_bcopy HAS_BCOPY	/**/

/* HAS_BZERO:
 *	This symbol is defined if the bzero() routine is available to
 *	set a memory block to 0.
 */
#$d_bzero HAS_BZERO	/**/

/* HASCONST:
 *	This symbol, if defined, indicates that this C compiler knows about
 *	the const type. There is no need to actually test for that symbol
 *	within your programs. The mere use of the "const" keyword will
 *	trigger the necessary tests.
 */
#$d_const HASCONST	/**/
#ifndef HASCONST
#define const
#endif

/* HAS_GETPRIORITY:
 *	This symbol, if defined, indicates that the getpriority routine is
 *	available to get a process's priority.
 */
#$d_getprior HAS_GETPRIORITY		/**/

/* INTERNET:
 *	This symbol, if defined, indicates that there is a mailer available
 *	which supports internet-style addresses (user@site.domain).
 */
#$d_internet	INTERNET	/**/

/* HAS_ITIMER:
 *	This symbol, if defined, indicates that the setitimer() routine exists.
 */
#$d_itimer HAS_ITIMER	/**/

/* HAS_MEMSET:
 *	This symbol, if defined, indicates that the memset routine is available
 *	to set blocks of memory.
 */
#$d_memset HAS_MEMSET	/**/

/* HAS_RENAME:
 *	This symbol, if defined, indicates that the rename routine is available
 *	to rename files.  Otherwise you should do the unlink(), link(), unlink()
 *	trick.
 */
#$d_rename HAS_RENAME	/**/

/* HAS_GETRUSAGE:
 *	This symbol, if defined, indicates that the getrusage() routine is
 *	available to get process statistics with a sub-second accuracy.
 *	Inclusion of <sys/resource.h> and <sys/time.h> may be necessary.
 */
#$d_rusage HAS_GETRUSAGE		/**/

/* HAS_SELECT:
 *	This symbol, if defined, indicates that the select routine is
 *	available to select active file descriptors. If the timeout field
 *	is used, <sys/time.h> may need to be included.
 */
#$d_select HAS_SELECT	/**/

/* HAS_SETLOCALE:
 *	This symbol, if defined, indicates that the setlocale routine is
 *	available to handle locale-specific ctype implementations.
 */
#$d_setlocale HAS_SETLOCALE	/**/

/* HAS_SETPGID:
 *	This symbol, if defined, indicates that the setpgid(pid, gpid)
 *	routine is available to set process group ID.
 */
#$d_setpgid HAS_SETPGID	/**/

/* HAS_SETPGRP:
 *	This symbol, if defined, indicates that the setpgrp routine is
 *	available to set the current process group.
 */
/* USE_BSD_SETPGRP:
 *	This symbol, if defined, indicates that setpgrp needs two
 *	arguments whereas USG one needs none.  See also HAS_SETPGID
 *	for a POSIX interface.
 */
#$d_setpgrp HAS_SETPGRP		/**/
#$d_bsdsetpgrp USE_BSD_SETPGRP	/**/

/* HAS_SETPRIORITY:
 *	This symbol, if defined, indicates that the setpriority routine is
 *	available to set a process's priority.
 */
#$d_setprior HAS_SETPRIORITY		/**/

/* HAS_SIGACTION:
 *	This symbol, if defined, indicates that Vr4's sigaction() routine
 *	is available.
 */
#$d_sigaction HAS_SIGACTION	/**/

/* HAS_SOCKET:
 *	This symbol, if defined, indicates that the BSD socket interface is
 *	supported.
 */
/* HAS_SOCKETPAIR:
 *	This symbol, if defined, indicates that the BSD socketpair() call is
 *	supported.
 */
#$d_socket HAS_SOCKET		/**/
#$d_sockpair HAS_SOCKETPAIR	/**/

/* HAS_STRCASECMP:
 *	This symbol, if defined, indicates that the strcasecmp() routine is
 *	available for case-insensitive string compares.
 */
#$d_strccmp HAS_STRCASECMP	/**/

/* HAS_STRDUP:
 *	This symbol, if defined, indicates that the strdup routine is
 *	available to duplicate strings in memory. Otherwise, roll up
 *	your own...
 */
#$d_strdup HAS_STRDUP		/**/

/* HAS_SYSCONF:
 *	This symbol, if defined, indicates that sysconf() is available
 *	to determine system related limits and options.
 */
#$d_sysconf HAS_SYSCONF	/**/

/* VOIDSIG:
 *	This symbol is defined if this system declares "void (*signal(...))()" in
 *	signal.h.  The old way was to declare it as "int (*signal(...))()".  It
 *	is up to the package author to declare things correctly based on the
 *	symbol.
 */
/* Signal_t:
 *	This symbol's value is either "void" or "int", corresponding to the
 *	appropriate return type of a signal handler.  Thus, you can declare
 *	a signal handler using "Signal_t (*handler)()", and define the
 *	handler using "Signal_t handler(sig)".
 */
#$d_voidsig VOIDSIG 	/**/
#define Signal_t $signal_t	/* Signal handler's return type */

/* HASVOLATILE:
 *	This symbol, if defined, indicates that this C compiler knows about
 *	the volatile declaration.
 */
#$d_volatile	HASVOLATILE	/**/
#ifndef HASVOLATILE
#define volatile
#endif

/* HAS_WAITPID:
 *	This symbol, if defined, indicates that the waitpid routine is
 *	available to wait for child process.
 */
#$d_waitpid HAS_WAITPID	/**/

/* I_ARPA_INET:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <arpa/inet.h> to get inet_addr and friends declarations.
 */
#$i_arpainet	I_ARPA_INET		/**/

/* I_FCNTL:
 *	This manifest constant tells the C program to include <fcntl.h>.
 */
#$i_fcntl I_FCNTL	/**/

/* I_LIMITS:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <limits.h> to get definition of symbols like WORD_BIT or
 *	LONG_MAX, i.e. machine dependant limitations.
 */
#$i_limits I_LIMITS		/**/

/* I_LOCALE:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <locale.h>.
 */
#$i_locale	I_LOCALE		/**/

/* I_MALLOC:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <malloc.h>.
 */
#$i_malloc I_MALLOC		/**/

/* I_NETINET_IN:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <netinet/in.h>. Otherwise, you may try <sys/in.h>.
 */
/* I_SYS_IN:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <sys/in.h> instead of <netinet/in.h>.
 */
#$i_niin I_NETINET_IN	/**/
#$i_sysin I_SYS_IN		/**/

/* I_STDDEF:
 *	This symbol, if defined, indicates that <stddef.h> exists and should
 *	be included.
 */
#$i_stddef I_STDDEF	/**/

/* I_STDLIB:
 *	This symbol, if defined, indicates that <stdlib.h> exists and should
 *	be included.
 */
#$i_stdlib I_STDLIB		/**/

/* I_STRING:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <string.h> (USG systems) instead of <strings.h> (BSD systems).
 */
#$i_string I_STRING		/**/

/* I_SYS_FILE:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <sys/file.h> to get definition of R_OK and friends.
 */
#$i_sysfile I_SYS_FILE		/**/

/* I_SYS_MMAN:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <sys/mman.h>.
 */
#$i_sysmman	I_SYS_MMAN		/**/

/* I_SYS_PARAM:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <sys/param.h>.
 */
#$i_sysparam I_SYS_PARAM		/**/

/* I_SYS_RESOURCE:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <sys/resource.h>.
 */
#$i_sysresrc I_SYS_RESOURCE		/**/

/* I_SYS_SELECT:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <sys/select.h> in order to get definition of struct timeval.
 */
#$i_sysselct I_SYS_SELECT	/**/

/* I_SYS_SOCKET:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <sys/socket.h> before performing socket calls.
 */
#$i_syssock I_SYS_SOCKET		/**/

/* I_SYS_STAT:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <sys/stat.h>.
 */
#$i_sysstat	I_SYS_STAT		/**/

/* I_SYS_TYPES:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <sys/types.h>.
 */
#$i_systypes	I_SYS_TYPES		/**/

/* I_SYS_WAIT:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <sys/wait.h>.
 */
#$i_syswait I_SYS_WAIT	/**/

/* I_TIME:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <time.h>.
 */
/* I_SYS_TIME:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <sys/time.h>.
 */
#$i_time I_TIME		/**/
#$i_systime I_SYS_TIME		/**/

/* I_UNISTD:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <unistd.h>.
 */
#$i_unistd I_UNISTD		/**/

/* I_VALUES:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <values.h> to get definition of symbols like MINFLOAT or
 *	MAXLONG, i.e. machine dependant limitations.  Probably, you
 *	should use <limits.h> instead, if it is available.
 */
#$i_values I_VALUES		/**/

/* Free_t:
 *	This variable contains the return type of free().  It is usually
 * void, but occasionally int.
 */
/* Malloc_t:
 *	This symbol is the type of pointer returned by malloc and realloc.
 */
#define Malloc_t $malloctype			/**/
#define Free_t $freetype			/**/

/* MYHOSTNAME:
 *	This symbol contains name of the host the program is going to run on.
 *	The domain is not kept with hostname, but must be gotten from MYDOMAIN.
 *	The dot comes with MYDOMAIN, and need not be supplied by the program.
 *	If gethostname() or uname() exist, MYHOSTNAME may be ignored. If MYDOMAIN
 *	is not used, MYHOSTNAME will hold the name derived from PHOSTNAME.
 */
#define MYHOSTNAME "$myhostname"		/**/

/* Pid_t:
 *	This symbol holds the type used to declare process ids in the kernel.
 *	It can be int, uint, pid_t, etc... It may be necessary to include
 *	<sys/types.h> to get any typedef'ed information.
 */
#define Pid_t $pidtype		/* PID type */

/* CAN_PROTOTYPE:
 *	If defined, this macro indicates that the C compiler can handle
 *	function prototypes.
 */
/* _:
 *	This macro is used to declare function parameters for folks who want
 *	to make declarations with prototypes using a different style than
 *	the above macros.  Use double parentheses.  For example:
 *
 *		int main _((int argc, char *argv[]));
 */
#$prototype	CAN_PROTOTYPE	/**/
#ifdef CAN_PROTOTYPE
#define	_(args) args
#else
#define	_(args) ()
#endif

/* Size_t:
 *	This symbol holds the type used to declare length parameters
 *	for string functions.  It is usually size_t, but may be
 *	unsigned long, int, etc.  It may be necessary to include
 *	<sys/types.h> to get any typedef'ed information.
 */
#define Size_t $sizetype	 /* length paramater for string functions */

/* VOIDFLAGS:
 *	This symbol indicates how much support of the void type is given by this
 *	compiler.  What various bits mean:
 *
 *	    1 = supports declaration of void
 *	    2 = supports arrays of pointers to functions returning void
 *	    4 = supports comparisons between pointers to void functions and
 *		    addresses of void functions
 *	    8 = suports declaration of generic void pointers
 *
 *	The package designer should define VOIDUSED to indicate the requirements
 *	of the package.  This can be done either by #defining VOIDUSED before
 *	including config.h, or by defining defvoidused in Myinit.U.  If the
 *	latter approach is taken, only those flags will be tested.  If the
 *	level of void support necessary is not present, defines void to int.
 */
#ifndef VOIDUSED
#define VOIDUSED $defvoidused
#endif
#define VOIDFLAGS $voidflags
#if (VOIDFLAGS & VOIDUSED) != VOIDUSED
#define void int		/* is void to be avoided? */
#define M_VOID			/* Xenix strikes again */
#endif

/* WIN32_CDECL:
 *	Defined as __cdecl if the compiler can handle that keyword to specify
 *	C-style argument passing conventions. This allows MS VC++
 *	on Win32 to use the __fastcall convention for everything else
 *	and get a performance boost. Any compiler with a brain (read:
 *	not MS VC) handles this optimization automatically without such a
 *	kludge. On these systems, this is defined as nothing.
 */
#define WIN32_CDECL $cdecl

/* CAN_TAKE_ARGS_IN_FP:
 *	Defined if the compiler prefers that function pointer parameters
 *	in prototypes include the function's arguments, rather than 
 *	nothing (that is, int (*fun)(int) rather than int(*fun)().
 */
#$d_argsinfp CAN_TAKE_ARGS_IN_FP /**/

/* HAS_ASSERT:
 *	If defined, this system has the assert() macro.
 */
#$d_assert HAS_ASSERT	/**/

/* HASATTRIBUTE:
 *	This symbol indicates the C compiler can check for function attributes,
 *	such as printf formats. This is normally only supported by GNU cc.
 */
#$d_attribut HASATTRIBUTE 	/**/
#ifndef HASATTRIBUTE
#define __attribute__(_arg_)
#endif

/* HAS_BINDTEXTDOMAIN:
 *	Defined if bindtextdomain is available().
 */
#$d_bindtextdomain HAS_BINDTEXTDOMAIN /**/

/* HAS_CRYPT:
 *	This symbol, if defined, indicates that the crypt routine is available
 *	to encrypt passwords and the like.
 */
/* I_CRYPT:
 *	This symbol, if defined, indicates that <crypt.h> can be included.
 */
#$d_crypt HAS_CRYPT		/**/

#$i_crypt I_CRYPT		/**/

/* FORCE_IPV4:
 *	If defined, this system will not use IPv6. Necessary for Openbsd.
 */
#$d_force_ipv4 FORCE_IPV4	/**/

/* HAS_FPSETROUND:
 *	This symbol, if defined, indicates that the crypt routine is available
 *	to encrypt passwords and the like.
 */
/* I_FLOATINGPOINT:
 *	This symbol, if defined, indicates that <crypt.h> can be included.
 */
#$d_fpsetround HAS_FPSETROUND		/**/

#$d_fpsetmask HAS_FPSETMASK		/**/

#$i_floatingpoint I_FLOATINGPOINT		/**/

/* HAS_GAI_STRERROR:
 *	This symbol, if defined, indicates that getaddrinfo()'s error cores
 * can be converted to strings for printing.
 */
#$d_gaistr HAS_GAI_STRERROR		/**/

/* HAS_GETADDRINFO:
 *	This symbol, if defined, indicates that the getaddrinfo() routine is
 *	available to lookup internet addresses in some data base or other.
 */
#$d_getadinf HAS_GETADDRINFO		/**/

/* HAS_GETDATE:
 *	This symbol, if defined, indicates that the getdate() routine is
 *	available to convert date strings into struct tm's.
 */
#$d_getdate HAS_GETDATE		/**/

/* HAS_GETHOSTBYNAME2:
 *	This symbol, if defined, indicates that the gethostbyname2() 
 * function is available to resolve hostnames.
 */
#$d_gethbynm2 HAS_GETHOSTBYNAME2		/**/

/* HAS_GETNAMEINFO:
 *	This symbol, if defined, indicates that the getnameinfo() routine is
 *	available to lookup host names in some data base or other.
 */
#$d_getnminf HAS_GETNAMEINFO		/**/

/* HAS_GETPAGESIZE:
 *	This symbol, if defined, indicates that the getpagesize system call
 *	is available to get system page size, which is the granularity of
 *	many memory management calls.
 */
/* PAGESIZE_VALUE:
 *	This symbol holds the size in bytes of a system page (obtained via
 *	the getpagesize() system call at configuration time or asked to the
 *	user if the system call is not available).
 */
#$d_getpagsz HAS_GETPAGESIZE     /**/
#define PAGESIZE_VALUE $pagesize /* System page size, in bytes */

/* HAS_GETTEXT:
 *	Defined if gettext is available().
 */
#$d_gettext HAS_GETTEXT /**/

/* HAS_HUGE_VAL:
 *	If defined, this system has the HUGE_VAL constant. We like this,
 *	and don't bother defining the other floats below if we find it.
 */
/* HAS_HUGE:
 *	If defined, this system has the HUGE constant. We like this, and
 *	don't bother defining the other floats below if we find it.
 */
/* HAS_INT_MAX:
 *	If defined, this system has the INT_MAX constant. 
 */
/* HAS_MAXINT:
 *	If defined, this system has the MAXINT constant. 
 */
/* HAS_MAXDOUBLE:
 *	If defined, this system has the MAXDOUBLE constant. 
 */
#$d_huge_val HAS_HUGE_VAL	/**/

#$d_huge HAS_HUGE	/**/

#$d_int_max HAS_INT_MAX	/**/

#$d_maxint HAS_MAXINT	/**/

#$d_maxdouble HAS_MAXDOUBLE	/**/

/* HAS_IEEE_MATH:
 *	Defined if the machine supports IEEE math - that is, can safely
 *	return NaN or Inf rather than crash on bad math.
 */
#$d_ieee HAS_IEEE_MATH /**/

/* HAS_INET_PTON:
 *	This symbol, if defined, indicates that the inet_pton() and
 *     inet_ntop() routines are available to convert IP addresses..
 */
#$d_in2p HAS_INET_PTON		/**/

/* HAS_IPV6:
 *	If defined, this system has the sockaddr_in6 struct and AF_INET6.
 * We can't rely on just AF_INET6 being defined.
 */
#$d_ipv6 HAS_IPV6	/**/

/* SIGNALS_KEPT:
 *	This symbol is defined if signal handlers needn't be reinstated after
 *	receipt of a signal.
 */
#$d_keepsig SIGNALS_KEPT	/**/

/* HAS_MEMCPY:
 *	This symbol, if defined, indicates that the memcpy routine is available
 *	to copy blocks of memory. If not, it will be mapped to bcopy
 *	in confmagic.h
 */
/* HAS_MEMMOVE:
 *	This symbol, if defined, indicates that the memmove routine is available
 *	to copy blocks of memory. If not, it will be mapped to bcopy
 */
#$d_memcpy HAS_MEMCPY	/**/

#$d_memmove HAS_MEMMOVE	/**/

/* CAN_NEWSTYLE:
 *	Defined if new-style function definitions are allowable.
 *	If they are, we can avoid some warnings that you get if
 *	you declare char arguments in a prototype and use old-style
 *	function definitions, which implicitly promote them to ints.
 */
#$d_newstyle CAN_NEWSTYLE /**/

/* HAS_RANDOM:
 *	Have we got random(), our first choice for number generation?
 */
/* HAS_LRAND48:
 *	Have we got lrand48(), our second choice?
 */
/* HAS_RAND:
 *	Have we got rand(), our last choice?
 */
#$d_random HAS_RANDOM	/**/
#$d_lrand48 HAS_LRAND48	/**/
#$d_rand HAS_RAND	/**/

/* HAS_GETRLIMIT:
 *	This symbol, if defined, indicates that the getrlimit() routine is
 *	available to get resource limits. Probably means setrlimit too.
 *	Inclusion of <sys/resource.h> and <sys/time.h> may be necessary.
 */
#$d_rlimit HAS_GETRLIMIT		/**/

/* SENDMAIL:
 *	This symbol contains the full pathname to sendmail.
 */
/* HAS_SENDMAIL:
 *	If defined, we have sendmail.
 */
#$d_sendmail HAS_SENDMAIL	/**/
#define SENDMAIL	"$sendmail"

/* HAS_SIGCHLD:
 *	If defined, this system has the SIGCHLD constant. 
 */
/* HAS_SIGCLD:
 *	If defined, this system has the SIGCLD constant (SysVish SIGCHLD).
 */
#$d_sigchld HAS_SIGCHLD	/**/

#$d_sigcld HAS_SIGCLD	/**/

/* CAN_PROTOTYPE_SIGNAL:
 *	This symbol is defined if we can safely prototype our rewritten
 *	signal() function as:
 *	Signal_t(*Sigfunc) _((int));
 *	extern Sigfunc signal _((int signo, Sigfunc func));
 */
#$d_signalproto CAN_PROTOTYPE_SIGNAL	/**/

/* HAS_SIGPROCMASK:
 *	This symbol, if defined, indicates that POSIX's sigprocmask() routine
 *	is available.
 */
#$d_sigprocmask HAS_SIGPROCMASK	/**/

/* HAS_SNPRINTF:
 *	This symbol, if defined, indicates that the snprintf routine is 
 *	available. If not, we use sprintf, which is less safe.
 */
#$d_snprintf HAS_SNPRINTF	/**/

/* HAS_SOCKLEN_T:
 *	If defined, this system has the socklen_t type.
 */
#$d_socklen HAS_SOCKLEN_T	/**/

/* HAS_STRCHR:
 *	This symbol is defined to indicate that the strchr()/strrchr()
 *	functions are available for string searching. If not, try the
 *	index()/rindex() pair.
 */
/* HAS_INDEX:
 *	This symbol is defined to indicate that the index()/rindex()
 *	functions are available for string searching.
 */
#$d_strchr HAS_STRCHR	/**/
#$d_index HAS_INDEX	/**/

/* HAS_STRCOLL:
 *	This symbol, if defined, indicates that the strcoll routine is
 *	available to compare strings using collating information.
 */
#$d_strcoll HAS_STRCOLL	/**/

/* HAS_STRXFRM:
 *	This symbol, if defined, indicates that the strxfrm routine is
 *	available to transform strings using collating information.
 */
#$d_strxfrm HAS_STRXFRM	/**/

/* HAS_TCL:
 *	This symbol, if defined, indicates that the tcl library is available
 */
/* I_TCL:
 *	This symbol, if defined, indicates that <tcl.h> can be included.
 */
/* HAS_TCL:
 *  This symbol, if defined, means we have the tcl library
 */
#$d_tcl HAS_TCL		/**/

/* I_TCL:
 *  This symbol, if defined, means we have the <tcl.h> include file
 */
#$i_tcl I_TCL		/**/

/* HAS_TEXTDOMAIN:
 *	Defined if textdomain is available().
 */
#$d_textdomain HAS_TEXTDOMAIN /**/

/* HAS_TIMELOCAL:
 *	This symbol, if defined, indicates that the timelocal() routine is
 *	available.
 */
#$d_timelocal HAS_TIMELOCAL		/**/

/* HAS_SAFE_TOUPPER:
 *	Defined if toupper() can operate safely on any ascii character.
 *	Some systems only allow toupper() on lower-case ascii chars.
 */
#$d_toupper HAS_SAFE_TOUPPER /**/

/* HAS_UINT32_T:
 *	If defined, this system has the uint32_t type.
 */
#$d_uint32_t HAS_UINT32_T	/**/

/* UPTIME_PATH:
 *	This symbol gives the full path to the uptime(1) program if
 *	it exists on the system. If not, this symbol is undefined.
 */
/* HAS_UPTIME:
 *	This symbol is defined if uptime(1) is available.
 */
#$d_uptime HAS_UPTIME	/**/
#define UPTIME_PATH "$uptime"

/* HAS_DEV_URANDOM:
 *	This symbol is defined if /dev/urandom is available.
 */
#$d_urandom HAS_DEV_URANDOM	/**/

/* UNION_WAIT:
 *	This symbol if defined indicates to the C program that the argument
 *	for the wait() system call should be declared as 'union wait status'
 *	instead of 'int status'. You probably need to include <sys/wait.h>
 *	in the former case (see I_SYSWAIT).
 */
#$d_uwait UNION_WAIT		/**/

/* HAS_VSNPRINTF:
 *	This symbol, if defined, indicates that the vsnprintf routine is 
 *	available to printf with a pointer to an argument list.  If not, you
 *	may need to write your own, probably in terms of _doprnt().
 */
#$d_vsnprintf HAS_VSNPRINTF	/**/

/* I_ARPA_NAMESER:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <arpa/nameser.h> to get nameser_addr and friends declarations.
 */
#$i_arpanameser	I_ARPA_NAMESER		/**/

/* I_ERRNO:
 *	This symbol, if defined, indicates to the C program that it can
 *	include <errno.h>.
 */
/* I_SYS_ERRNO:
 *	This symbol, if defined, indicates to the C program that it can
 *	include <sys/errno.h>.
 */
#$i_errno I_ERRNO		/**/

#$i_syserrno I_SYS_ERRNO		/**/

/* I_LIBINTL:
 *	This symbol, if defined, indicates to the C program that it can
 *	include <libintl.h>.
 */
#$i_libintl I_LIBINTL		/**/

/* I_MEMORY:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <memory.h>.
 */
#$i_memory I_MEMORY		/**/

/* I_NETDB:
 *	This symbol, if defined, indicates to the C program that it can
 *	include <errno.h>.
 */
#$i_netdb I_NETDB		/**/

/* I_NETINET_TCP:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <netinet/tcp.h>.
 */
#$i_nitcp I_NETINET_TCP	/**/

/* I_SETJMP:
 *	This symbol, if defined, indicates to the C program that it can
 *	include <setjmp.h> and have things work right.
 */
#$i_setjmp I_SETJMP		/**/

/* I_STDINT:
 *	This symbol, if defined, indicates that the program should include
 *	<stdint.h>.
 */
#$i_stdint I_STDINT		/**/

/* USE_TIOCNOTTY:
 *	This symbol, if defined indicate to the C program that the ioctl()
 *	call with TIOCNOTTY should be used to void tty association.
 *	Otherwise (on USG probably), it is enough to close the standard file
 *	decriptors and do a setpgrp().
 */
#$d_voidtty USE_TIOCNOTTY	/**/

/* I_SYS_PAGE:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <sys/page.h>.
 */
#$i_syspage I_SYS_PAGE		/**/

/* I_SYS_VLIMIT:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <sys/vlimit.h>.
 */
#$i_sysvlimit I_SYS_VLIMIT		/**/

/* I_STDARG:
 *	This symbol, if defined, indicates that <stdarg.h> exists and should
 *	be included.
 */
#$i_stdarg I_STDARG		/**/

/* HAS_MYSQL:
 *     Defined if mysql client libraries are available.
 */
#$d_mysql HAS_MYSQL /**/

/* HAS_OPENSSL:
 *     Defined if openssl 0.9.6+ is available.
 */
#$d_openssl HAS_OPENSSL /**/

/* CAN_KEEPALIVE:
 *	This symbol if defined indicates to the C program that the SO_KEEPALIVE
 *	option of setsockopt() will work as advertised in the manual.
 */
/* CAN_KEEPIDLE:
 *	This symbol if defined indicates to the C program that the TCP_KEEPIDLE
 *	option of setsockopt() will work as advertised in the manual.
 */
#$d_keepalive CAN_KEEPALIVE		/**/

#$d_keepidle CAN_KEEPIDLE		/**/

#endif
!GROK!THIS!
