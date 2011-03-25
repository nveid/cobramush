#!/bin/sh

# Bootstrap
chmod 755 "$0"

SELF=`which "$0"`
DIR=`dirname "$SELF"`
cd "$DIR"
cd ..

# Step 1: set appropriate scripts executable
echo "Setting scripts executable..."
chmod 755 utils/makedist.sh
chmod 755 utils/customize.pl
chmod 755 utils/make_access_cnf.sh
chmod 755 utils/mkvershlp.pl
chmod 755 utils/update-cnf.pl
chmod 755 utils/update.pl

# Step 2: create auto-generated source/header files
echo "Generating source and header files..."
perl utils/mkcmds.pl all
mv hdrs/cmds.h win32/
mv hdrs/switches.h win32/
mv hdrs/funs.h win32/

# Step 3: run metaconfig
echo "Running metaconfig to generate Configure and config_h.SH..."
metaconfig -M

# Step 4: generate changes.txt
echo "Generating changes.txt..."
( cd game/txt && ruby genchanges.rb )

# Step 5: index help files
echo "Indexing help, news, and events..."
cd game/txt
for i in hlp nws evt ; do
    cd $i
    rm -f index.$i
    cat *.$i | perl ../index-files.pl > index.$i
    cd ..
done

cd ../..

