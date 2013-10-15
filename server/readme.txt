      Main Features of ECMNet 3.0.2

ECMNet is a client/server application that is used to factor a list of composite
numbers in a distributed fashion.

Tim Charon wrote the original ECMNet.  Since he lost interest, I took over the 
development in 2005 and have added many new features.  To learn about the features
I have added, please read the history.txt file in the parent folder.

Many thanks to those who have tested and used ECMNet and to who have provided suggestions
or code that I have integrated into this software.

*************************** Running ECMNet **********************************************

These are the minimal steps to get ECMNet up and running.  More configuration options
can be found in other sections of this document.

1)  Build the ecmserver executable by using "make" or by using the ecmnet.sln
    project workspace for Visual Studio.  Note that if building on *nix, that
    changes to the makefile might be necessary.

2)  Modify the ecmserver.ini and modify the following:
       email=       Your email address.  E-mail will be sent to this address
                    when a factor is found.
       port=        The port that the server will listen to waiting for connections
                    from clients or slave servers.
       servertype=  The servertype is important for reasons stated below.

    Most of the remaining values in ecmserver.ini have defaults.  There is
    a description for each in that file that explains what they are used for.

3)  Create a MySQL/PostgreSQL database:

    a.  If necessary d/l MySQL from http://www.mysql.com or PostgreSQL from
        http://www.postgresql.org/.
    b.  Also d/l and install MySQL Connector/ODBC or psqlODBC for PostgreSQL.
    c.  On Linux, you might need to install iodbc and myodbc libraries.
    d.  On Linux, you might need to add an entry to the odbcinst.ini file
        for the MySQL/PostgreSQL drivers.

    d.1 Create a database user (suggested name "ecmnet") with minimal
        privilege and a known password.  For PostgreSQL these commands
        will do the job:

	createuser -S -D -R -l -P -e -U privuser ecmnet

	(The first four options are the default but are given here to
	emphasis the minimal privileges required.  The -l allows the
	ecmnet account to log in; the -P forces createuser to ask for
	a password for the new account and "-U privuser" is the
	privileged username which is to create the new account.)

<<< TODO:  MySQL commands >>>

    e.  For mysql create a new database using the mysqladmin tool.  For
        PostgreSQL use this command:

	createdb -e -O ecmnet -E UNICODE -U privuser ecmnet 

    f.  Connect to your new database.
    g.  Create the ECMNet tables using the create_tables_mysql.sql or
        create_tables_postgre.sql script
        found in the source directory.
    h.  Load the database via the ecmadmin tool.

4)  Modify the datbase.ini file to point to your database and to give
    the password you set when the ecmnet account was created..  Note
    the ECMNet can connect directly through the ODBC driver or a DSN.

5)  Modify the ecmserver.b1 file based upon Paul Zimmerman's website recommendations
    at http://www.loria.fr/~zimmerma/records/ecm/params.html.

6)  Modify the ecmserver.ini and modify the following:
       email=       Your email address.  E-mail will be sent to this address
                    when a factor is found.
       password=    A password is used to prevent rogue slave servers from
                    connecting to this server (in a master/slave configuration).
       port=        The port that the server will listen to waiting for connections
                    from clients or slave servers.

       Most of the remaining values in ecmclient.ini have defaults.  There is
       a description for each in that file that explains what they are used for.

7)  Use ecmadmin to populate the tables in the server.  If upgrading from ECMNet
    2.x, then rename ecmserver.ini as ecmserver.ini.old then start the server
    with the -u option.


*************************** Composite Selection *****************************************

Strategies are used to control how the server sends work out to composites.  They can be
set up to cause individual composites to get the most work or they can be set up to evenly
distribute how factoring work is sent to clients.

There are two areas of configuration that affect composite selection.  One is used to
determine which composite are sent (strategy) and the other is used to control how much
work is sent for each composite (maxwork).

Before going further, it is important to understand the concepts of "work" and "difficulty".
Work measures how much ECM factoring has been done for a number.  Work is computed as:
   sum of (b1 * curves done for B1) for each B1
Difficulty gives an indication of how hard a number is to factor.  It is computed as:
   work * length^2 (length is the decimal length of the number to factor)

First, we'll cover maxwork.  maxwork expects two values, delimited by a semi-colon.  The first
value is the maximum amount of work that the server can send for each number to be factored.
The second is the maximum amount of work that the server can send to a client.  Note that this
parameter only has meaning for 3.x clients.

Now we'll cover strategies.  One or more strategies must be configured in the ecmserver.ini
file.  The server supports up to ten strategies.  The strategies are configured as follows:

   strategy=a:b:c:d:e:f

The first parameter indicates the percentage of times that the server will choose that
strategy to send work to clients.

The second parameter provides sorting criteria for the server.  These criteria represent
a comma-delimited list of values that will tell the server how to order the composites for
the strategy.  All of these are ascending.  The supported values and their meanings are:
   a = Age        - order by last time composite had any factoring work
   d = difficulty - order by difficulty
   l = length     - order by decimal length
   w = work       - order by work

The third parameter is a base percentage that a composite for the strategy will be selected.

The last three parameters are used to increase or decrease the percentage that a composite
for the strategy will be selected based upon the last time it had work sent out or returned.
The fourth parameter is a threshold in hours, while the last two parameters are multipliers
for the increase and decrease of the base percentage.  Here is an example of how that works:

strategy=50:w,l:40:10:3:2

   This strategy will be used for 50% of the clients connecting to the server.  When used,
   the server will order composites by total work done and decimal length

   Given composite A, if that composite was last updated 6 hours ago, then the chance that
   the server will send composite A to a client is:
      40 + (6-10)*3 = 28 percent

   Given composite B, if that composite was last updated 28 hours ago, then the chance that
   the server will send composite B to a client is:
      40 + (28-10)*2 = 76 percent

Note for those who are upgrading from 2.x:
   strategy0 has no equivalent, but one can get close using low percentages for other parameters
   strategy1 has no direct equivalent, but "w,l" should produce results almost exactly the same
   strategy2 is equivalent to the sort parameter "w,l"
   strategy3 is equivalent to the sort parameter "d,l"
   strategy4 is equivalent to the sort parameter "l,w"

You should try to achieve a balance so that too many clients don't ECM the same composite
concurrently.  If a large number of clients are working on the same composite and one finds
a factor, then the other clients are wasting CPU on that composite.

Using strategy along with maxwork to help avoid doing too many curves for a given B1 on any
composite.  A case where maxwork can help is when the server sends the same composite and B1 to
two clients.  If each client is given 500 curves (because that B1 level needs 500 more
curves) and is allowed to complete them, then that composite will have 500 more curves for that
B1 than necessary.  Chances than an extra 500 curves will find a factor is extremely small.

How you set these values is up to you.  It will be dependent upon the frequency in which
clients connect to your server.  You will probably want to tweak them a few times to find
a good balance.

