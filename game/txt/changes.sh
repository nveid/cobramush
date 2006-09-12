#!/bin/sh

# First generate our changes file
ruby genchanges.rb

# Create Entries Index
cat changes.txt | perl index-files.pl > changes.idx

# Then combine the two
cat changes.idx >> changes.txt

# Then we clean up our mess
#mv changes.new changes.txt
rm changes.idx
