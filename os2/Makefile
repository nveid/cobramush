# Makefile for PennMUSH 

# - System configuration - #

#
# This section of the file should be automatically configured by
# the Configure script. If it doesn't work, you might try starting
# from the Makefile.old that's included instead, and reporting
# your problem (including this Makefile) to Paul/Javelin,
# dunemush@pennmush.org
#

CC=gcc
CCFLAGS=-g -O -W -Wreturn-type -Wswitch -Wshadow -Wwrite-strings -Wformat -Wparentheses -Wuninitialized -I.. -I../hdrs
LDFLAGS=
CLIBS=-lbsd -lsocket -lcrypt 
LNS=/bin/ln


# stupid SYS V shell
SHELL=/bin/sh


all: dune.h options.h hdrs/cmds.h hdrs/funs.h game/mush.cnf
	@echo "Making all in src."
	(cd src; make all "CC=$(CC)" "CCFLAGS=$(CCFLAGS)" \
	"LDFLAGS=$(LDFLAGS)" "CLIBS=$(CLIBS)" )
	@echo "If the make was successful, use 'make install' to install links"

dune.h: dune.h.dist
	@echo "Please use 'make update' to update your dune.h file from dune.h.dist"
	@echo "Or you may cp dune.h.dist to dune.h and edit it."
	exit 1

options.h: options.h.dist
	@echo "Please use 'make update' to update your options.h file from options.h.dist"
	@echo "You must cp options.h.dist to options.h and edit it."
	exit 1

hdrs/cmds.h: src/SWITCHES src/cmds.c
	(cd utils; sh mkcmds.sh)

hdrs/funs.h: src/fun*.c
	(cd utils; sh mkcmds.sh)

install: all
	-rm -f game/netmush
	-rm -f game/info_slave
	(cd game; \
	$(LNS) ../src/netmud netmush; \
	$(LNS) ../src/info_slave info_slave)
	@echo "If you plan to run multiple MUSHes, consider running 'make customize'"

netmud: 
	(cd src; make netmud "CC=$(CC)" "CCFLAGS=$(CCFLAGS)" \
	"LDFLAGS=$(LDFLAGS)" "CLIBS=$(CLIBS)" )

access:
	utils/make_access_cnf.sh game

portmsg:
	(cd src; make portmsg "CC=$(CC)" "CCFLAGS=$(CCFLAGS)" \
	"LDFLAGS=$(LDFLAGS)" "CLIBS=$(CLIBS)" )

concentrate:
	(cd src; make concentrate "CC=$(CC)" "CCFLAGS=$(CCFLAGS)" \
	"LDFLAGS=$(LDFLAGS)" "CLIBS=$(CLIBS)" )

install_conc: concentrate
	-rm -f game/concentrate
	(cd game; $(LNS) ../src/concentrate concentrate)

announce:
	(cd src; make announce "CC=$(CC)" "CCFLAGS=$(CCFLAGS)" \
	"LDFLAGS=$(LDFLAGS)" "CLIBS=$(CLIBS)" )

ident_made:
	(cd src/IDENT; make CC="$(CC)" CCFLAGS="$(CCFLAGS)")
	(cd src; touch ident_made)

dump:
	(cd src; make dump "CC=$(CC)" "CCFLAGS=$(CCFLAGS)" \
	"LDFLAGS=$(LDFLAGS)" "CLIBS=$(CLIBS)" )
 
extract:
	(cd src; make extract "CC=$(CC)" "CCFLAGS=$(CCFLAGS)" \
	"LDFLAGS=$(LDFLAGS)" "CLIBS=$(CLIBS)" )

decompress:
	(cd src; make decompress "CC=$(CC)" "CCFLAGS=$(CCFLAGS)" \
	"LDFLAGS=$(LDFLAGS)" "CLIBS=$(CLIBS)" )

safety:
	cp src/*.c /var/pennmush-bak/src
	cp hdrs/*.h /var/pennmush-bak/hdrs
	cp * /var/pennmush-bak

distdepend: 
	(cd src; make depend "CC=$(CC)" "CCFLAGS=$(CCFLAGS)" \
	"LDFLAGS=$(LDFLAGS)" "CLIBS=$(CLIBS)" )

# REQUIRES GNU INDENT! DON'T INDENT WITH ANYTHING ELSE!
indent:
	(cd src; make indent)

protoize:
	(cd src; make protoize "CCFLAGS=$(CCFLAGS)")


customize: update-conf
	-@perl utils/customize.pl

update-conf: game/mushcnf.dst
	-@touch game/mushcnf.dst
	-@perl utils/update-cnf.pl game/mush.cnf game/mushcnf.dst

game/mush.cnf: game/mushcnf.dst
	-@touch game/mushcnf.dst
	-@perl utils/update-cnf.pl game/mush.cnf game/mushcnf.dst

update: update-hdr update-conf

update-hdr:
	-@touch options.h.dist dune.h.dist
	-@perl utils/update.pl options.h options.h.dist
	-@perl utils/update.pl dune.h dune.h.dist


clean:
	(cd src; make clean)
	(cd game; rm -f netmush)

distclean: 
	(cd src; make distclean)
	(cd hdrs; rm -f *.orig *~ \#* *.rej *.bak funs.h cmds.h)
	(cd utils; rm -f *.orig *~ \#* *.rej *.bak)
	(cd game; rm -rf *.log netmush *.orig *.rej *~ *.bak mush.cnf)
	(cd win32; rm -rf *.rej *.orig *~ *.bak)
	(cd game/txt; make clean)

totallyclean: distclean 
	(cd hdrs; rm -rf *.rej)
	(cd src; rm -rf *.rej)
	-rm -f Makefile

distci: distclean ci-src ci-game

ci-src:
	-(yes . | ci -l -f -N$(NAME) FAQ* BUGS CHANGE* READ* Configure \
	  configure utils/* Makefile.SH Patchlevel config_h.SH confmagic.h \
	  *.dist src/Makefile src/IDENT/*  src/*.c src/*.dst \
	  hdrs/* hints/* win32/*)

ci-game:
	-(yes . | ci -l -f -N$(NAME) game/restart game/mushcnf.dst \
	  game/access.README \
	  game/txt/* game/txt/nws/* game/txt/evt/* game/txt/hlp/* )

diffs: 
	-rm hdrs/buildinf.h src/funlocal.c src/cmdlocal.c src/local.c hdrs/funs.h hdrs/cmds.h
	-(rcsdiff -c -r$(OLD) Patchlevel FAQ* BUGS CHANGE* READ* Configure \
	  configure utils/*.sh utils/*.pl \
	  Makefile.SH win32/* config_h.SH confmagic.h \
	  *.dist src/Makefile src/IDENT/*  src/*.c src/*.dst \
	  src/SWITCHES hdrs/*.h hints/* \
	  game/restart game/mushcnf.dst game/txt/hlp/pen*.hlp | \
	grep -v "No differences encountered" > /tmp/diffs)

etags: 
	(cd src; make etags)

ctags:
	(cd src; make ctags)

dist.tar.Z: distclean dist.tar
	compress /tmp/dist.tar

dist.tar.gz: distclean dist.tar
	gzip /tmp/dist.tar

dist.tar: distclean
	(tar -cvFFfX /tmp/dist.tar exclude.tar \
		Patchlevel FAQ* BUGS CHANGE* READ* Configure configure \
		Makefile.SH config_h.SH confmagic.h \
		options.h.dist dune.h.dist src hdrs game hints utils win32)
	(pgp -sb /tmp/dist.tar)

CSRImalloc.tar.Z:
	(cd src/CSRI; make clean)
	(tar -cvFFf /tmp/CSRImalloc.tar `cat exclude.tar`)
	compress /tmp/CSRImalloc.tar

