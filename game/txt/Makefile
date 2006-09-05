#
# This makefile only rebuilds the text files we need it
#
# By default we build help, news, and events.
# To build rules.txt:
#	add rules.txt to the TXT line
# Do the same to build index.txt (but add index.txt)

TXT=help.txt news.txt events.txt

# INDEX_FLAGS can be set to one or more of:
#  --first     Insert the first entry alias in the index
#  --longest   Insert the longest entry alias in the index
# If left blank, all aliases are indexed. This is the default behavior.
# (By default, this variable is not used in making the help index,
# but if you want it to be, you can figure out how from the examples
# below.)
INDEX_FLAGS=

all: $(TXT)

help.txt: hlp/*.hlp hlp compose.sh
	./compose.sh hlp
	mv hlp.txt help.txt
	chmod 660 help.txt

news.txt: nws/*.nws nws compose.sh
	./compose.sh nws $(INDEX_FLAGS)
	mv nws.txt news.txt
	chmod 660 news.txt

events.txt: evt/*.evt evt compose.sh
	./compose.sh evt $(INDEX_FLAGS)
	mv evt.txt events.txt
	chmod 660 events.txt

rules.txt: rules/*.rules rules compose.sh
	./compose.sh rules $(INDEX_FLAGS)
	chmod 660 events.txt

index.txt: index/*.index index compose.sh
	./compose.sh index $(INDEX_FLAGS)
	chmod 660 *.txt
clean:
	-rm -f $(IDX) $(TXT)
	-rm -f compose.sh
	-rm -f hlp/*.orig hlp/*.rej hlp/\#* hlp/*~

compose.sh: compose.sh.SH
	sh compose.sh.SH

