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
: This forces SH files to create target in same directory as SH file.
: This is so that make depend always knows where to find SH derivatives.
case "$0" in
*/*) cd `expr X$0 : 'X\(.*\)/'` ;;
esac
echo "Extracting Makefile (with variable substitutions)"
if test "x$OSTYPE" = "xmsys"; then
  OUTFILES="buildinf netmud"
else
  OUTFILES="buildinf netmud info_slave console"
fi
: This section of the file will have variable substitutions done on it.
: Move anything that needs config subs from !NO!SUBS! section to !GROK!THIS!.
: Protect any dollar signs and backticks that you do not want interpreted
: by putting a backslash in front.  You may delete these comments.
$spitshell >Makefile <<!GROK!THIS!
# Makefile for CobraMUSH

###############################################################################
#         You shouldn't have to change anything in this file.                 #
###############################################################################

OUTFILES=$OUTFILES

!GROK!THIS!

: In the following dollars and backticks do not need the extra backslash.
$spitshell >>Makefile <<'!NO!SUBS!'

# stupid SYS V shell
SHELL=/bin/sh

# for lint target
LINT=lint
LINTFLAGS=-haz $(DEFINES)
LINTFILT=egrep -v '(possible pointer|long assign|not yet im|:$$)'

# Libs
LIBS=$(CLIBS) $(RLIBS) $(ILIBS)

CFLAGS=$(CCFLAGS) $(RDEFS) $(IDEFS)

# Used for protoizing
#.c.o:
#	$(CC) $(CFLAGS) -aux-info $<.X -c $<

# List of C files, used for make depend:
C_FILES=access.c atr_tab.c attrib.c boolexp.c bsd.c bufferq.c \
	chunk.c cmdlocal.c cmds.c \
	command.c compress.c conf.c cque.c create.c cron.c db.c destroy.c  division.c extchat.c \
	extmail.c filecopy.c flaglocal.c flags.c funcrypt.c function.c \
	fundb.c fundiv.c funlist.c funlocal.c funmath.c funmisc.c funstr.c funtime.c \
	funufun.c game.c help.c htab.c ident.c local.c lock.c log.c look.c \
	malias.c match.c memcheck.c move.c mushlua.c mushlua_wrappers.c mycrypt.c mymalloc.c mysocket.c \
	myssl.c notify.c parse.c pcre.c player.c plyrlist.c \
	predicat.c privtab.c prog.o ptab.c rob.c rplog.c services.c set.c shs.c  \
	sig.c speech.c sql.c strdup.c strtree.c  strutil.c tables.c timer.c unparse.c  \
	utils.c version.c warnings.c  wild.c wiz.c

H_FILES = ../config.h ../confmagic.h ../hdrs/ansi.h ../hdrs/atr_tab.h \
	  ../hdrs/attrib.h ../hdrs/boolexp.h ../hdrs/bufferq.h ../hdrs/case.h \
	  ../hdrs/case.h ../hdrs/chunk.h ../hdrs/command.h ../hdrs/conf.h \
	  ../hdrs/copyrite.h ../hdrs/dbdefs.h ../hdrs/dbio.h ../hdrs/extchat.h \
	  ../hdrs/externs.h ../hdrs/extmail.h ../hdrs/flags.h \
	  ../hdrs/function.h ../hdrs/game.h ../hdrs/getpgsiz.h ../hdrs/help.h \
	  ../hdrs/htab.h ../hdrs/htab.h ../hdrs/ident.h ../hdrs/lock.h \
	  ../hdrs/log.h ../hdrs/log.h ../hdrs/malias.h ../hdrs/match.h \
	  ../hdrs/mushdb.h ../hdrs/mushlua.h ../hdrs/mushtype.h \
	  ../hdrs/mymalloc.h ../hdrs/mysocket.h ../hdrs/myssl.h \
	  ../hdrs/parse.h ../hdrs/pcre.h ../hdrs/privtab.h ../hdrs/ptab.h \
	  ../hdrs/strtree.h ../hdrs/version.h ../options.h ../hdrs/division.h ../hdrs/cron.h

# .o versions of above - these are used in the build
COMMON_O_FILES=access.o atr_tab.o attrib.o boolexp.o bufferq.o \
	chunk.o cmdlocal.o cmds.o \
	command.o compress.o conf.o cque.o create.o cron.o db.o destroy.o division.o extchat.o \
	extmail.o filecopy.o flaglocal.o flags.o funcrypt.o function.o \
	fundb.o funlist.o fundiv.o funlocal.o funmath.o funmisc.o funstr.o funtime.o \
	funufun.o game.o help.o htab.o ident.o local.o lock.o log.o look.o \
	malias.o match.o memcheck.o move.o mushlua.o mushlua_wrappers.o mycrypt.o mymalloc.o \
	mysocket.o myssl.o notify.o parse.o pcre.o player.o plyrlist.o predicat.o privtab.o \
	prog.o   ptab.o rob.o rplog.o services.o set.o shs.o sig.o speech.o sql.o  strdup.o \
	strtree.o  strutil.o tables.o timer.o unparse.o utils.o version.o warnings.o \
	wild.o wiz.o

O_FILES=$(COMMON_O_FILES) bsd.o
CONSOLE_O_FILES=$(COMMON_O_FILES) console.o

# This is a dummy target, in case you type 'make' in the source
# directory (likely for emacs users who M-x compile.)
# This means that the top-level make had better specifically 'make all' :)
first:
	(cd ..; make)

all: $(OUTFILES)

netmud: $(O_FILES) 
	@echo "Making netmud."
	-mv -f netmud netmud~
	$(CC) $(LDFLAGS) $(CCFLAGS) -o netmud $(O_FILES) $(LIBS) 

console: $(CONSOLE_O_FILES)
	@echo "Making console."
	-mv -f console console~
	$(CC) $(LDFLAGS) $(CCFLAGS) -o console $(CONSOLE_O_FILES) $(LIBS)

# By default, db.c initially allocates enough space for 5000 objects, then
#   grows the space if needed.  To change this value, include
#   -DDB_INITIAL_SIZE=xxxx where xxxx is the new value (minimum 1).


# We recompile mysocket.c instead of reusing mysocket.o because we
# want to do some error handing differently for info_slave.
info_slave: info_slave.c ident.o strdup.o sig.o mymalloc.o mysocket.c
	@echo "Making info_slave."
	$(CC) $(CCFLAGS) $(IDEFS) -c info_slave.c
	$(CC) $(LDFLAGS) $(CCFLAGS) -DINFOSLAVE -o info_slave info_slave.o \
	ident.o strdup.o sig.o mymalloc.o mysocket.c  $(LIBS)

# ../hdrs/buildinf.h contains build information used by version.c:
# time/date of build and CFLAGS
# It should always be out of date.
buildinf:
	-rm -f ../hdrs/buildinf.h
	@echo "/* This file generated automatically from Makefile */" >> ../hdrs/buildinf.h
	@echo "#define BUILDDATE \"`date`\"" >> ../hdrs/buildinf.h
	@echo "#define COMPILER \"$(CC)\"" >> ../hdrs/buildinf.h
	@echo "#define CCFLAGS \"$(CCFLAGS)\"" >> ../hdrs/buildinf.h
	@echo "#define RDEFS \"$(RDEFS)\"" >> ../hdrs/buildinf.h
	@echo "#define IDEFS \"$(IDEFS)\"" >> ../hdrs/buildinf.h

# If funlocal.c doesn't exist, we want to build it from
# funlocal.dst. 
funlocal.c: funlocal.dst
	@if [ ! -f funlocal.c ]; then \
	  cp funlocal.dst funlocal.c; \
	else \
	  echo "********************************************************"; \
	  echo "NOTE! funlocal.dst has been changed. You may need to incorporate these" ; \
	  echo "changes into your funlocal.c. Edit or touch funlocal.c to prevent this message"; \
	  echo "********************************************************"; \
	fi

flaglocal.c: flaglocal.dst
	@if [ ! -f flaglocal.c ]; then \
	  cp flaglocal.dst flaglocal.c; \
	else \
	  echo "********************************************************"; \
	  echo "NOTE! flaglocal.dst has been changed. You may need to incorporate these" ; \
	  echo "changes into your flaglocal.c. Edit or touch flaglocal.c to prevent this message"; \
	  echo "********************************************************"; \
	fi

cmdlocal.c: cmdlocal.dst
	@if [ ! -f cmdlocal.c ]; then \
	  cp cmdlocal.dst cmdlocal.c; \
	else \
	  echo "********************************************************"; \
	  echo "NOTE! cmdlocal.dst has been changed. You may need to incorporate these" ; \
	  echo "changes into your cmdlocal.c. Edit or touch cmdlocal.c to prevent this message"; \
	  echo "********************************************************"; \
	fi

mushlua_wrappers.c: mushlua.i
	@echo "Generating mushlua_wrappers.c"
	-swig -o mushlua_wrappers.c -lua mushlua.i

local.c: local.dst
	@if [ ! -f local.c ]; then \
	  cp local.dst local.c; \
	else \
	  echo "********************************************************"; \
	  echo "NOTE! local.dst has been changed. You may need to incorporate these" ; \
	  echo "changes into your local.c. Edit or touch local.c to prevent this message"; \
	  echo "********************************************************"; \
	fi

../hdrs/patches.h:
	if [ ! -f ../hdrs/patches.h ]; then \
	  (cd ../utils; sh mkcmds.sh patches); \
	fi

../po/pennmush.pot: $(C_FILES) $(H_FILES)
	xgettext -d pennmush -kT -o ../po/pennmush.pot $(C_FILES) $(H_FILES)

etags: 
	etags *.c ../*.h ../hdrs/*.h

ctags: 
	ctags *.c 

depend: funlocal.c cmdlocal.c local.c flaglocal.c
	makedepend -fMakefile.SH -w10 -- -I../hdrs -I.. $(CFLAGS) -- $(C_FILES) $(H_FILES)
	perl ../utils/fixdepend.pl Makefile.SH

# Requires GNU indent! 
indent:
	(set +e; for file in *.dst *.c  ../hdrs/*.h ; do echo $$file; \
	/usr/bin/expand $$file > tmpfile; mv -f tmpfile $$file; \
	/usr/bin/indent -npro -kr -ci2 -ss -psl -ip4 -i2 -cs -l80 -lc75 \
	-T ATRALIAS -T DESC -T CNode -T CONF -T BQUE -T FUN \
	-T NVAL -T i_rec -T f_rec -T USERFN_ENTRY -T PRIV -T FLAG \
	-T FLAGENT -T FLAG_ALIAS -T tlist -T u -T stat -T tcheck -T ATTR \
	-T ALIST -T CHTAB -T FBLKHDR -T FBLOCK -T OPTTAB -T dbref \
	-T object_flag_type -T channel_type -T boolexp_type -T CHAN \
	-T mail_flag -T help_indx -T lock_type -T lock_list -T MEM \
	-T warn_type -T POWER -T POWER_ALIAS -T acsflag -T StrTree \
	-T switch_mask -T COMLIST -T COMALIAS -T COMMAND -T SWITCH_VALUE \
	-T COMSORTSTRUC -T CHANUSER -T Signal_t -T Sigfunc -T univptr_t  \
	-T __ptr_t -T __malloc_size_t -T HASHENT -T HASHTAB -T COMMAND_INFO \
	-T CHANLIST -T Malloc_t -T Free_t -T MATH -T socklen_t \
	-T StrNode -T IDENT -T Size_t -T Port_t -T help_file -T PTAB \
	-T SHS_INFO -T ansi_string -T MAIL $$file ; done)

clean:
	-rm -f *.o 
	-rm -f a.out core gmon.out $(OUTFILES) 

distclean: clean
	-rm -f *~ *.orig *.rej *.bak funlocal.c cmdlocal.c flaglocal.c local.c \#*

test_compress: comp_h.c
	$(CC) $(CFLAGS) -o test_compress -DSTANDALONE comp_h.c

portmsg: portmsg.c
	$(CC) $(CFLAGS) -o portmsg portmsg.c $(LIBS)

# Some dependencies that make depend doesn't handle well
compress.o: comp_h.c comp_w.c comp_w8.c
version.o: ../hdrs/buildinf.h
cmds.o: ../hdrs/funs.h ../hdrs/cmds.h

# CobraMUSH no-network console
console.o:	bsd.c
	$(CC) $(CFLAGS) -DCOMPILE_CONSOLE -o console.o -c bsd.c

# DO NOT DELETE THIS LINE -- make depend depends on it.

access.o: ../config.h
access.o: ../hdrs/copyrite.h
access.o: ../hdrs/conf.h
access.o: ../options.h
access.o: ../hdrs/mushtype.h
access.o: ../hdrs/htab.h
access.o: ../hdrs/externs.h
access.o: ../hdrs/compile.h
access.o: ../hdrs/dbdefs.h
access.o: ../hdrs/mushdb.h
access.o: ../hdrs/flags.h
access.o: ../hdrs/ptab.h
access.o: ../hdrs/division.h
access.o: ../hdrs/chunk.h
access.o: ../hdrs/bufferq.h
access.o: ../confmagic.h
access.o: ../hdrs/access.h
access.o: ../hdrs/mymalloc.h
access.o: ../hdrs/match.h
access.o: ../hdrs/parse.h
access.o: ../hdrs/log.h
atr_tab.o: ../config.h
atr_tab.o: ../hdrs/conf.h
atr_tab.o: ../hdrs/copyrite.h
atr_tab.o: ../options.h
atr_tab.o: ../hdrs/mushtype.h
atr_tab.o: ../hdrs/htab.h
atr_tab.o: ../hdrs/externs.h
atr_tab.o: ../hdrs/compile.h
atr_tab.o: ../hdrs/dbdefs.h
atr_tab.o: ../hdrs/mushdb.h
atr_tab.o: ../hdrs/flags.h
atr_tab.o: ../hdrs/ptab.h
atr_tab.o: ../hdrs/division.h
atr_tab.o: ../hdrs/chunk.h
atr_tab.o: ../hdrs/bufferq.h
atr_tab.o: ../confmagic.h
atr_tab.o: ../hdrs/attrib.h
atr_tab.o: ../hdrs/boolexp.h
atr_tab.o: ../hdrs/command.h
atr_tab.o: ../hdrs/switches.h
atr_tab.o: ../hdrs/atr_tab.h
atr_tab.o: ../hdrs/privtab.h
atr_tab.o: ../hdrs/mymalloc.h
atr_tab.o: ../hdrs/log.h
atr_tab.o: ../hdrs/parse.h
attrib.o: ../hdrs/copyrite.h
attrib.o: ../config.h
attrib.o: ../hdrs/conf.h
attrib.o: ../options.h
attrib.o: ../hdrs/mushtype.h
attrib.o: ../hdrs/htab.h
attrib.o: ../hdrs/externs.h
attrib.o: ../hdrs/compile.h
attrib.o: ../hdrs/dbdefs.h
attrib.o: ../hdrs/mushdb.h
attrib.o: ../hdrs/flags.h
attrib.o: ../hdrs/ptab.h
attrib.o: ../hdrs/division.h
attrib.o: ../hdrs/chunk.h
attrib.o: ../hdrs/bufferq.h
attrib.o: ../confmagic.h
attrib.o: ../hdrs/attrib.h
attrib.o: ../hdrs/boolexp.h
attrib.o: ../hdrs/command.h
attrib.o: ../hdrs/switches.h
attrib.o: ../hdrs/match.h
attrib.o: ../hdrs/parse.h
attrib.o: ../hdrs/privtab.h
attrib.o: ../hdrs/mymalloc.h
attrib.o: ../hdrs/strtree.h
attrib.o: ../hdrs/lock.h
attrib.o: ../hdrs/log.h
boolexp.o: ../hdrs/copyrite.h
boolexp.o: ../config.h
boolexp.o: ../hdrs/conf.h
boolexp.o: ../options.h
boolexp.o: ../hdrs/mushtype.h
boolexp.o: ../hdrs/htab.h
boolexp.o: ../hdrs/mushdb.h
boolexp.o: ../hdrs/flags.h
boolexp.o: ../hdrs/ptab.h
boolexp.o: ../hdrs/division.h
boolexp.o: ../hdrs/match.h
boolexp.o: ../hdrs/externs.h
boolexp.o: ../hdrs/compile.h
boolexp.o: ../hdrs/dbdefs.h
boolexp.o: ../hdrs/chunk.h
boolexp.o: ../hdrs/bufferq.h
boolexp.o: ../confmagic.h
boolexp.o: ../hdrs/lock.h
boolexp.o: ../hdrs/boolexp.h
boolexp.o: ../hdrs/parse.h
boolexp.o: ../hdrs/attrib.h
boolexp.o: ../hdrs/command.h
boolexp.o: ../hdrs/switches.h
boolexp.o: ../hdrs/log.h
boolexp.o: ../hdrs/extchat.h
boolexp.o: ../hdrs/strtree.h
bsd.o: ../hdrs/copyrite.h
bsd.o: ../config.h
bsd.o: ../hdrs/conf.h
bsd.o: ../options.h
bsd.o: ../hdrs/mushtype.h
bsd.o: ../hdrs/htab.h
bsd.o: ../hdrs/externs.h
bsd.o: ../hdrs/compile.h
bsd.o: ../hdrs/dbdefs.h
bsd.o: ../hdrs/mushdb.h
bsd.o: ../hdrs/flags.h
bsd.o: ../hdrs/ptab.h
bsd.o: ../hdrs/division.h
bsd.o: ../hdrs/chunk.h
bsd.o: ../hdrs/bufferq.h
bsd.o: ../confmagic.h
bsd.o: ../hdrs/lock.h
bsd.o: ../hdrs/boolexp.h
bsd.o: ../hdrs/help.h
bsd.o: ../hdrs/match.h
bsd.o: ../hdrs/ansi.h
bsd.o: ../hdrs/pueblo.h
bsd.o: ../hdrs/parse.h
bsd.o: ../hdrs/access.h
bsd.o: ../hdrs/command.h
bsd.o: ../hdrs/switches.h
bsd.o: ../hdrs/version.h
bsd.o: ../hdrs/patches.h
bsd.o: ../hdrs/mysocket.h
bsd.o: ../hdrs/ident.h
bsd.o: ../hdrs/strtree.h
bsd.o: ../hdrs/log.h
bsd.o: ../hdrs/pcre.h
bsd.o: ../hdrs/myssl.h
bsd.o: ../hdrs/mymalloc.h
bsd.o: ../hdrs/extmail.h
bsd.o: ../hdrs/attrib.h
bsd.o: ../hdrs/game.h
bsd.o: ../hdrs/dbio.h
bufferq.o: ../hdrs/copyrite.h
bufferq.o: ../config.h
bufferq.o: ../hdrs/conf.h
bufferq.o: ../options.h
bufferq.o: ../hdrs/mushtype.h
bufferq.o: ../hdrs/htab.h
bufferq.o: ../hdrs/externs.h
bufferq.o: ../hdrs/compile.h
bufferq.o: ../hdrs/dbdefs.h
bufferq.o: ../hdrs/mushdb.h
bufferq.o: ../hdrs/flags.h
bufferq.o: ../hdrs/ptab.h
bufferq.o: ../hdrs/division.h
bufferq.o: ../hdrs/chunk.h
bufferq.o: ../hdrs/bufferq.h
bufferq.o: ../confmagic.h
bufferq.o: ../hdrs/mymalloc.h
bufferq.o: ../hdrs/log.h
chunk.o: ../hdrs/copyrite.h
chunk.o: ../config.h
chunk.o: ../hdrs/conf.h
chunk.o: ../options.h
chunk.o: ../hdrs/mushtype.h
chunk.o: ../hdrs/htab.h
chunk.o: ../hdrs/externs.h
chunk.o: ../hdrs/compile.h
chunk.o: ../hdrs/dbdefs.h
chunk.o: ../hdrs/mushdb.h
chunk.o: ../hdrs/flags.h
chunk.o: ../hdrs/ptab.h
chunk.o: ../hdrs/division.h
chunk.o: ../hdrs/chunk.h
chunk.o: ../hdrs/bufferq.h
chunk.o: ../confmagic.h
chunk.o: ../hdrs/boolexp.h
chunk.o: ../hdrs/command.h
chunk.o: ../hdrs/switches.h
chunk.o: ../hdrs/intrface.h
chunk.o: ../hdrs/log.h
chunk.o: ../hdrs/mymalloc.h
cmdlocal.o: ../hdrs/copyrite.h
cmdlocal.o: ../config.h
cmdlocal.o: ../hdrs/conf.h
cmdlocal.o: ../options.h
cmdlocal.o: ../hdrs/mushtype.h
cmdlocal.o: ../hdrs/htab.h
cmdlocal.o: ../hdrs/externs.h
cmdlocal.o: ../hdrs/compile.h
cmdlocal.o: ../hdrs/dbdefs.h
cmdlocal.o: ../hdrs/mushdb.h
cmdlocal.o: ../hdrs/flags.h
cmdlocal.o: ../hdrs/ptab.h
cmdlocal.o: ../hdrs/division.h
cmdlocal.o: ../hdrs/chunk.h
cmdlocal.o: ../hdrs/bufferq.h
cmdlocal.o: ../confmagic.h
cmdlocal.o: ../hdrs/parse.h
cmdlocal.o: ../hdrs/command.h
cmdlocal.o: ../hdrs/boolexp.h
cmdlocal.o: ../hdrs/switches.h
cmdlocal.o: ../hdrs/cmds.h
cmds.o: ../hdrs/copyrite.h
cmds.o: ../config.h
cmds.o: ../hdrs/conf.h
cmds.o: ../options.h
cmds.o: ../hdrs/mushtype.h
cmds.o: ../hdrs/htab.h
cmds.o: ../hdrs/externs.h
cmds.o: ../hdrs/compile.h
cmds.o: ../hdrs/dbdefs.h
cmds.o: ../hdrs/mushdb.h
cmds.o: ../hdrs/flags.h
cmds.o: ../hdrs/ptab.h
cmds.o: ../hdrs/division.h
cmds.o: ../hdrs/chunk.h
cmds.o: ../hdrs/bufferq.h
cmds.o: ../confmagic.h
cmds.o: ../hdrs/match.h
cmds.o: ../hdrs/game.h
cmds.o: ../hdrs/attrib.h
cmds.o: ../hdrs/boolexp.h
cmds.o: ../hdrs/command.h
cmds.o: ../hdrs/switches.h
cmds.o: ../hdrs/extmail.h
cmds.o: ../hdrs/malias.h
cmds.o: ../hdrs/getpgsiz.h
cmds.o: ../hdrs/parse.h
cmds.o: ../hdrs/access.h
cmds.o: ../hdrs/version.h
cmds.o: ../hdrs/lock.h
cmds.o: ../hdrs/function.h
cmds.o: ../hdrs/log.h
command.o: ../hdrs/copyrite.h
command.o: ../config.h
command.o: ../hdrs/conf.h
command.o: ../options.h
command.o: ../hdrs/mushtype.h
command.o: ../hdrs/htab.h
command.o: ../hdrs/externs.h
command.o: ../hdrs/compile.h
command.o: ../hdrs/dbdefs.h
command.o: ../hdrs/mushdb.h
command.o: ../hdrs/flags.h
command.o: ../hdrs/ptab.h
command.o: ../hdrs/division.h
command.o: ../hdrs/chunk.h
command.o: ../hdrs/bufferq.h
command.o: ../confmagic.h
command.o: ../hdrs/game.h
command.o: ../hdrs/match.h
command.o: ../hdrs/attrib.h
command.o: ../hdrs/boolexp.h
command.o: ../hdrs/command.h
command.o: ../hdrs/switches.h
command.o: ../hdrs/extmail.h
command.o: ../hdrs/getpgsiz.h
command.o: ../hdrs/parse.h
command.o: ../hdrs/access.h
command.o: ../hdrs/version.h
command.o: ../hdrs/function.h
command.o: ../hdrs/mymalloc.h
command.o: ../hdrs/log.h
command.o: ../hdrs/cmds.h
command.o: switchinc.c
compress.o: ../config.h
compress.o: ../options.h
compress.o: ../hdrs/mushtype.h
compress.o: ../hdrs/copyrite.h
compress.o: ../hdrs/log.h
compress.o: comp_w.c
compress.o: ../hdrs/conf.h
compress.o: ../hdrs/htab.h
compress.o: ../hdrs/externs.h
compress.o: ../hdrs/compile.h
compress.o: ../hdrs/dbdefs.h
compress.o: ../hdrs/mushdb.h
compress.o: ../hdrs/flags.h
compress.o: ../hdrs/ptab.h
compress.o: ../hdrs/division.h
compress.o: ../hdrs/chunk.h
compress.o: ../hdrs/bufferq.h
compress.o: ../confmagic.h
compress.o: ../hdrs/mymalloc.h
conf.o: ../hdrs/copyrite.h
conf.o: ../config.h
conf.o: ../hdrs/conf.h
conf.o: ../options.h
conf.o: ../hdrs/mushtype.h
conf.o: ../hdrs/htab.h
conf.o: ../hdrs/externs.h
conf.o: ../hdrs/compile.h
conf.o: ../hdrs/dbdefs.h
conf.o: ../hdrs/mushdb.h
conf.o: ../hdrs/flags.h
conf.o: ../hdrs/ptab.h
conf.o: ../hdrs/division.h
conf.o: ../hdrs/chunk.h
conf.o: ../hdrs/bufferq.h
conf.o: ../confmagic.h
conf.o: ../hdrs/pueblo.h
conf.o: ../hdrs/parse.h
conf.o: ../hdrs/boolexp.h
conf.o: ../hdrs/command.h
conf.o: ../hdrs/switches.h
conf.o: ../hdrs/log.h
conf.o: ../hdrs/game.h
conf.o: ../hdrs/attrib.h
conf.o: ../hdrs/help.h
conf.o: ../hdrs/function.h
cque.o: ../hdrs/copyrite.h
cque.o: ../config.h
cque.o: ../hdrs/conf.h
cque.o: ../options.h
cque.o: ../hdrs/mushtype.h
cque.o: ../hdrs/htab.h
cque.o: ../hdrs/boolexp.h
cque.o: ../hdrs/chunk.h
cque.o: ../hdrs/command.h
cque.o: ../hdrs/division.h
cque.o: ../hdrs/ptab.h
cque.o: ../hdrs/switches.h
cque.o: ../hdrs/mushdb.h
cque.o: ../hdrs/flags.h
cque.o: ../hdrs/match.h
cque.o: ../hdrs/externs.h
cque.o: ../hdrs/compile.h
cque.o: ../hdrs/dbdefs.h
cque.o: ../hdrs/bufferq.h
cque.o: ../confmagic.h
cque.o: ../hdrs/parse.h
cque.o: ../hdrs/strtree.h
cque.o: ../hdrs/mymalloc.h
cque.o: ../hdrs/game.h
cque.o: ../hdrs/attrib.h
cque.o: ../hdrs/log.h
create.o: ../hdrs/copyrite.h
create.o: ../config.h
create.o: ../hdrs/conf.h
create.o: ../options.h
create.o: ../hdrs/mushtype.h
create.o: ../hdrs/htab.h
create.o: ../hdrs/externs.h
create.o: ../hdrs/compile.h
create.o: ../hdrs/dbdefs.h
create.o: ../hdrs/mushdb.h
create.o: ../hdrs/flags.h
create.o: ../hdrs/ptab.h
create.o: ../hdrs/division.h
create.o: ../hdrs/chunk.h
create.o: ../hdrs/bufferq.h
create.o: ../confmagic.h
create.o: ../hdrs/attrib.h
create.o: ../hdrs/boolexp.h
create.o: ../hdrs/command.h
create.o: ../hdrs/switches.h
create.o: ../hdrs/match.h
create.o: ../hdrs/extchat.h
create.o: ../hdrs/log.h
create.o: ../hdrs/lock.h
create.o: ../hdrs/parse.h
create.o: ../hdrs/game.h
cron.o: ../hdrs/copyrite.h
cron.o: ../config.h
cron.o: ../hdrs/conf.h
cron.o: ../options.h
cron.o: ../hdrs/mushtype.h
cron.o: ../hdrs/htab.h
cron.o: ../hdrs/externs.h
cron.o: ../hdrs/compile.h
cron.o: ../hdrs/dbdefs.h
cron.o: ../hdrs/mushdb.h
cron.o: ../hdrs/flags.h
cron.o: ../hdrs/ptab.h
cron.o: ../hdrs/division.h
cron.o: ../hdrs/chunk.h
cron.o: ../hdrs/bufferq.h
cron.o: ../confmagic.h
cron.o: ../hdrs/parse.h
cron.o: ../hdrs/privtab.h
cron.o: ../hdrs/attrib.h
cron.o: ../hdrs/boolexp.h
cron.o: ../hdrs/command.h
cron.o: ../hdrs/switches.h
cron.o: ../hdrs/lock.h
cron.o: ../hdrs/log.h
cron.o: ../hdrs/match.h
cron.o: ../hdrs/cron.h
db.o: ../hdrs/copyrite.h
db.o: ../config.h
db.o: ../hdrs/conf.h
db.o: ../options.h
db.o: ../hdrs/mushtype.h
db.o: ../hdrs/htab.h
db.o: ../hdrs/dbio.h
db.o: ../hdrs/externs.h
db.o: ../hdrs/compile.h
db.o: ../hdrs/dbdefs.h
db.o: ../hdrs/mushdb.h
db.o: ../hdrs/flags.h
db.o: ../hdrs/ptab.h
db.o: ../hdrs/division.h
db.o: ../hdrs/chunk.h
db.o: ../hdrs/bufferq.h
db.o: ../confmagic.h
db.o: ../hdrs/attrib.h
db.o: ../hdrs/boolexp.h
db.o: ../hdrs/command.h
db.o: ../hdrs/switches.h
db.o: ../hdrs/mymalloc.h
db.o: ../hdrs/game.h
db.o: ../hdrs/lock.h
db.o: ../hdrs/log.h
db.o: ../hdrs/strtree.h
db.o: ../hdrs/parse.h
db.o: ../hdrs/privtab.h
db.o: ../hdrs/extmail.h
destroy.o: ../config.h
destroy.o: ../hdrs/copyrite.h
destroy.o: ../hdrs/conf.h
destroy.o: ../options.h
destroy.o: ../hdrs/mushtype.h
destroy.o: ../hdrs/htab.h
destroy.o: ../hdrs/mushdb.h
destroy.o: ../hdrs/flags.h
destroy.o: ../hdrs/ptab.h
destroy.o: ../hdrs/division.h
destroy.o: ../hdrs/match.h
destroy.o: ../hdrs/externs.h
destroy.o: ../hdrs/compile.h
destroy.o: ../hdrs/dbdefs.h
destroy.o: ../hdrs/chunk.h
destroy.o: ../hdrs/bufferq.h
destroy.o: ../confmagic.h
destroy.o: ../hdrs/log.h
destroy.o: ../hdrs/game.h
destroy.o: ../hdrs/extmail.h
destroy.o: ../hdrs/malias.h
destroy.o: ../hdrs/attrib.h
destroy.o: ../hdrs/boolexp.h
destroy.o: ../hdrs/command.h
destroy.o: ../hdrs/switches.h
destroy.o: ../hdrs/lock.h
division.o: ../hdrs/copyrite.h
division.o: ../config.h
division.o: ../hdrs/conf.h
division.o: ../options.h
division.o: ../hdrs/mushtype.h
division.o: ../hdrs/htab.h
division.o: ../hdrs/externs.h
division.o: ../hdrs/compile.h
division.o: ../hdrs/dbdefs.h
division.o: ../hdrs/mushdb.h
division.o: ../hdrs/flags.h
division.o: ../hdrs/ptab.h
division.o: ../hdrs/division.h
division.o: ../hdrs/chunk.h
division.o: ../hdrs/bufferq.h
division.o: ../confmagic.h
division.o: ../hdrs/boolexp.h
division.o: ../hdrs/command.h
division.o: ../hdrs/switches.h
division.o: ../hdrs/parse.h
division.o: ../hdrs/cmds.h
division.o: ../hdrs/dbio.h
division.o: ../hdrs/match.h
division.o: ../hdrs/log.h
division.o: ../hdrs/attrib.h
division.o: ../hdrs/ansi.h
extchat.o: ../hdrs/copyrite.h
extchat.o: ../config.h
extchat.o: ../hdrs/conf.h
extchat.o: ../options.h
extchat.o: ../hdrs/mushtype.h
extchat.o: ../hdrs/htab.h
extchat.o: ../hdrs/externs.h
extchat.o: ../hdrs/compile.h
extchat.o: ../hdrs/dbdefs.h
extchat.o: ../hdrs/mushdb.h
extchat.o: ../hdrs/flags.h
extchat.o: ../hdrs/ptab.h
extchat.o: ../hdrs/division.h
extchat.o: ../hdrs/chunk.h
extchat.o: ../hdrs/bufferq.h
extchat.o: ../confmagic.h
extchat.o: ../hdrs/attrib.h
extchat.o: ../hdrs/boolexp.h
extchat.o: ../hdrs/command.h
extchat.o: ../hdrs/switches.h
extchat.o: ../hdrs/match.h
extchat.o: ../hdrs/extchat.h
extchat.o: ../hdrs/ansi.h
extchat.o: ../hdrs/privtab.h
extchat.o: ../hdrs/mymalloc.h
extchat.o: ../hdrs/pueblo.h
extchat.o: ../hdrs/parse.h
extchat.o: ../hdrs/lock.h
extchat.o: ../hdrs/log.h
extchat.o: ../hdrs/game.h
extchat.o: ../hdrs/function.h
extchat.o: ../hdrs/dbio.h
extmail.o: ../config.h
extmail.o: ../hdrs/copyrite.h
extmail.o: ../hdrs/conf.h
extmail.o: ../options.h
extmail.o: ../hdrs/mushtype.h
extmail.o: ../hdrs/htab.h
extmail.o: ../hdrs/externs.h
extmail.o: ../hdrs/compile.h
extmail.o: ../hdrs/dbdefs.h
extmail.o: ../hdrs/mushdb.h
extmail.o: ../hdrs/flags.h
extmail.o: ../hdrs/ptab.h
extmail.o: ../hdrs/division.h
extmail.o: ../hdrs/chunk.h
extmail.o: ../hdrs/bufferq.h
extmail.o: ../confmagic.h
extmail.o: ../hdrs/match.h
extmail.o: ../hdrs/extmail.h
extmail.o: ../hdrs/malias.h
extmail.o: ../hdrs/attrib.h
extmail.o: ../hdrs/boolexp.h
extmail.o: ../hdrs/command.h
extmail.o: ../hdrs/switches.h
extmail.o: ../hdrs/parse.h
extmail.o: ../hdrs/mymalloc.h
extmail.o: ../hdrs/pueblo.h
extmail.o: ../hdrs/log.h
extmail.o: ../hdrs/lock.h
extmail.o: ../hdrs/dbio.h
flaglocal.o: ../hdrs/copyrite.h
flaglocal.o: ../config.h
flaglocal.o: ../hdrs/conf.h
flaglocal.o: ../options.h
flaglocal.o: ../hdrs/mushtype.h
flaglocal.o: ../hdrs/htab.h
flaglocal.o: ../hdrs/externs.h
flaglocal.o: ../hdrs/compile.h
flaglocal.o: ../hdrs/dbdefs.h
flaglocal.o: ../hdrs/mushdb.h
flaglocal.o: ../hdrs/flags.h
flaglocal.o: ../hdrs/ptab.h
flaglocal.o: ../hdrs/division.h
flaglocal.o: ../hdrs/chunk.h
flaglocal.o: ../hdrs/bufferq.h
flaglocal.o: ../confmagic.h
flags.o: ../config.h
flags.o: ../hdrs/conf.h
flags.o: ../hdrs/copyrite.h
flags.o: ../options.h
flags.o: ../hdrs/mushtype.h
flags.o: ../hdrs/htab.h
flags.o: ../hdrs/externs.h
flags.o: ../hdrs/compile.h
flags.o: ../hdrs/dbdefs.h
flags.o: ../hdrs/mushdb.h
flags.o: ../hdrs/flags.h
flags.o: ../hdrs/ptab.h
flags.o: ../hdrs/division.h
flags.o: ../hdrs/chunk.h
flags.o: ../hdrs/bufferq.h
flags.o: ../confmagic.h
flags.o: ../hdrs/boolexp.h
flags.o: ../hdrs/command.h
flags.o: ../hdrs/switches.h
flags.o: ../hdrs/attrib.h
flags.o: ../hdrs/parse.h
flags.o: ../hdrs/match.h
flags.o: ../hdrs/privtab.h
flags.o: ../hdrs/game.h
flags.o: ../hdrs/lock.h
flags.o: ../hdrs/log.h
flags.o: ../hdrs/dbio.h
flags.o: ../hdrs/oldflags.h
flags.o: ../hdrs/ansi.h
funcrypt.o: ../hdrs/copyrite.h
funcrypt.o: ../config.h
funcrypt.o: ../hdrs/conf.h
funcrypt.o: ../options.h
funcrypt.o: ../hdrs/mushtype.h
funcrypt.o: ../hdrs/htab.h
funcrypt.o: ../hdrs/case.h
funcrypt.o: ../hdrs/externs.h
funcrypt.o: ../hdrs/compile.h
funcrypt.o: ../hdrs/dbdefs.h
funcrypt.o: ../hdrs/mushdb.h
funcrypt.o: ../hdrs/flags.h
funcrypt.o: ../hdrs/ptab.h
funcrypt.o: ../hdrs/division.h
funcrypt.o: ../hdrs/chunk.h
funcrypt.o: ../hdrs/bufferq.h
funcrypt.o: ../confmagic.h
funcrypt.o: ../hdrs/version.h
funcrypt.o: ../hdrs/extchat.h
funcrypt.o: ../hdrs/boolexp.h
funcrypt.o: ../hdrs/parse.h
funcrypt.o: ../hdrs/function.h
funcrypt.o: ../hdrs/command.h
funcrypt.o: ../hdrs/switches.h
funcrypt.o: ../hdrs/game.h
funcrypt.o: ../hdrs/attrib.h
funcrypt.o: ../hdrs/ansi.h
funcrypt.o: ../hdrs/match.h
function.o: ../hdrs/copyrite.h
function.o: ../config.h
function.o: ../hdrs/conf.h
function.o: ../options.h
function.o: ../hdrs/mushtype.h
function.o: ../hdrs/htab.h
function.o: ../hdrs/externs.h
function.o: ../hdrs/compile.h
function.o: ../hdrs/dbdefs.h
function.o: ../hdrs/mushdb.h
function.o: ../hdrs/flags.h
function.o: ../hdrs/ptab.h
function.o: ../hdrs/division.h
function.o: ../hdrs/chunk.h
function.o: ../hdrs/bufferq.h
function.o: ../confmagic.h
function.o: ../hdrs/attrib.h
function.o: ../hdrs/boolexp.h
function.o: ../hdrs/command.h
function.o: ../hdrs/switches.h
function.o: ../hdrs/function.h
function.o: ../hdrs/match.h
function.o: ../hdrs/parse.h
function.o: ../hdrs/lock.h
function.o: ../hdrs/game.h
function.o: ../hdrs/mymalloc.h
function.o: ../hdrs/funs.h
fundb.o: ../hdrs/copyrite.h
fundb.o: ../config.h
fundb.o: ../hdrs/conf.h
fundb.o: ../options.h
fundb.o: ../hdrs/mushtype.h
fundb.o: ../hdrs/htab.h
fundb.o: ../hdrs/externs.h
fundb.o: ../hdrs/compile.h
fundb.o: ../hdrs/dbdefs.h
fundb.o: ../hdrs/mushdb.h
fundb.o: ../hdrs/flags.h
fundb.o: ../hdrs/ptab.h
fundb.o: ../hdrs/division.h
fundb.o: ../hdrs/chunk.h
fundb.o: ../hdrs/bufferq.h
fundb.o: ../confmagic.h
fundb.o: ../hdrs/match.h
fundb.o: ../hdrs/parse.h
fundb.o: ../hdrs/boolexp.h
fundb.o: ../hdrs/command.h
fundb.o: ../hdrs/switches.h
fundb.o: ../hdrs/game.h
fundb.o: ../hdrs/privtab.h
fundb.o: ../hdrs/lock.h
fundb.o: ../hdrs/log.h
fundb.o: ../hdrs/attrib.h
fundb.o: ../hdrs/function.h
fundiv.o: ../hdrs/copyrite.h
fundiv.o: ../config.h
fundiv.o: ../hdrs/conf.h
fundiv.o: ../options.h
fundiv.o: ../hdrs/mushtype.h
fundiv.o: ../hdrs/htab.h
fundiv.o: ../hdrs/externs.h
fundiv.o: ../hdrs/compile.h
fundiv.o: ../hdrs/dbdefs.h
fundiv.o: ../hdrs/mushdb.h
fundiv.o: ../hdrs/flags.h
fundiv.o: ../hdrs/ptab.h
fundiv.o: ../hdrs/division.h
fundiv.o: ../hdrs/chunk.h
fundiv.o: ../hdrs/bufferq.h
fundiv.o: ../confmagic.h
fundiv.o: ../hdrs/match.h
fundiv.o: ../hdrs/parse.h
fundiv.o: ../hdrs/boolexp.h
fundiv.o: ../hdrs/command.h
fundiv.o: ../hdrs/switches.h
fundiv.o: ../hdrs/game.h
fundiv.o: ../hdrs/privtab.h
fundiv.o: ../hdrs/lock.h
fundiv.o: ../hdrs/log.h
fundiv.o: ../hdrs/attrib.h
fundiv.o: ../hdrs/extchat.h
funlist.o: ../hdrs/copyrite.h
funlist.o: ../config.h
funlist.o: ../hdrs/ansi.h
funlist.o: ../hdrs/conf.h
funlist.o: ../options.h
funlist.o: ../hdrs/mushtype.h
funlist.o: ../hdrs/htab.h
funlist.o: ../hdrs/case.h
funlist.o: ../hdrs/externs.h
funlist.o: ../hdrs/compile.h
funlist.o: ../hdrs/dbdefs.h
funlist.o: ../hdrs/mushdb.h
funlist.o: ../hdrs/flags.h
funlist.o: ../hdrs/ptab.h
funlist.o: ../hdrs/division.h
funlist.o: ../hdrs/chunk.h
funlist.o: ../hdrs/bufferq.h
funlist.o: ../confmagic.h
funlist.o: ../hdrs/parse.h
funlist.o: ../hdrs/boolexp.h
funlist.o: ../hdrs/function.h
funlist.o: ../hdrs/mymalloc.h
funlist.o: ../hdrs/pcre.h
funlist.o: ../hdrs/match.h
funlist.o: ../hdrs/attrib.h
funlist.o: ../hdrs/command.h
funlist.o: ../hdrs/switches.h
funlist.o: ../hdrs/lock.h
funlocal.o: ../hdrs/copyrite.h
funlocal.o: ../config.h
funlocal.o: ../hdrs/conf.h
funlocal.o: ../options.h
funlocal.o: ../hdrs/mushtype.h
funlocal.o: ../hdrs/htab.h
funlocal.o: ../hdrs/externs.h
funlocal.o: ../hdrs/compile.h
funlocal.o: ../hdrs/dbdefs.h
funlocal.o: ../hdrs/mushdb.h
funlocal.o: ../hdrs/flags.h
funlocal.o: ../hdrs/ptab.h
funlocal.o: ../hdrs/division.h
funlocal.o: ../hdrs/chunk.h
funlocal.o: ../hdrs/bufferq.h
funlocal.o: ../confmagic.h
funlocal.o: ../hdrs/parse.h
funlocal.o: ../hdrs/boolexp.h
funlocal.o: ../hdrs/function.h
funmath.o: ../hdrs/copyrite.h
funmath.o: ../config.h
funmath.o: ../hdrs/conf.h
funmath.o: ../options.h
funmath.o: ../hdrs/mushtype.h
funmath.o: ../hdrs/htab.h
funmath.o: ../hdrs/externs.h
funmath.o: ../hdrs/compile.h
funmath.o: ../hdrs/dbdefs.h
funmath.o: ../hdrs/mushdb.h
funmath.o: ../hdrs/flags.h
funmath.o: ../hdrs/ptab.h
funmath.o: ../hdrs/division.h
funmath.o: ../hdrs/chunk.h
funmath.o: ../hdrs/bufferq.h
funmath.o: ../confmagic.h
funmath.o: ../hdrs/parse.h
funmisc.o: ../hdrs/copyrite.h
funmisc.o: ../config.h
funmisc.o: ../hdrs/conf.h
funmisc.o: ../options.h
funmisc.o: ../hdrs/mushtype.h
funmisc.o: ../hdrs/htab.h
funmisc.o: ../hdrs/case.h
funmisc.o: ../hdrs/externs.h
funmisc.o: ../hdrs/compile.h
funmisc.o: ../hdrs/dbdefs.h
funmisc.o: ../hdrs/mushdb.h
funmisc.o: ../hdrs/flags.h
funmisc.o: ../hdrs/ptab.h
funmisc.o: ../hdrs/division.h
funmisc.o: ../hdrs/chunk.h
funmisc.o: ../hdrs/bufferq.h
funmisc.o: ../confmagic.h
funmisc.o: ../hdrs/version.h
funmisc.o: ../hdrs/lock.h
funmisc.o: ../hdrs/boolexp.h
funmisc.o: ../hdrs/match.h
funmisc.o: ../hdrs/parse.h
funmisc.o: ../hdrs/function.h
funmisc.o: ../hdrs/command.h
funmisc.o: ../hdrs/switches.h
funmisc.o: ../hdrs/game.h
funmisc.o: ../hdrs/attrib.h
funstr.o: ../hdrs/copyrite.h
funstr.o: ../config.h
funstr.o: ../hdrs/conf.h
funstr.o: ../options.h
funstr.o: ../hdrs/mushtype.h
funstr.o: ../hdrs/htab.h
funstr.o: ../hdrs/ansi.h
funstr.o: ../hdrs/externs.h
funstr.o: ../hdrs/compile.h
funstr.o: ../hdrs/dbdefs.h
funstr.o: ../hdrs/mushdb.h
funstr.o: ../hdrs/flags.h
funstr.o: ../hdrs/ptab.h
funstr.o: ../hdrs/division.h
funstr.o: ../hdrs/chunk.h
funstr.o: ../hdrs/bufferq.h
funstr.o: ../confmagic.h
funstr.o: ../hdrs/case.h
funstr.o: ../hdrs/match.h
funstr.o: ../hdrs/parse.h
funstr.o: ../hdrs/pueblo.h
funstr.o: ../hdrs/attrib.h
funstr.o: ../hdrs/boolexp.h
funstr.o: ../hdrs/command.h
funstr.o: ../hdrs/switches.h
funstr.o: ../hdrs/lock.h
funtime.o: ../hdrs/copyrite.h
funtime.o: ../config.h
funtime.o: ../hdrs/conf.h
funtime.o: ../options.h
funtime.o: ../hdrs/mushtype.h
funtime.o: ../hdrs/htab.h
funtime.o: ../hdrs/externs.h
funtime.o: ../hdrs/compile.h
funtime.o: ../hdrs/dbdefs.h
funtime.o: ../hdrs/mushdb.h
funtime.o: ../hdrs/flags.h
funtime.o: ../hdrs/ptab.h
funtime.o: ../hdrs/division.h
funtime.o: ../hdrs/chunk.h
funtime.o: ../hdrs/bufferq.h
funtime.o: ../confmagic.h
funtime.o: ../hdrs/parse.h
funtime.o: ../hdrs/log.h
funtime.o: ../hdrs/match.h
funtime.o: ../hdrs/attrib.h
funtime.o: ../hdrs/boolexp.h
funtime.o: ../hdrs/command.h
funtime.o: ../hdrs/switches.h
funufun.o: ../hdrs/copyrite.h
funufun.o: ../config.h
funufun.o: ../hdrs/conf.h
funufun.o: ../options.h
funufun.o: ../hdrs/mushtype.h
funufun.o: ../hdrs/htab.h
funufun.o: ../hdrs/externs.h
funufun.o: ../hdrs/compile.h
funufun.o: ../hdrs/dbdefs.h
funufun.o: ../hdrs/mushdb.h
funufun.o: ../hdrs/flags.h
funufun.o: ../hdrs/ptab.h
funufun.o: ../hdrs/division.h
funufun.o: ../hdrs/chunk.h
funufun.o: ../hdrs/bufferq.h
funufun.o: ../confmagic.h
funufun.o: ../hdrs/match.h
funufun.o: ../hdrs/parse.h
funufun.o: ../hdrs/mymalloc.h
funufun.o: ../hdrs/attrib.h
funufun.o: ../hdrs/boolexp.h
funufun.o: ../hdrs/command.h
funufun.o: ../hdrs/switches.h
funufun.o: ../hdrs/lock.h
game.o: ../hdrs/copyrite.h
game.o: ../config.h
game.o: ../hdrs/conf.h
game.o: ../options.h
game.o: ../hdrs/mushtype.h
game.o: ../hdrs/htab.h
game.o: ../hdrs/externs.h
game.o: ../hdrs/compile.h
game.o: ../hdrs/dbdefs.h
game.o: ../hdrs/mushdb.h
game.o: ../hdrs/flags.h
game.o: ../hdrs/ptab.h
game.o: ../hdrs/division.h
game.o: ../hdrs/chunk.h
game.o: ../hdrs/bufferq.h
game.o: ../confmagic.h
game.o: ../hdrs/game.h
game.o: ../hdrs/attrib.h
game.o: ../hdrs/boolexp.h
game.o: ../hdrs/command.h
game.o: ../hdrs/switches.h
game.o: ../hdrs/match.h
game.o: ../hdrs/case.h
game.o: ../hdrs/extmail.h
game.o: ../hdrs/extchat.h
game.o: ../hdrs/myssl.h
game.o: ../hdrs/getpgsiz.h
game.o: ../hdrs/parse.h
game.o: ../hdrs/access.h
game.o: ../hdrs/version.h
game.o: ../hdrs/strtree.h
game.o: ../hdrs/log.h
game.o: ../hdrs/lock.h
game.o: ../hdrs/function.h
game.o: ../hdrs/help.h
game.o: ../hdrs/dbio.h
game.o: ../hdrs/pcre.h
help.o: ../config.h
help.o: ../hdrs/conf.h
help.o: ../hdrs/copyrite.h
help.o: ../options.h
help.o: ../hdrs/mushtype.h
help.o: ../hdrs/htab.h
help.o: ../hdrs/externs.h
help.o: ../hdrs/compile.h
help.o: ../hdrs/dbdefs.h
help.o: ../hdrs/mushdb.h
help.o: ../hdrs/flags.h
help.o: ../hdrs/ptab.h
help.o: ../hdrs/division.h
help.o: ../hdrs/chunk.h
help.o: ../hdrs/bufferq.h
help.o: ../confmagic.h
help.o: ../hdrs/boolexp.h
help.o: ../hdrs/command.h
help.o: ../hdrs/switches.h
help.o: ../hdrs/help.h
help.o: ../hdrs/log.h
help.o: ../hdrs/ansi.h
help.o: ../hdrs/parse.h
help.o: ../hdrs/pueblo.h
help.o: ../hdrs/mymalloc.h
htab.o: ../config.h
htab.o: ../hdrs/copyrite.h
htab.o: ../hdrs/conf.h
htab.o: ../options.h
htab.o: ../hdrs/mushtype.h
htab.o: ../hdrs/htab.h
htab.o: ../hdrs/externs.h
htab.o: ../hdrs/compile.h
htab.o: ../hdrs/dbdefs.h
htab.o: ../hdrs/mushdb.h
htab.o: ../hdrs/flags.h
htab.o: ../hdrs/ptab.h
htab.o: ../hdrs/division.h
htab.o: ../hdrs/chunk.h
htab.o: ../hdrs/bufferq.h
htab.o: ../confmagic.h
htab.o: ../hdrs/mymalloc.h
ident.o: ../config.h
ident.o: ../hdrs/conf.h
ident.o: ../hdrs/copyrite.h
ident.o: ../options.h
ident.o: ../hdrs/mushtype.h
ident.o: ../hdrs/htab.h
ident.o: ../hdrs/externs.h
ident.o: ../hdrs/compile.h
ident.o: ../hdrs/dbdefs.h
ident.o: ../hdrs/mushdb.h
ident.o: ../hdrs/flags.h
ident.o: ../hdrs/ptab.h
ident.o: ../hdrs/division.h
ident.o: ../hdrs/chunk.h
ident.o: ../hdrs/bufferq.h
ident.o: ../confmagic.h
ident.o: ../hdrs/attrib.h
ident.o: ../hdrs/boolexp.h
ident.o: ../hdrs/command.h
ident.o: ../hdrs/switches.h
ident.o: ../hdrs/ident.h
ident.o: ../hdrs/mysocket.h
ident.o: ../hdrs/mymalloc.h
local.o: ../hdrs/copyrite.h
local.o: ../config.h
local.o: ../hdrs/conf.h
local.o: ../options.h
local.o: ../hdrs/mushtype.h
local.o: ../hdrs/htab.h
local.o: ../hdrs/dbio.h
local.o: ../hdrs/externs.h
local.o: ../hdrs/compile.h
local.o: ../hdrs/dbdefs.h
local.o: ../hdrs/mushdb.h
local.o: ../hdrs/flags.h
local.o: ../hdrs/ptab.h
local.o: ../hdrs/division.h
local.o: ../hdrs/chunk.h
local.o: ../hdrs/bufferq.h
local.o: ../confmagic.h
local.o: ../hdrs/parse.h
local.o: ../hdrs/command.h
local.o: ../hdrs/boolexp.h
local.o: ../hdrs/switches.h
lock.o: ../hdrs/copyrite.h
lock.o: ../config.h
lock.o: ../hdrs/conf.h
lock.o: ../options.h
lock.o: ../hdrs/mushtype.h
lock.o: ../hdrs/htab.h
lock.o: ../hdrs/externs.h
lock.o: ../hdrs/compile.h
lock.o: ../hdrs/dbdefs.h
lock.o: ../hdrs/mushdb.h
lock.o: ../hdrs/flags.h
lock.o: ../hdrs/ptab.h
lock.o: ../hdrs/division.h
lock.o: ../hdrs/chunk.h
lock.o: ../hdrs/bufferq.h
lock.o: ../confmagic.h
lock.o: ../hdrs/boolexp.h
lock.o: ../hdrs/attrib.h
lock.o: ../hdrs/command.h
lock.o: ../hdrs/switches.h
lock.o: ../hdrs/lock.h
lock.o: ../hdrs/match.h
lock.o: ../hdrs/log.h
lock.o: ../hdrs/mymalloc.h
lock.o: ../hdrs/strtree.h
lock.o: ../hdrs/privtab.h
lock.o: ../hdrs/parse.h
log.o: ../hdrs/copyrite.h
log.o: ../config.h
log.o: ../hdrs/conf.h
log.o: ../options.h
log.o: ../hdrs/mushtype.h
log.o: ../hdrs/htab.h
log.o: ../hdrs/externs.h
log.o: ../hdrs/compile.h
log.o: ../hdrs/dbdefs.h
log.o: ../hdrs/mushdb.h
log.o: ../hdrs/flags.h
log.o: ../hdrs/ptab.h
log.o: ../hdrs/division.h
log.o: ../hdrs/chunk.h
log.o: ../hdrs/bufferq.h
log.o: ../confmagic.h
log.o: ../hdrs/log.h
look.o: ../config.h
look.o: ../hdrs/copyrite.h
look.o: ../hdrs/conf.h
look.o: ../options.h
look.o: ../hdrs/mushtype.h
look.o: ../hdrs/htab.h
look.o: ../hdrs/externs.h
look.o: ../hdrs/compile.h
look.o: ../hdrs/dbdefs.h
look.o: ../hdrs/mushdb.h
look.o: ../hdrs/flags.h
look.o: ../hdrs/ptab.h
look.o: ../hdrs/division.h
look.o: ../hdrs/chunk.h
look.o: ../hdrs/bufferq.h
look.o: ../confmagic.h
look.o: ../hdrs/lock.h
look.o: ../hdrs/boolexp.h
look.o: ../hdrs/attrib.h
look.o: ../hdrs/command.h
look.o: ../hdrs/switches.h
look.o: ../hdrs/match.h
look.o: ../hdrs/ansi.h
look.o: ../hdrs/pueblo.h
look.o: ../hdrs/extchat.h
look.o: ../hdrs/game.h
look.o: ../hdrs/parse.h
look.o: ../hdrs/privtab.h
look.o: ../hdrs/log.h
malias.o: ../config.h
malias.o: ../hdrs/copyrite.h
malias.o: ../hdrs/conf.h
malias.o: ../options.h
malias.o: ../hdrs/mushtype.h
malias.o: ../hdrs/htab.h
malias.o: ../hdrs/externs.h
malias.o: ../hdrs/compile.h
malias.o: ../hdrs/dbdefs.h
malias.o: ../hdrs/mushdb.h
malias.o: ../hdrs/flags.h
malias.o: ../hdrs/ptab.h
malias.o: ../hdrs/division.h
malias.o: ../hdrs/chunk.h
malias.o: ../hdrs/bufferq.h
malias.o: ../confmagic.h
malias.o: ../hdrs/match.h
malias.o: ../hdrs/parse.h
malias.o: ../hdrs/malias.h
malias.o: ../hdrs/privtab.h
malias.o: ../hdrs/mymalloc.h
malias.o: ../hdrs/pueblo.h
malias.o: ../hdrs/log.h
malias.o: ../hdrs/dbio.h
match.o: ../hdrs/copyrite.h
match.o: ../config.h
match.o: ../hdrs/conf.h
match.o: ../options.h
match.o: ../hdrs/mushtype.h
match.o: ../hdrs/htab.h
match.o: ../hdrs/mushdb.h
match.o: ../hdrs/flags.h
match.o: ../hdrs/ptab.h
match.o: ../hdrs/division.h
match.o: ../hdrs/externs.h
match.o: ../hdrs/compile.h
match.o: ../hdrs/dbdefs.h
match.o: ../hdrs/chunk.h
match.o: ../hdrs/bufferq.h
match.o: ../confmagic.h
match.o: ../hdrs/case.h
match.o: ../hdrs/match.h
match.o: ../hdrs/parse.h
memcheck.o: ../config.h
memcheck.o: ../hdrs/conf.h
memcheck.o: ../hdrs/copyrite.h
memcheck.o: ../options.h
memcheck.o: ../hdrs/mushtype.h
memcheck.o: ../hdrs/htab.h
memcheck.o: ../hdrs/externs.h
memcheck.o: ../hdrs/compile.h
memcheck.o: ../hdrs/dbdefs.h
memcheck.o: ../hdrs/mushdb.h
memcheck.o: ../hdrs/flags.h
memcheck.o: ../hdrs/ptab.h
memcheck.o: ../hdrs/division.h
memcheck.o: ../hdrs/chunk.h
memcheck.o: ../hdrs/bufferq.h
memcheck.o: ../confmagic.h
memcheck.o: ../hdrs/mymalloc.h
memcheck.o: ../hdrs/log.h
move.o: ../hdrs/copyrite.h
move.o: ../config.h
move.o: ../hdrs/conf.h
move.o: ../options.h
move.o: ../hdrs/mushtype.h
move.o: ../hdrs/htab.h
move.o: ../hdrs/externs.h
move.o: ../hdrs/compile.h
move.o: ../hdrs/dbdefs.h
move.o: ../hdrs/mushdb.h
move.o: ../hdrs/flags.h
move.o: ../hdrs/ptab.h
move.o: ../hdrs/division.h
move.o: ../hdrs/chunk.h
move.o: ../hdrs/bufferq.h
move.o: ../confmagic.h
move.o: ../hdrs/attrib.h
move.o: ../hdrs/boolexp.h
move.o: ../hdrs/command.h
move.o: ../hdrs/switches.h
move.o: ../hdrs/match.h
move.o: ../hdrs/lock.h
move.o: ../hdrs/parse.h
move.o: ../hdrs/log.h
move.o: ../hdrs/cmds.h
move.o: ../hdrs/game.h
mycrypt.o: ../config.h
mycrypt.o: ../hdrs/conf.h
mycrypt.o: ../hdrs/copyrite.h
mycrypt.o: ../options.h
mycrypt.o: ../hdrs/mushtype.h
mycrypt.o: ../hdrs/htab.h
mycrypt.o: ../confmagic.h
mymalloc.o: ../config.h
mymalloc.o: ../options.h
mymalloc.o: ../confmagic.h
mysocket.o: ../hdrs/copyrite.h
mysocket.o: ../config.h
mysocket.o: ../hdrs/conf.h
mysocket.o: ../options.h
mysocket.o: ../hdrs/mushtype.h
mysocket.o: ../hdrs/htab.h
mysocket.o: ../hdrs/externs.h
mysocket.o: ../hdrs/compile.h
mysocket.o: ../hdrs/dbdefs.h
mysocket.o: ../hdrs/mushdb.h
mysocket.o: ../hdrs/flags.h
mysocket.o: ../hdrs/ptab.h
mysocket.o: ../hdrs/division.h
mysocket.o: ../hdrs/chunk.h
mysocket.o: ../hdrs/bufferq.h
mysocket.o: ../confmagic.h
mysocket.o: ../hdrs/mymalloc.h
mysocket.o: ../hdrs/mysocket.h
mysocket.o: ../hdrs/ident.h
myssl.o: ../hdrs/copyrite.h
myssl.o: ../config.h
myssl.o: ../hdrs/conf.h
myssl.o: ../options.h
myssl.o: ../hdrs/mushtype.h
myssl.o: ../hdrs/htab.h
myssl.o: ../hdrs/mysocket.h
myssl.o: ../confmagic.h
myssl.o: ../hdrs/myssl.h
myssl.o: ../hdrs/log.h
myssl.o: ../hdrs/parse.h
notify.o: ../hdrs/copyrite.h
notify.o: ../config.h
notify.o: ../hdrs/conf.h
notify.o: ../options.h
notify.o: ../hdrs/mushtype.h
notify.o: ../hdrs/htab.h
notify.o: ../hdrs/mushdb.h
notify.o: ../hdrs/flags.h
notify.o: ../hdrs/ptab.h
notify.o: ../hdrs/division.h
notify.o: ../hdrs/externs.h
notify.o: ../hdrs/compile.h
notify.o: ../hdrs/dbdefs.h
notify.o: ../hdrs/chunk.h
notify.o: ../hdrs/bufferq.h
notify.o: ../confmagic.h
notify.o: ../hdrs/lock.h
notify.o: ../hdrs/boolexp.h
notify.o: ../hdrs/help.h
notify.o: ../hdrs/match.h
notify.o: ../hdrs/ansi.h
notify.o: ../hdrs/pueblo.h
notify.o: ../hdrs/parse.h
notify.o: ../hdrs/access.h
notify.o: ../hdrs/version.h
notify.o: ../hdrs/patches.h
notify.o: ../hdrs/mysocket.h
notify.o: ../hdrs/ident.h
notify.o: ../hdrs/strtree.h
notify.o: ../hdrs/log.h
notify.o: ../hdrs/mymalloc.h
notify.o: ../hdrs/extchat.h
notify.o: ../hdrs/extmail.h
notify.o: ../hdrs/attrib.h
notify.o: ../hdrs/command.h
notify.o: ../hdrs/switches.h
notify.o: ../hdrs/game.h
parse.o: ../hdrs/copyrite.h
parse.o: ../config.h
parse.o: ../hdrs/conf.h
parse.o: ../options.h
parse.o: ../hdrs/mushtype.h
parse.o: ../hdrs/htab.h
parse.o: ../hdrs/externs.h
parse.o: ../hdrs/compile.h
parse.o: ../hdrs/dbdefs.h
parse.o: ../hdrs/mushdb.h
parse.o: ../hdrs/flags.h
parse.o: ../hdrs/ptab.h
parse.o: ../hdrs/division.h
parse.o: ../hdrs/chunk.h
parse.o: ../hdrs/bufferq.h
parse.o: ../confmagic.h
parse.o: ../hdrs/ansi.h
parse.o: ../hdrs/boolexp.h
parse.o: ../hdrs/function.h
parse.o: ../hdrs/case.h
parse.o: ../hdrs/match.h
parse.o: ../hdrs/parse.h
parse.o: ../hdrs/attrib.h
parse.o: ../hdrs/command.h
parse.o: ../hdrs/switches.h
parse.o: ../hdrs/pcre.h
parse.o: ../hdrs/log.h
parse.o: ../hdrs/mymalloc.h
pcre.o: ../config.h
pcre.o: ../hdrs/pcre.h
pcre.o: ../confmagic.h
player.o: ../hdrs/copyrite.h
player.o: ../config.h
player.o: ../hdrs/conf.h
player.o: ../options.h
player.o: ../hdrs/mushtype.h
player.o: ../hdrs/htab.h
player.o: ../hdrs/externs.h
player.o: ../hdrs/compile.h
player.o: ../hdrs/dbdefs.h
player.o: ../hdrs/mushdb.h
player.o: ../hdrs/flags.h
player.o: ../hdrs/ptab.h
player.o: ../hdrs/division.h
player.o: ../hdrs/chunk.h
player.o: ../hdrs/bufferq.h
player.o: ../confmagic.h
player.o: ../hdrs/attrib.h
player.o: ../hdrs/boolexp.h
player.o: ../hdrs/command.h
player.o: ../hdrs/switches.h
player.o: ../hdrs/access.h
player.o: ../hdrs/parse.h
player.o: ../hdrs/mymalloc.h
player.o: ../hdrs/log.h
player.o: ../hdrs/lock.h
player.o: ../hdrs/extmail.h
plyrlist.o: ../config.h
plyrlist.o: ../hdrs/copyrite.h
plyrlist.o: ../hdrs/conf.h
plyrlist.o: ../options.h
plyrlist.o: ../hdrs/mushtype.h
plyrlist.o: ../hdrs/htab.h
plyrlist.o: ../hdrs/externs.h
plyrlist.o: ../hdrs/compile.h
plyrlist.o: ../hdrs/dbdefs.h
plyrlist.o: ../hdrs/mushdb.h
plyrlist.o: ../hdrs/flags.h
plyrlist.o: ../hdrs/ptab.h
plyrlist.o: ../hdrs/division.h
plyrlist.o: ../hdrs/chunk.h
plyrlist.o: ../hdrs/bufferq.h
plyrlist.o: ../confmagic.h
plyrlist.o: ../hdrs/attrib.h
plyrlist.o: ../hdrs/boolexp.h
plyrlist.o: ../hdrs/command.h
plyrlist.o: ../hdrs/switches.h
predicat.o: ../hdrs/copyrite.h
predicat.o: ../config.h
predicat.o: ../hdrs/conf.h
predicat.o: ../options.h
predicat.o: ../hdrs/mushtype.h
predicat.o: ../hdrs/htab.h
predicat.o: ../hdrs/externs.h
predicat.o: ../hdrs/compile.h
predicat.o: ../hdrs/dbdefs.h
predicat.o: ../hdrs/mushdb.h
predicat.o: ../hdrs/flags.h
predicat.o: ../hdrs/ptab.h
predicat.o: ../hdrs/division.h
predicat.o: ../hdrs/chunk.h
predicat.o: ../hdrs/bufferq.h
predicat.o: ../confmagic.h
predicat.o: ../hdrs/attrib.h
predicat.o: ../hdrs/boolexp.h
predicat.o: ../hdrs/command.h
predicat.o: ../hdrs/switches.h
predicat.o: ../hdrs/lock.h
predicat.o: ../hdrs/match.h
predicat.o: ../hdrs/ansi.h
predicat.o: ../hdrs/parse.h
predicat.o: ../hdrs/privtab.h
predicat.o: ../hdrs/mymalloc.h
privtab.o: ../hdrs/copyrite.h
privtab.o: ../config.h
privtab.o: ../hdrs/conf.h
privtab.o: ../options.h
privtab.o: ../hdrs/mushtype.h
privtab.o: ../hdrs/htab.h
privtab.o: ../hdrs/privtab.h
privtab.o: ../confmagic.h
privtab.o: ../hdrs/externs.h
privtab.o: ../hdrs/compile.h
privtab.o: ../hdrs/dbdefs.h
privtab.o: ../hdrs/mushdb.h
privtab.o: ../hdrs/flags.h
privtab.o: ../hdrs/ptab.h
privtab.o: ../hdrs/division.h
privtab.o: ../hdrs/chunk.h
privtab.o: ../hdrs/bufferq.h
ptab.o: ../config.h
ptab.o: ../hdrs/copyrite.h
ptab.o: ../hdrs/conf.h
ptab.o: ../options.h
ptab.o: ../hdrs/mushtype.h
ptab.o: ../hdrs/htab.h
ptab.o: ../hdrs/externs.h
ptab.o: ../hdrs/compile.h
ptab.o: ../hdrs/dbdefs.h
ptab.o: ../hdrs/mushdb.h
ptab.o: ../hdrs/flags.h
ptab.o: ../hdrs/ptab.h
ptab.o: ../hdrs/division.h
ptab.o: ../hdrs/chunk.h
ptab.o: ../hdrs/bufferq.h
ptab.o: ../confmagic.h
rob.o: ../config.h
rob.o: ../hdrs/copyrite.h
rob.o: ../hdrs/conf.h
rob.o: ../options.h
rob.o: ../hdrs/mushtype.h
rob.o: ../hdrs/htab.h
rob.o: ../hdrs/externs.h
rob.o: ../hdrs/compile.h
rob.o: ../hdrs/dbdefs.h
rob.o: ../hdrs/mushdb.h
rob.o: ../hdrs/flags.h
rob.o: ../hdrs/ptab.h
rob.o: ../hdrs/division.h
rob.o: ../hdrs/chunk.h
rob.o: ../hdrs/bufferq.h
rob.o: ../confmagic.h
rob.o: ../hdrs/attrib.h
rob.o: ../hdrs/boolexp.h
rob.o: ../hdrs/command.h
rob.o: ../hdrs/switches.h
rob.o: ../hdrs/match.h
rob.o: ../hdrs/parse.h
rob.o: ../hdrs/log.h
rob.o: ../hdrs/lock.h
rob.o: ../hdrs/game.h
rob.o: ../hdrs/case.h
rplog.o: ../hdrs/copyrite.h
rplog.o: ../config.h
rplog.o: ../hdrs/conf.h
rplog.o: ../options.h
rplog.o: ../hdrs/mushtype.h
rplog.o: ../hdrs/htab.h
rplog.o: ../hdrs/externs.h
rplog.o: ../hdrs/compile.h
rplog.o: ../hdrs/dbdefs.h
rplog.o: ../hdrs/mushdb.h
rplog.o: ../hdrs/flags.h
rplog.o: ../hdrs/ptab.h
rplog.o: ../hdrs/division.h
rplog.o: ../hdrs/chunk.h
rplog.o: ../hdrs/bufferq.h
rplog.o: ../confmagic.h
rplog.o: ../hdrs/parse.h
rplog.o: ../hdrs/command.h
rplog.o: ../hdrs/boolexp.h
rplog.o: ../hdrs/switches.h
rplog.o: ../hdrs/cmds.h
rplog.o: ../hdrs/attrib.h
rplog.o: ../hdrs/match.h
rplog.o: ../hdrs/ansi.h
rplog.o: ../hdrs/log.h
set.o: ../hdrs/copyrite.h
set.o: ../config.h
set.o: ../hdrs/conf.h
set.o: ../options.h
set.o: ../hdrs/mushtype.h
set.o: ../hdrs/htab.h
set.o: ../hdrs/externs.h
set.o: ../hdrs/compile.h
set.o: ../hdrs/dbdefs.h
set.o: ../hdrs/mushdb.h
set.o: ../hdrs/flags.h
set.o: ../hdrs/ptab.h
set.o: ../hdrs/division.h
set.o: ../hdrs/chunk.h
set.o: ../hdrs/bufferq.h
set.o: ../confmagic.h
set.o: ../hdrs/game.h
set.o: ../hdrs/match.h
set.o: ../hdrs/attrib.h
set.o: ../hdrs/boolexp.h
set.o: ../hdrs/command.h
set.o: ../hdrs/switches.h
set.o: ../hdrs/ansi.h
set.o: ../hdrs/mymalloc.h
set.o: ../hdrs/lock.h
set.o: ../hdrs/log.h
shs.o: ../hdrs/copyrite.h
shs.o: ../config.h
sig.o: ../config.h
sig.o: ../hdrs/conf.h
sig.o: ../hdrs/copyrite.h
sig.o: ../options.h
sig.o: ../hdrs/mushtype.h
sig.o: ../hdrs/htab.h
sig.o: ../hdrs/externs.h
sig.o: ../hdrs/compile.h
sig.o: ../hdrs/dbdefs.h
sig.o: ../hdrs/mushdb.h
sig.o: ../hdrs/flags.h
sig.o: ../hdrs/ptab.h
sig.o: ../hdrs/division.h
sig.o: ../hdrs/chunk.h
sig.o: ../hdrs/bufferq.h
sig.o: ../confmagic.h
speech.o: ../hdrs/copyrite.h
speech.o: ../config.h
speech.o: ../hdrs/conf.h
speech.o: ../options.h
speech.o: ../hdrs/mushtype.h
speech.o: ../hdrs/htab.h
speech.o: ../hdrs/ansi.h
speech.o: ../hdrs/externs.h
speech.o: ../hdrs/compile.h
speech.o: ../hdrs/dbdefs.h
speech.o: ../hdrs/mushdb.h
speech.o: ../hdrs/flags.h
speech.o: ../hdrs/ptab.h
speech.o: ../hdrs/division.h
speech.o: ../hdrs/chunk.h
speech.o: ../hdrs/bufferq.h
speech.o: ../confmagic.h
speech.o: ../hdrs/lock.h
speech.o: ../hdrs/boolexp.h
speech.o: ../hdrs/log.h
speech.o: ../hdrs/match.h
speech.o: ../hdrs/attrib.h
speech.o: ../hdrs/command.h
speech.o: ../hdrs/switches.h
speech.o: ../hdrs/parse.h
speech.o: ../hdrs/game.h
speech.o: ../hdrs/pcre.h
sql.o: ../hdrs/copyrite.h
sql.o: ../config.h
sql.o: ../hdrs/conf.h
sql.o: ../options.h
sql.o: ../hdrs/mushtype.h
sql.o: ../hdrs/htab.h
sql.o: ../hdrs/externs.h
sql.o: ../hdrs/compile.h
sql.o: ../hdrs/dbdefs.h
sql.o: ../hdrs/mushdb.h
sql.o: ../hdrs/flags.h
sql.o: ../hdrs/ptab.h
sql.o: ../hdrs/division.h
sql.o: ../hdrs/chunk.h
sql.o: ../hdrs/bufferq.h
sql.o: ../confmagic.h
sql.o: ../hdrs/access.h
sql.o: ../hdrs/log.h
sql.o: ../hdrs/parse.h
sql.o: ../hdrs/boolexp.h
sql.o: ../hdrs/command.h
sql.o: ../hdrs/switches.h
sql.o: ../hdrs/function.h
strdup.o: ../config.h
strdup.o: ../hdrs/conf.h
strdup.o: ../hdrs/copyrite.h
strdup.o: ../options.h
strdup.o: ../hdrs/mushtype.h
strdup.o: ../hdrs/htab.h
strdup.o: ../hdrs/mymalloc.h
strdup.o: ../confmagic.h
strtree.o: ../hdrs/copyrite.h
strtree.o: ../config.h
strtree.o: ../hdrs/conf.h
strtree.o: ../options.h
strtree.o: ../hdrs/mushtype.h
strtree.o: ../hdrs/htab.h
strtree.o: ../hdrs/externs.h
strtree.o: ../hdrs/compile.h
strtree.o: ../hdrs/dbdefs.h
strtree.o: ../hdrs/mushdb.h
strtree.o: ../hdrs/flags.h
strtree.o: ../hdrs/ptab.h
strtree.o: ../hdrs/division.h
strtree.o: ../hdrs/chunk.h
strtree.o: ../hdrs/bufferq.h
strtree.o: ../confmagic.h
strtree.o: ../hdrs/strtree.h
strutil.o: ../config.h
strutil.o: ../hdrs/copyrite.h
strutil.o: ../hdrs/conf.h
strutil.o: ../options.h
strutil.o: ../hdrs/mushtype.h
strutil.o: ../hdrs/htab.h
strutil.o: ../hdrs/case.h
strutil.o: ../hdrs/ansi.h
strutil.o: ../hdrs/pueblo.h
strutil.o: ../hdrs/parse.h
strutil.o: ../confmagic.h
strutil.o: ../hdrs/externs.h
strutil.o: ../hdrs/compile.h
strutil.o: ../hdrs/dbdefs.h
strutil.o: ../hdrs/mushdb.h
strutil.o: ../hdrs/flags.h
strutil.o: ../hdrs/ptab.h
strutil.o: ../hdrs/division.h
strutil.o: ../hdrs/chunk.h
strutil.o: ../hdrs/bufferq.h
strutil.o: ../hdrs/mymalloc.h
strutil.o: ../hdrs/log.h
timer.o: ../hdrs/copyrite.h
timer.o: ../config.h
timer.o: ../hdrs/conf.h
timer.o: ../options.h
timer.o: ../hdrs/mushtype.h
timer.o: ../hdrs/htab.h
timer.o: ../hdrs/externs.h
timer.o: ../hdrs/compile.h
timer.o: ../hdrs/dbdefs.h
timer.o: ../hdrs/mushdb.h
timer.o: ../hdrs/flags.h
timer.o: ../hdrs/ptab.h
timer.o: ../hdrs/division.h
timer.o: ../hdrs/chunk.h
timer.o: ../hdrs/bufferq.h
timer.o: ../confmagic.h
timer.o: ../hdrs/attrib.h
timer.o: ../hdrs/boolexp.h
timer.o: ../hdrs/command.h
timer.o: ../hdrs/switches.h
timer.o: ../hdrs/lock.h
timer.o: ../hdrs/extmail.h
timer.o: ../hdrs/match.h
timer.o: ../hdrs/access.h
timer.o: ../hdrs/log.h
timer.o: ../hdrs/game.h
timer.o: ../hdrs/help.h
timer.o: ../hdrs/parse.h
unparse.o: ../hdrs/copyrite.h
unparse.o: ../config.h
unparse.o: ../hdrs/conf.h
unparse.o: ../options.h
unparse.o: ../hdrs/mushtype.h
unparse.o: ../hdrs/htab.h
unparse.o: ../hdrs/externs.h
unparse.o: ../hdrs/compile.h
unparse.o: ../hdrs/dbdefs.h
unparse.o: ../hdrs/mushdb.h
unparse.o: ../hdrs/flags.h
unparse.o: ../hdrs/ptab.h
unparse.o: ../hdrs/division.h
unparse.o: ../hdrs/chunk.h
unparse.o: ../hdrs/bufferq.h
unparse.o: ../confmagic.h
unparse.o: ../hdrs/lock.h
unparse.o: ../hdrs/boolexp.h
unparse.o: ../hdrs/attrib.h
unparse.o: ../hdrs/command.h
unparse.o: ../hdrs/switches.h
unparse.o: ../hdrs/ansi.h
unparse.o: ../hdrs/pueblo.h
unparse.o: ../hdrs/parse.h
utils.o: ../hdrs/copyrite.h
utils.o: ../config.h
utils.o: ../hdrs/conf.h
utils.o: ../options.h
utils.o: ../hdrs/mushtype.h
utils.o: ../hdrs/htab.h
utils.o: ../hdrs/match.h
utils.o: ../hdrs/externs.h
utils.o: ../hdrs/compile.h
utils.o: ../hdrs/dbdefs.h
utils.o: ../hdrs/mushdb.h
utils.o: ../hdrs/flags.h
utils.o: ../hdrs/ptab.h
utils.o: ../hdrs/division.h
utils.o: ../hdrs/chunk.h
utils.o: ../hdrs/bufferq.h
utils.o: ../confmagic.h
utils.o: ../hdrs/mymalloc.h
utils.o: ../hdrs/log.h
utils.o: ../hdrs/attrib.h
utils.o: ../hdrs/boolexp.h
utils.o: ../hdrs/command.h
utils.o: ../hdrs/switches.h
utils.o: ../hdrs/parse.h
utils.o: ../hdrs/lock.h
version.o: ../config.h
version.o: ../hdrs/copyrite.h
version.o: ../hdrs/conf.h
version.o: ../options.h
version.o: ../hdrs/mushtype.h
version.o: ../hdrs/htab.h
version.o: ../hdrs/externs.h
version.o: ../hdrs/compile.h
version.o: ../hdrs/dbdefs.h
version.o: ../hdrs/mushdb.h
version.o: ../hdrs/flags.h
version.o: ../hdrs/ptab.h
version.o: ../hdrs/division.h
version.o: ../hdrs/chunk.h
version.o: ../hdrs/bufferq.h
version.o: ../confmagic.h
version.o: ../hdrs/version.h
version.o: ../hdrs/patches.h
version.o: ../hdrs/buildinf.h
warnings.o: ../config.h
warnings.o: ../hdrs/copyrite.h
warnings.o: ../hdrs/conf.h
warnings.o: ../options.h
warnings.o: ../hdrs/mushtype.h
warnings.o: ../hdrs/htab.h
warnings.o: ../hdrs/externs.h
warnings.o: ../hdrs/compile.h
warnings.o: ../hdrs/dbdefs.h
warnings.o: ../hdrs/mushdb.h
warnings.o: ../hdrs/flags.h
warnings.o: ../hdrs/ptab.h
warnings.o: ../hdrs/division.h
warnings.o: ../hdrs/chunk.h
warnings.o: ../hdrs/bufferq.h
warnings.o: ../confmagic.h
warnings.o: ../hdrs/lock.h
warnings.o: ../hdrs/boolexp.h
warnings.o: ../hdrs/match.h
warnings.o: ../hdrs/attrib.h
warnings.o: ../hdrs/command.h
warnings.o: ../hdrs/switches.h
wild.o: ../config.h
wild.o: ../hdrs/copyrite.h
wild.o: ../hdrs/conf.h
wild.o: ../options.h
wild.o: ../hdrs/mushtype.h
wild.o: ../hdrs/htab.h
wild.o: ../hdrs/case.h
wild.o: ../hdrs/externs.h
wild.o: ../hdrs/compile.h
wild.o: ../hdrs/dbdefs.h
wild.o: ../hdrs/mushdb.h
wild.o: ../hdrs/flags.h
wild.o: ../hdrs/ptab.h
wild.o: ../hdrs/division.h
wild.o: ../hdrs/chunk.h
wild.o: ../hdrs/bufferq.h
wild.o: ../confmagic.h
wild.o: ../hdrs/mymalloc.h
wild.o: ../hdrs/parse.h
wild.o: ../hdrs/pcre.h
wiz.o: ../hdrs/copyrite.h
wiz.o: ../config.h
wiz.o: ../hdrs/conf.h
wiz.o: ../options.h
wiz.o: ../hdrs/mushtype.h
wiz.o: ../hdrs/htab.h
wiz.o: ../hdrs/externs.h
wiz.o: ../hdrs/compile.h
wiz.o: ../hdrs/dbdefs.h
wiz.o: ../hdrs/mushdb.h
wiz.o: ../hdrs/flags.h
wiz.o: ../hdrs/ptab.h
wiz.o: ../hdrs/division.h
wiz.o: ../hdrs/chunk.h
wiz.o: ../hdrs/bufferq.h
wiz.o: ../confmagic.h
wiz.o: ../hdrs/attrib.h
wiz.o: ../hdrs/boolexp.h
wiz.o: ../hdrs/command.h
wiz.o: ../hdrs/switches.h
wiz.o: ../hdrs/match.h
wiz.o: ../hdrs/access.h
wiz.o: ../hdrs/parse.h
wiz.o: ../hdrs/mymalloc.h
wiz.o: ../hdrs/lock.h
wiz.o: ../hdrs/log.h
wiz.o: ../hdrs/game.h
wiz.o: ../hdrs/extmail.h
../hdrs/atr_tab.o: ../hdrs/attrib.h
../hdrs/atr_tab.o: ../hdrs/mushtype.h
../hdrs/atr_tab.o: ../hdrs/copyrite.h
../hdrs/atr_tab.o: ../options.h
../hdrs/atr_tab.o: ../hdrs/boolexp.h
../hdrs/atr_tab.o: ../hdrs/chunk.h
../hdrs/atr_tab.o: ../hdrs/command.h
../hdrs/atr_tab.o: ../hdrs/division.h
../hdrs/atr_tab.o: ../hdrs/ptab.h
../hdrs/atr_tab.o: ../hdrs/switches.h
../hdrs/attrib.o: ../hdrs/mushtype.h
../hdrs/attrib.o: ../hdrs/copyrite.h
../hdrs/attrib.o: ../options.h
../hdrs/attrib.o: ../hdrs/boolexp.h
../hdrs/attrib.o: ../hdrs/chunk.h
../hdrs/attrib.o: ../hdrs/command.h
../hdrs/attrib.o: ../hdrs/division.h
../hdrs/attrib.o: ../hdrs/ptab.h
../hdrs/attrib.o: ../hdrs/switches.h
../hdrs/boolexp.o: ../hdrs/copyrite.h
../hdrs/boolexp.o: ../hdrs/chunk.h
../hdrs/boolexp.o: ../hdrs/mushtype.h
../hdrs/boolexp.o: ../options.h
../hdrs/case.o: ../config.h
../hdrs/case.o: ../config.h
../hdrs/chunk.o: ../hdrs/mushtype.h
../hdrs/chunk.o: ../hdrs/copyrite.h
../hdrs/chunk.o: ../options.h
../hdrs/command.o: ../hdrs/boolexp.h
../hdrs/command.o: ../hdrs/copyrite.h
../hdrs/command.o: ../hdrs/chunk.h
../hdrs/command.o: ../hdrs/mushtype.h
../hdrs/command.o: ../options.h
../hdrs/command.o: ../hdrs/division.h
../hdrs/command.o: ../hdrs/ptab.h
../hdrs/command.o: ../hdrs/switches.h
../hdrs/conf.o: ../hdrs/copyrite.h
../hdrs/conf.o: ../options.h
../hdrs/conf.o: ../hdrs/mushtype.h
../hdrs/conf.o: ../hdrs/htab.h
../hdrs/dbdefs.o: ../hdrs/mushdb.h
../hdrs/dbdefs.o: ../config.h
../hdrs/dbdefs.o: ../hdrs/copyrite.h
../hdrs/dbdefs.o: ../hdrs/flags.h
../hdrs/dbdefs.o: ../hdrs/mushtype.h
../hdrs/dbdefs.o: ../options.h
../hdrs/dbdefs.o: ../hdrs/ptab.h
../hdrs/dbdefs.o: ../hdrs/division.h
../hdrs/dbdefs.o: ../hdrs/htab.h
../hdrs/dbdefs.o: ../hdrs/chunk.h
../hdrs/dbdefs.o: ../hdrs/bufferq.h
../hdrs/externs.o: ../config.h
../hdrs/externs.o: ../hdrs/copyrite.h
../hdrs/externs.o: ../hdrs/compile.h
../hdrs/externs.o: ../hdrs/mushtype.h
../hdrs/externs.o: ../options.h
../hdrs/externs.o: ../hdrs/dbdefs.h
../hdrs/externs.o: ../hdrs/mushdb.h
../hdrs/externs.o: ../hdrs/flags.h
../hdrs/externs.o: ../hdrs/ptab.h
../hdrs/externs.o: ../hdrs/division.h
../hdrs/externs.o: ../hdrs/htab.h
../hdrs/externs.o: ../hdrs/chunk.h
../hdrs/externs.o: ../hdrs/bufferq.h
../hdrs/externs.o: ../confmagic.h
../hdrs/flags.o: ../hdrs/mushtype.h
../hdrs/flags.o: ../hdrs/copyrite.h
../hdrs/flags.o: ../options.h
../hdrs/flags.o: ../hdrs/ptab.h
../hdrs/flags.o: ../hdrs/division.h
../hdrs/function.o: ../hdrs/copyrite.h
../hdrs/function.o: ../hdrs/boolexp.h
../hdrs/function.o: ../hdrs/chunk.h
../hdrs/function.o: ../hdrs/mushtype.h
../hdrs/function.o: ../options.h
../hdrs/ident.o: ../config.h
../hdrs/ident.o: ../hdrs/mysocket.h
../hdrs/ident.o: ../hdrs/copyrite.h
../hdrs/ident.o: ../confmagic.h
../hdrs/lock.o: ../hdrs/copyrite.h
../hdrs/lock.o: ../hdrs/mushtype.h
../hdrs/lock.o: ../options.h
../hdrs/lock.o: ../hdrs/conf.h
../hdrs/lock.o: ../hdrs/htab.h
../hdrs/lock.o: ../hdrs/boolexp.h
../hdrs/lock.o: ../hdrs/chunk.h
../hdrs/match.o: ../hdrs/copyrite.h
../hdrs/mushdb.o: ../config.h
../hdrs/mushdb.o: ../hdrs/copyrite.h
../hdrs/mushdb.o: ../hdrs/flags.h
../hdrs/mushdb.o: ../hdrs/mushtype.h
../hdrs/mushdb.o: ../options.h
../hdrs/mushdb.o: ../hdrs/ptab.h
../hdrs/mushdb.o: ../hdrs/division.h
../hdrs/mushtype.o: ../hdrs/copyrite.h
../hdrs/mushtype.o: ../options.h
../hdrs/mymalloc.o: ../options.h
../hdrs/mysocket.o: ../hdrs/copyrite.h
../hdrs/mysocket.o: ../config.h
../hdrs/mysocket.o: ../confmagic.h
../hdrs/myssl.o: ../hdrs/copyrite.h
../hdrs/parse.o: ../hdrs/copyrite.h
../hdrs/parse.o: ../config.h
../hdrs/parse.o: ../confmagic.h
../hdrs/privtab.o: ../hdrs/copyrite.h
../hdrs/privtab.o: ../config.h
../hdrs/privtab.o: ../confmagic.h
../hdrs/division.o: ../hdrs/ptab.h
../hdrs/cron.o: ../hdrs/conf.h
../hdrs/cron.o: ../hdrs/copyrite.h
../hdrs/cron.o: ../options.h
../hdrs/cron.o: ../hdrs/mushtype.h
../hdrs/cron.o: ../hdrs/htab.h
../hdrs/cron.o: ../hdrs/externs.h
../hdrs/cron.o: ../config.h
../hdrs/cron.o: ../hdrs/compile.h
../hdrs/cron.o: ../hdrs/dbdefs.h
../hdrs/cron.o: ../hdrs/mushdb.h
../hdrs/cron.o: ../hdrs/flags.h
../hdrs/cron.o: ../hdrs/ptab.h
../hdrs/cron.o: ../hdrs/division.h
../hdrs/cron.o: ../hdrs/chunk.h
../hdrs/cron.o: ../hdrs/bufferq.h
../hdrs/cron.o: ../confmagic.h
../hdrs/cron.o: ../hdrs/command.h
../hdrs/cron.o: ../hdrs/boolexp.h
../hdrs/cron.o: ../hdrs/switches.h
