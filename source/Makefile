# specify the compiler
CPP=g++

CPPFLAGS=-c -g -Wall -O

# prpclient/prpadmin link flags
LFLAGS_CLIENT=-o
LFLAGS_CLIENT2=-lpthread
# use this on most Unix systems for prpserver
LFLAGS_SERVER=-o
LFLAGS_SERVER2=-lodbc -lgmp -lpthread 

SOCKETOBJECTS=Socket.o ServerSocket.o SlaveSocket.o MailSocket.o HelperSocket.o
WORKOBJECTS=WorkSender.o WorkReceiver.o Composite.o MasterComposite.o UserStats.o CompositeFactorEvent.o UpgradeFromV2.o  
THREADOBJECTS=ServerThread.o HelperThread.o KeepAliveThread.o
SERVEROBJECTS=IAmAMaster.o IAmASlave.o DBInterface.o SQLStatement.o SharedMemoryItem.o HTMLGenerator.o eval.o getprime.o 

all:  ecmclient ecmserver

ecmadmin: ecmadmin.o Log.o Socket.o ClientSocket.o SharedMemoryItem.o
	$(CPP) $(LFLAGS_CLIENT) ecmadmin ecmadmin.o Log.o Socket.o ClientSocket.o SharedMemoryItem.o $(LFLAGS_CLIENT2)

ecmclient: ecmclient.o Log.o Socket.o Work.o ClientSocket.o WorkUnit.o SharedMemoryItem.o
	$(CPP) $(LFLAGS_CLIENT) ecmclient ecmclient.o Log.o Socket.o Work.o ClientSocket.o WorkUnit.o SharedMemoryItem.o $(LFLAGS_CLIENT2)

ecmserver: ecmserver.o Log.o Mail.o server.h $(SERVEROBJECTS) $(SOCKETOBJECTS) $(WORKOBJECTS) $(THREADOBJECTS)
	$(CPP) $(LFLAGS_SERVER) ecmserver ecmserver.o Log.o Mail.o $(SERVEROBJECTS) $(SOCKETOBJECTS) $(WORKOBJECTS) $(THREADOBJECTS) $(LFLAGS_SERVER2)

ecmadmin.o: ecmadmin.cpp defs.h Log.h Socket.h
	$(CPP) $(CPPFLAGS) ecmadmin.cpp

ecmclient.o: ecmclient.cpp defs.h Log.h Socket.h Work.h
	$(CPP) $(CPPFLAGS) ecmclient.cpp

ecmserver.o: ecmserver.cpp defs.h Log.h Socket.h Mail.h
	$(CPP) $(CPPFLAGS) ecmserver.cpp

%.o: %.cpp %.h Log.h Socket.h
	$(CPP) $(CPPFLAGS) $<

clean:
	rm *.o ecmserver ecmclient
