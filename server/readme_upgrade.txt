      Upgrading ECMNet from 2.x to 3.x

You must follow these steps to get a clean upgrade from 2.x.

1)  Install MySQL.  It's free and it works marvelously.

2)  Rename ecmserver.ini to ecmserver.ini.old.  This is the file that contains
    composite numbers to be factored and the factoring work done on them.  If you
    lose this, then you are SOL.

3)  Modify the stock mysql.ini and configure as needed.

4)  Modify the stock ecmserver.ini and configure it as desired.  You should refer to
    the readme.txt for more information.  If you have a master/slave setup with
    servers, it is recommended to do one server at a time, starting with the slaves,
    but fudging the upstreamserver value in the ini file to prevent communication
    until both master and slave are running 3.x.  The master/slave protocol is
    not backward compatible with 2.x because only one side would be using database 
    transactions.

5)  Start the server with the -u option.  It will shutdown when done whether the
    upgrade is completely successful or not.

6)  Use the mysql command line tool (or a GUI one if you have one) to do spot 
    verification that the upgrade was successful.

7)  If not successful, that is most likely due to a code bug, so contact me at
    roguemgr@gmail.com if that happens.

8)  If successful, let 'er rip.

