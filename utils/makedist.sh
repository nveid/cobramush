#!/bin/sh

SELF=`which "$0"`
DIR=`dirname "$SELF"`
cd "$DIR"
cd ..

# Step 1: make sure it is distribution-ready
echo "Ensure that you run preparedist.sh before makedist.sh to ensure"
echo "that all files are up-to-date"

# Step 2: determine version
echo "Determining version..."
VERSION=`grep VERSION hdrs/version.h | sed 's/^.*"\(.*\)".*$/\1/'`
BRANCH=`grep VBRANCH hdrs/version.h | sed 's/^.*"\(.*\)".*$/\1/'`
echo "Version is $VERSION, branch is $BRANCH"

# Step 3: copy everything into a temporary directory
WD=`pwd`
TMPDIR=`mktemp -d "cobramush.dist.XXXXXX"`
cd "$TMPDIR"
mkdir cobramush
split -l 50 "$WD/MANIFEST"
for i in x?? ; do
    for j in `cat $i` ; do
        mkdir -p cobramush/`dirname "$j"`
        cp "$WD/$j" cobramush/`dirname "$j"`
    done
done
rm x??

# Step 4: make tarball
echo "Making tarball..."
if [ "$BRANCH" = "release" ]; then
    BRANCHNAME=""
else
    BRANCHNAME="-$BRANCH"
fi
TARBALL="cobramush-$VERSION$BRANCHNAME.tar.gz"
tar -czf "$WD/$TARBALL" cobramush

# Step 5: clean up
echo "Cleaning up..."
cd "$WD"
rm -r "$TMPDIR"

echo "$TARBALL created"

