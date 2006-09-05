#!/bin/sh

# First get our entries line number and cut 'em up
lineat=`grep -n "& Entries" changes.txt | sed "s/\([0-9]\+\):& Entries/\1/"`
line=`expr $lineat - 1`

cat changes.txt | head -n $line > changes.new

# Next create entries file

cat changes.new | perl index-files.pl > changes.idx

# And combine the two
cat changes.idx >> changes.new

# Then we clean up our mess
mv changes.new changes.txt
rm changes.idx
