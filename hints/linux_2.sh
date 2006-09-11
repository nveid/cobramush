nm_opt='-B'

# sometimes mysql might be found in an actual mysql
# subdirectory in the library path.. We'll need to
# check for that

if [ -d /usr/lib/mysql ]; then
   ldflags='-L/usr/lib/mysql'
 elif [ -d /usr/local/lib/mysql ]; then
    ldflags='-L/usr/local/lib/mysql'
fi


echo "I suggest using sysmalloc when you edit options.h."
