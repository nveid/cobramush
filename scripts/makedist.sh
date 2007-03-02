#!/bin/sh

SELF=`which "$0"`
DIR=`dirname "$SELF"`
cd "$DIR"
cd ..

# Step 1: make it distribution-ready
echo "Preparing distribution..."
. ./scripts/preparedist.sh

# Step 2: determine version
echo "Determining version..."
VERSION=`grep VERSION hdrs/version.h | sed 's/^.*"\(.*\)".*$/\1/'`
echo "Version is $VERSION"

# Step 3: make tarball
echo "Making tarball..."
TARBALL="cobramush-$VERSION.tar.gz"
tar -czf "$TARBALL" `cat MANIFEST`

echo "Tarball $TARBALL created"

