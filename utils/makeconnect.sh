#!/bin/sh
cat > connect.txt <<EOSH
<This is where you announce that they've connected to your MUSH>
<It's a good idea to include the version/patchlevel of MUSH you're running>
<It's a good idea to include an email address for questions about the MUSH>
EOSH

version=`grep VERSION hdrs/version.h | sed 's/^.*"\(.*\)[p].*$/\1/'`
header_string="----------------------- CobraMUSH-v$version -------------------------------------"
echo $header_string >> connect.txt
cat >> connect.txt << EOSH
Use create <name> <password> to create a character.
Use connect <name> <password> to connect to your existing character.
Use 'ch <name> <pass>' to connect hidden, and cd to connect DARK (admin)
Use QUIT to logout.
Use the WHO command to find out who is online currently.
-----------------------------------------------------------------------------
Yell at your local god to personalize this file!

EOSH

