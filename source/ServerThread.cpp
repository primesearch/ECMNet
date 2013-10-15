/* ServerThread Class for ECMNet.

  Copyright 2011 Mark Rodenkirch

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the
  Free Software Foundation; either version 2 of the License, or (at your
  option) any later version.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU General Public License along
  with this program; see the file COPYING.  If not, write to the Free
  Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
  02111-1307, USA.
*/

#include "ServerThread.h"
#include "ServerSocket.h"
#include "HelperThread.h"
#include "HelperSocket.h"

#ifdef WIN32
   static DWORD WINAPI ServerThreadEntryPoint(LPVOID threadInfo);
#else
   static void  *ServerThreadEntryPoint(void *threadInfo);
#endif

void  ServerThread::StartThread(globals_t *globals)
{
   globals->p_ServerSocket = new ServerSocket(globals->p_Log, globals->i_PortID);

#ifdef WIN32
   // Ignore the thread handle return since the parent process won't suspend
   // or terminate the thread.
   CreateThread(0,                        // security attributes (0=use parents)
                0,                        // stack size (0=use parents)
                ServerThreadEntryPoint,
                globals,
                0,                        // execute immediately
                0);                       // don't care about the thread ID
#else
   pthread_create(&ih_thread, NULL, &ServerThreadEntryPoint, globals);
   pthread_detach(ih_thread);
#endif
}

#ifdef WIN32
DWORD WINAPI ServerThreadEntryPoint(LPVOID threadInfo)
#else
static void  *ServerThreadEntryPoint(void *threadInfo)
#endif
{
   globals_t    *globals;
   ServerSocket *serverSocket;
   HelperSocket *helperSocket;
   int32_t       clientSocketID, quitting, clientsConnected;
   char          clientAddress[200], socketDescription[50];
   HelperThread *helperThread;

   globals = (globals_t *) threadInfo;
   serverSocket = globals->p_ServerSocket;

   serverSocket->Open();

   while (true)
   {
      if (!serverSocket->AcceptMessage(clientSocketID, clientAddress))
         continue;

      quitting = (int32_t) globals->p_Quitting->GetValueNoLock();
      clientsConnected = (int32_t) globals->p_ClientCounter->GetValueNoLock();

      // If quitting, then break out of the loop
      if (quitting > 0)
         break;

      // If quitting and no clients are connected, then break out of the loop
      if (quitting && !clientsConnected)
         break;

      // If this is a client and we aren't quitting, generate a thread to handle it
      if (clientSocketID && !quitting)
      {
         sprintf(socketDescription, "%d", clientSocketID);
         helperSocket = new HelperSocket(globals->p_Log, clientSocketID, clientAddress, socketDescription);

         if (clientsConnected >= globals->i_MaxClients)
         {
            helperSocket->Send("ERROR: Server cannot handle more connections");
            globals->p_Log->LogMessage("Server has reached max connections of %d.  Connection from %s rejected",
                                       globals->i_MaxClients, clientAddress);
            delete helperSocket;
         }
         else
         {
            helperThread = new HelperThread();
            helperThread->StartThread(globals, helperSocket);
         }
      }
      else
         Sleep(500);
   }

#ifdef WIN32
   return 0;
#else
   pthread_exit(0);
#endif
}
