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
  INSTALL_LINKS="$lns ../src/netmud.exe netmush"
else
  INSTALL_LINKS="$lns ../src/netmud netmush; \
                 $lns ../src/console console; \
                 $lns ../src/info_slave info_slave"
fi

: This section of the file will have variable substitutions done on it.
: Move anything that needs config subs from !NO!SUBS! section to !GROK!THIS!.
: Protect any dollar signs and backticks that you do not want interpreted
: by putting a backslash in front.  You may delete these comments.
$spitshell >Makefile <<!GROK!THIS!
# Makefile for CobraMUSH 

# - System configuration - #

#
# This section of the file should be automatically configured by
# the Configure script. If it doesn't work, you might try starting
# from the Makefile.old that's included instead, and reporting
# your problem (including this Makefile) to devteam@cobramush.org
#
# If you want to profile the code, add -pg -a -DPROFILING to CCFLAGS
# and (probably) remove -O
#
# If you want to do a less paranoid compile, remove the following from CCFLAGS:
# -Wall -Wno-comment -Wno-parentheses -Wno-switch -Werror
#
$make_set_make
CC=$cc
CCFLAGS=$optimize -I.. -I../hdrs -Wall -WNo-comment -Wno-parentheses -Wno-switch -Werror $ccflags $warnings
LDFLAGS=$ldflags
CLIBS=$libs $cryptlib $libssl $libcrypto $libmysqlclient
INSTALL=$install
INSTALLDIR=$installdir
CP=$cp
CHMOD=$chmod
INSTALL_LINKS=$INSTALL_LINKS
!GROK!THIS!

: In the following dollars and backticks do not need the extra backslash.
$spitshell >>Makefile <<'!NO!SUBS!'


# stupid SYS V shell
SHELL=/bin/sh
# Where to install with 'make globalinstall'
GLOBAL_INSTALL=/usr/libexec/cobramush

# Where to install with 'make debianinstall'
DEB_INSTALL=$(DESTDIR)/usr/lib/cobramush/game
DEB_BIN=$(DESTDIR)/usr/games

all: config.h options.h autogen game/mush.cnf
	@echo "Making all in src."
	(cd src; make all "CC=$(CC)" "CCFLAGS=$(CCFLAGS)" \
	"LDFLAGS=$(LDFLAGS)" "CLIBS=$(CLIBS)" "MAKE=$(MAKE)" \
	"MAKEFLAGS=$(MAKEFLAGS)")
	@echo "If the make was successful, use 'make install' to install links."

config.h: Configure
	@echo "Looks like your Configure has been updated."
	@echo "Run that first."
	exit 1

options.h: options.h.dist
	@echo "Please use 'make update' to update your options.h file from options.h.dist"
	@echo "You must cp options.h.dist to options.h and edit it."
	exit 1

autogen: hdrs/cmds.h hdrs/funs.h hdrs/switches.h

hdrs/cmds.h: src/cmds.c src/command.c src/cque.c src/help.c src/set.c src/sql.c Patchlevel
	(perl utils/mkcmds.pl commands)

hdrs/switches.h: src/SWITCHES Patchlevel
	(perl utils/mkcmds.pl switches)

src/switchinc.c: src/SWITCHES Patchlevel
	(perl utils/mkcmds.pl switches)

hdrs/funs.h: src/fun*.c src/bsd.c src/conf.c src/cque.c src/extmail.c src/help.c src/wiz.c src/sql.c Patchlevel
	(perl utils/mkcmds.pl functions)

install: localized all game/restart
	-rm -f game/netmush
	-rm -f game/console
	-rm -f game/info_slave
	(cd game; $(INSTALL_LINKS))
	(cd game/txt; make)
	@echo "If you plan to run multiple MUSHes, consider running 'make customize'"

netmud: 
	(cd src; make netmud "CC=$(CC)" "CCFLAGS=$(CCFLAGS)" \
	"LDFLAGS=$(LDFLAGS)" "CLIBS=$(CLIBS)" )

console:
	(cd src; make console "CC=$(CC)" "CCFLAGS=$(CCFLAGS)" \
	"LDFLAGS=$(LDFLAGS)" "CLIBS=$(CLIBS)" )

access:
	utils/make_access_cnf.sh game

cobramush.pot:
	(cd src; make ../po/cobramush.pot)

localized:
	-echo "Localizing for your locale..."
	-(cd po; make localized)

portmsg:
	(cd src; make portmsg "CC=$(CC)" "CCFLAGS=$(CCFLAGS)" \
	"LDFLAGS=$(LDFLAGS)" "CLIBS=$(CLIBS)" )

announce:
	(cd src; make announce "CC=$(CC)" "CCFLAGS=$(CCFLAGS)" \
	"LDFLAGS=$(LDFLAGS)" "CLIBS=$(CLIBS)" )

versions: game/txt/changes/*   
	-@rm -rf game/txt/changes.txt
	(cd game/txt && ruby genchanges.rb) 

safety:
	$(CP) src/*.c /var/cobramush-bak/src
	$(CP) hdrs/*.h /var/cobramush-bak/hdrs
	$(CP) * /var/cobramush-bak

distdepend: hdrs/funs.h hdrs/cmds.h
	(cd src; make depend "CC=$(CC)" "CCFLAGS=$(CCFLAGS)" \
	"LDFLAGS=$(LDFLAGS)" "CLIBS=$(CLIBS)" )

# REQUIRES GNU INDENT! DON'T INDENT WITH ANYTHING ELSE!
indent:
#	@(cd src; make indent)

protoize:
	(cd src; make protoize "CCFLAGS=$(CCFLAGS)")

!NO!SUBS!

: This section of the file will have variable substitutions done on it.
: Move anything that needs config subs from !NO!SUBS! section to !GROK!THIS!.
: Protect any dollar signs and backticks that you do not want interpreted
: by putting a backslash in front.  You may delete these comments.
$spitshell >>Makefile <<!GROK!THIS!

customize: update-conf
	-@$perl utils/customize.pl

# The default place to find the runtime files is in this directory,
# but it can be overridden with env variables so people can use
# other game directories.
GAMEDIR=game

update-conf: game/mushcnf.dst game/aliascnf.dst game/restrictcnf.dst game/namescnf.dst game/restart.dst
	-@$touch game/mushcnf.dst
	-@$perl utils/update-cnf.pl \$(GAMEDIR)/mush.cnf game/mushcnf.dst
	-@$touch game/aliascnf.dst
	-@$perl utils/update-cnf.pl \$(GAMEDIR)/alias.cnf game/aliascnf.dst
	-@$touch game/restrictcnf.dst
	-@$perl utils/update-cnf.pl \$(GAMEDIR)/restrict.cnf game/restrictcnf.dst
	-@if [ ! -f \$(GAMEDIR)/names.cnf ]; then \$(CP) game/namescnf.dst \$(GAMEDIR)/names.cnf; fi
	-@if [ ! -f \$(GAMEDIR)/restart ]; then \$(CP) game/restart.dst \$(GAMEDIR)/restart; chmod 755 \$(GAMEDIR)/restart; fi

\$(GAMEDIR)/alias.cnf: game/aliascnf.dst
	-@$touch game/aliascnf.dst
	-@$perl utils/update-cnf.pl \$(GAMEDIR)/alias.cnf game/aliascnf.dst

\$(GAMEDIR)/restrict.cnf: game/restrictcnf.dst
	-@$touch game/restrictcnf.dst
	-@$perl utils/update-cnf.pl \$(GAMEDIR)/restrict.cnf game/restrictcnf.dst

\$(GAMEDIR)/names.cnf: game/namescnf.dst
	if [ ! -f \$(GAMEDIR)/names.cnf ]; then \
		\$(CP) game/namescnf.dst \$(GAMEDIR)/names.cnf; \
	fi

\$(GAMEDIR)/restart: game/restart.dst
	if [ ! -f \$(GAMEDIR)/restart ]; then \
		\$(CP) game/restart.dst \$(GAMEDIR)/restart ; \
		chmod 755 \$(GAMEDIR)/restart ; \
	fi

\$(GAMEDIR)/mush.cnf: game/mushcnf.dst
	-@$touch game/mushcnf.dst
	-@$perl utils/update-cnf.pl \$(GAMEDIR)/mush.cnf game/mushcnf.dst

update: update-hdr update-conf

update-hdr:
	-@$touch options.h.dist
	-@$perl utils/update.pl options.h options.h.dist

test: netmud
	(cd test; $perl alltests.pl)

!GROK!THIS!

: In the following dollars and backticks do not need the extra backslash.
$spitshell >>Makefile <<'!NO!SUBS!'

clean:
	(cd src; make clean)
	(cd game; rm -f netmush console info_slave)

distclean: 
	(cd hdrs; rm -f *.orig *~ \#* *.rej *.bak funs.h cmds.h buildinf.h patches.h)
	(cd utils; rm -f *.orig *~ \#* *.rej *.bak)
	(cd game; rm -rf *.log netmush info_slave *.orig *.rej *~ *.bak mush.cnf)
	(cd os2; rm -rf *.rej *.orig *~ *.bak)
	(cd src; make distclean; rm -f Makefile)
	(cd game/txt; make clean)
	(rm -rf .config Makefile config.h config.sh options.h)

totallyclean: distclean 
	(cd hdrs; rm -rf *.rej)
	(cd src; rm -rf *.rej)
	-rm -f Makefile

distci: distclean ci-src ci-game

ci-src:
	-(yes . | ci -l -f -N$(NAME) FAQ* BUGS COPY* CHANGE* READ* MANIFEST \
	  Configure utils/* Makefile.SH Patchlevel config_h.SH confmagic.h \
	  *.dist src/Makefile src/SWITCHES src/*.c src/*.dst \
	  hdrs/* hints/* os2/*)

ci-game:
	-(yes . | ci -l -f -N$(NAME) game/restart game/mushcnf.dst \
	  game/access.README \
	  game/txt/* game/txt/nws/* game/txt/evt/* game/txt/hlp/* )

diffs:
	@make indent > /dev/null 2>&1
	@make versions > /dev/null 2>&1
	@make touchswitches > /dev/null 2>&1
	@make autogen > /dev/null 2>&1
	@(prcs diff -r$(VS) -N cobramush `cat MANIFEST` | grep -v 'Index:')

checkin: versions autogen
	@prcs checkin

patch: versions
	@make-patch-header
	@make diffs

etags: 
	(cd src; make etags)

ctags:
	(cd src; make ctags)

dist.tar.Z: distclean dist.tar
	compress /tmp/dist.tar

dist.tar.gz: distclean dist.tar
	gzip /tmp/dist.tar

touchswitches:
	@touch src/SWITCHES

dist.tar: indent distclean versions touchswitches autogen
	makedist -c cobramush
	tar -cvf /tmp/dist.tar cobramush
# CobraMUSH does not pgp sign distributions at this moment
#	-pgp -sb /tmp/dist.tar
	-rm -rf cobramush

globalinstall: install
	(cd game/txt; make clean compose.sh)
	$(INSTALLDIR) $(GLOBAL_INSTALL)
	$(CP) -R game/* $(GLOBAL_INSTALL)
	rm -f $(GLOBAL_INSTALL)/netmush $(GLOBAL_INSTALL)/info_slave
	rm -f $(GLOBAL_INSTALL)/console
	$(INSTALL) config.sh $(GLOBAL_INSTALL)/config.sh
	$(INSTALL) src/netmud $(GLOBAL_INSTALL)/netmush
	$(INSTALL) src/console $(GLOBAL_INSTALL)/console
	$(INSTALL) src/info_slave utils/ln-dir.sh $(GLOBAL_INSTALL)
	$(CHMOD) a+rX -R $(GLOBAL_INSTALL)
	@echo "** Files installed in $(GLOBAL_INSTALL). Feel free to move them."
	@echo "** You can run $(GLOBAL_INSTALL)/ln-dir.sh to create a user directory,"
	@echo "** or symlink that to somewhere easier to run. You may wish to strip them."

debianinstall: install
	(cd game/txt; make clean compose.sh)
	$(INSTALLDIR) $(DEB_INSTALL)
	$(INSTALLDIR) $(DEB_BIN)
	$(CP) -R game/* $(DEB_INSTALL)
	-rm -f $(DEB_INSTALL)/netmush $(DEB_INSTALL)/info_slave
	-rm -f $(DEB_INSTALL)/console
	$(INSTALL) config.sh $(DEB_INSTALL)/config.sh
	$(INSTALL) src/netmud $(DEB_INSTALL)/netmush
	$(INSTALL) src/console $(DEB_INSTALL)/console
	$(INSTALL) src/info_slave $(DEB_INSTALL)/info_slave
	$(INSTALL) utils/cobramush-install $(DEB_BIN)/cobramush-install
	$(CHMOD) a+rX -R $(DEB_INSTALL)
	$(CHMOD) a+rX $(DEB_BIN)/cobramush-install
	@echo "** Files installed in $(DEB_INSTALL)."
	@echo "** You can run cobramush-install to create a user directory."

!NO!SUBS!
chmod 644 Makefile
