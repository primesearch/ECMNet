These are the minimal steps to get the ECMNet client running.

1)  Build the ecmclient executable by using "make" or by using the ecmnet.sln
    project workspace for Visual Studio 2008.

2)  Modify the ecmclient.cfg and modify the following:
       email=       Your email address.  E-mail will be sent to this address
                    when a factor is found.
       server=      This points to an ECMNet server from which the client
                    will get work.  Some known ECMNet servers and their
                    descriptions can be found in ecmnet_servers.txt.
       gmpecmexe=   This is the GMP-ECM compatible executable that will be
                    used to factor.  This executable will only be used if
                    the server is an GMP-ECM server.

    Most of the remaining values in ecmclient.ini have defaults.  There is
    a description for each in that file that explains what they are used for.

