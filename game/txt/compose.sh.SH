#!/bin/sh
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
echo "Extracting compose.sh (with variable substitutions)"
: This section of the file will have variable substitutions done on it.
: Move anything that needs config subs from !NO!SUBS! section to !GROK!THIS!.
: Protect any dollar signs and backticks that you do not want interpreted
: by putting a backslash in front.  You may delete these comments.
$spitshell >compose.sh <<!GROK!THIS!
#!/bin/sh
#
# compose.sh: a shell script for putting together help.txt, etc.
#
# Usage: compose.sh <directory>
# Example: compose.sh help
#
# This script calls index-files.pl
# 
# By Alan Schwartz (Javelin/Paul)
#

# These come from Configure
perl=${perl-none}
test=$test
cat=$cat
rm=$rm
echo=$echo

!GROK!THIS!

: In the following dollars and backticks do not need the extra backslash.
$spitshell >>compose.sh <<'!NO!SUBS!'
# This process can eat CPU, so uncomment if you want to be nice
#/etc/renice +4 $$

# What subdirectories should we be processing?
dir=$1
if $test ! -d $dir; then
  $echo "Usage: compose.sh <directory>"
  exit 0
fi

index_args=$2

# Ok, let's do 'em:
  cd $dir

  # Remove the old index
  $rm -f index.$dir

  # Build a new index, and tack it on.
  $echo Building index for $dir...
  if test -f $perl; then
    $cat *.$dir | tee ../$dir.txt | $perl ../index-files.pl $index_args > index.$dir
    $cat index.$dir >> ../$dir.txt
  else
    $cat *.$dir > ../$dir.txt
  fi
 
  cd ..

$echo Done.
$echo Remember to use @readcache if the mush is currently running.
!NO!SUBS!
chmod 755 compose.sh
$eunicefix compose.sh
