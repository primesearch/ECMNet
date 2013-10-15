Welcome to ECMNET Client/Server Version 3.0.0 (May 2011)

First of I want to thank Tim Charron for his work on the original ECMNET.
As he had not supported it for more than five years and has not shown
any interest in doing so, I have taken the mantle of supporting this
software and of adding new features to it.

What is ECMNET Client?
    ECMNET Client is a program that uses GMP-ECM to factor numbers
    using the ECM, P-1, and P+1 factoring methods.  As a client, this program 
    communicates with a centralized server (see "What is ECMNET Server?" below) 
    which doles out work to this client.  This client will run one or more curves 
    and then report back to the server the results of those indicating how many 
    curves it ran and factor information if a factor was one.

What do I need to do to get the client/server running?
    You can build the client in the source directory using make or by using the
    Visual C++ project workspace provided.  Specific instructions for running 
    the client or server can be found in those directories.

What is ECMNET Server?
    ECMNET Server is a server program that manages a list of composites numbers.
    When a client (see "What is ECMNET Client?" above) requests work, the server
    will use a set of predifined rules to determine which composite needs to have
    factoring work done.  It will then respond to that client with a candidate
    and a factoring method.  When a factor is found, it will log the factor and
    cofactor and update the list of composites. 

What is ECMNET Admin?
    ECMNET Admin is a program with which can perform various administrative
    functions with the server, such as loading composites into the server or
    making specific composites active or inactive.  Most ECMNET Admin functions
    require a password to prevent unauthorized modifications by outside users.
    It is not recommended to use ECMNET Admin without the permission of the
    person which is managing the server.

Will this work with versions of ECMNet prior to 2.5?
    No.
    
Will this work with other versions of ECMNet prior to 3.0, such as 2.7.x?
    Yes and no.  The client and server are fully compatible forward and
    backward.  The ECMNet 3.x client cannot read the ECMNet 2.x save files,
    so you should ensure that you have no completed curves that have not
    been reported to the server before converting.

Will 2.x and 3.x servers work together in a master/slave configuration?
    No.  I would prefer to ensure that 3.x is stable than write the enormous
    amount of code needed to support that feature.

Are there any concerns with migrating the server from 2.x to 3.x?
    None that I know of.

Can 2.x clients work with 3.x servers?
    Yes.

Can 3.x clients work with 2.x servers?
    No and I'm not intending to fix it because it would require people running 2.x
    servers to upgrade in order to allow it.

How do I compile/run these programs?
    If you are running Windows, then you can use the provided executables.  They
    were compiled with Visual C++.  For other environments, these programs can
    be easily compiled with the supplied Makefiles.  The default makefile can be 
    used with Cygwin under Windows (with the MinGW g++ compiler) and with Mac OS X.
    There is also a project workspace (ecmnet.sln) for use with Visual Studio 2008.
    Once the executable you need is built, you will need to modify the ini file in 
    the appropriate directory.  Please refer to that ini file to understand how to 
    set the various configuration parameters.

Where can I get the current source for ECMNet?
    At http://home.roadrunner.com/mrodenkirch/ecmnet.html

Where can I get GMP-ECM?
    At http://www.loria.fr/~zimmerma/records/ecmnet.html

Who are you and how do I contact you if I find or bug or have any questions?
    My name is Mark Rodenkirch.  I can be contacted at rogue@wi.rr.com.

