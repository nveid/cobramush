#!/bin/sh

SELF=`which "$0"`
DIR=`dirname "$SELF"`
cd "$DIR"
cd ..

# Step 1: run metaconfig
echo "Running metaconfig to generate Configure and config_h.SH..."
metaconfig -M

# Step 2: generate changes.txt
echo "Generating changes.txt..."
( cd game/txt && ruby genchanges.rb )

# Step 3: index help files
echo "Indexing help, news, and events..."
cd game/txt
for i in hlp nws evt ; do
    cd $i
    rm -f index.$i
    cat *.$i | perl ../index-files.pl > index.$i
    cd ..
done

cd ../..

