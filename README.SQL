
                            Use SQL with PennMUSH
                             Revised: 24 May 2004


As of version 1.7.7p32, PennMUSH includes functions and commands that
can access SQL databases. Currently, the following databases are 
supported:

  * MySQL

This document explains how to use (or avoid) SQL with PennMUSH, and covers
the following issues:

   I. Compiling with or without SQL
   II. Mush configuration overview
   III. SQL setup tips

I. Compiling with MySQL

  The Configure script distributed with PennMUSH automatically detects
  the MySQL client library (libmysqlclient) and attempts to link
  them into the executable, defining HAS_MYSQL in config.h

  If you want to avoid linking these libraries on systems where they
  are present, use the '-D no_mysql' switch to Configure.

  If you installed Mysql from a binary package (e.g. rpm or apt),
  you should be sure that your system also has the development
  package (usuallyl mysql-dev or mysql-devel).

  If you think you have mysql libraries and header files but Configure
  isn't finding them, they may be in an unusual location on your system.
  Creating symbolic links from the header directory to /usr/include/mysql
  and from the libraries in /usr/lib (or adding the mysql library directory
  to your /etc/ld.so.conf and running ldconfig) are the easiest ways
  to fix this.

II. Mush configuration overview

  mush.cnf includes these directives that configure the SQL support:

  sql_platform provides the name of the SQL database server software
  that will be used for connections. It current takes one of two
  values: "disabled" (no SQL) or "mysql". If not specified, it 
  defaults to disabled.

  sql_host gives the name of the host running the SQL server.
  It defaults to 127.0.0.1, which makes a TCP connection to the 
  local host. The keyword "localhost" instead makes a domain socket
  (Unix) or named pipe (Windows) connection.

  sql_database gives the name of the database that contains the 
  MUSH's tables. This must be specified and there is no default.
 
  sql_username provides a username to connect to the SQL server
  with. If no specified, a null username will be used, which many
  SQL servers treat as "the user running this (pennmush) process".

  sql_password provides the password for the user. It defaults to
  no password.
 
III. SQL setup tips

  You will have to set up the appropriate database on the SQL server,
  and username permitted to perform operations in that database, 
  and a password for that username. This is a platform-specific process.

  A. MySQL platform

    Easiest way is:

    % mysql_setpermission --user root 			REQUIRED
          --host <mysql host> --port <mysql port>	OPTIONAL, OR:
          --socket <unix domain socket>			OPTIONAL

    ######################################################################
    ## Welcome to the permission setter 1.2 for MySQL.
    ## made by Luuk de Boer
    ######################################################################
    What would you like to do:
      1. Set password for a user.
      2. Add a database + user privilege for that database.
         - user can do all except all admin functions
      3. Add user privilege for an existing database.
         - user can do all except all admin functions
      4. Add user privilege for an existing database.
         - user can do all except all admin functions + no create/drop
      5. Add user privilege for an existing database.
         - user can do only selects (no update/delete/insert etc.)
      0. exit this program
   
    Make your choice [1,2,3,4,5,0]: 2                    <==========

    Which database would you like to add: mush
    The new database mush will be created
    
    What username is to be created: mush                 <==========
    Username = mush
    Would you like to set a password for  [y/n]: y       <==========
    What password do you want to specify for :           <==========
    Type the password again:                             <==========
    We now need to know from what host(s) the user will connect.
    Keep in mind that % means 'from any host' ...
    The host please: localhost                           <==========
    Would you like to add another host [yes/no]: no      <==========
    Okay we keep it with this ...
    The following host(s) will be used: localhost.
    ######################################################################
    
    That was it ... here is an overview of what you gave to me:
    The database name       : mush
    The username            : mush
    The host(s)             : localhost
    ######################################################################
    
    Are you pretty sure you would like to implement this [yes/no]: yes

