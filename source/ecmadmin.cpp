/* ecmadmin main() for ECMNet.

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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "defs.h"

#ifdef WIN32
  #include <io.h>
  #include <direct.h>
#endif

#ifdef UNIX
   #include <signal.h>
#endif

#include "Log.h"
#include "ClientSocket.h"

// global variables
char       g_EmailID[200];
char       g_ServerName[200];
uint32_t   g_PortID;
uint32_t   g_Debug;
char       g_Password[100];
uint32_t   g_Quit;
Log       *g_Log;
ClientSocket *g_Socket;

// procedure declarations
void     AdminMenu(void);
bool     OpenAdminSocket(void);
void     CloseAdminSocket(void);
char    *PromptForValue(const char *prompt, uint32_t verifyFile);
bool     VerifyCommand(const char *theCommand);
void     SetCompositeStatus(bool active);
void     SendCompositeFile(const char *abcFileName);
void     SendCurvesFile(const char *curvesFileName);
void     SetQuitting(int a);
void     PipeInterrupt(int a);
void     ProcessCommandLine(int count, char *arg[]);
void     Usage(char *exeName);

void      AdminMenu(void)
{
   int   menuitem = -1;
   char  response[500];

   while (true)
   {
      printf("\nECMNet Admin menu\n");
      printf("==========\n");
      printf(" 1. Enter new server address (%s)\n", g_ServerName);
      printf(" 2. Enter new server port # (%u)\n", g_PortID);
      printf(" 3. Enter admin password\n");
      if (strlen(g_Password))
      {
         printf(" 4. Add composites to the server\n");
         printf(" 5. Make a composite active\n");
         printf(" 6. Make a composite inactive\n");
         printf(" 7. Add completed curves\n");
      }
      printf("99. Quit.\n");

      printf("Enter option from menu: ");
      if (fgets(response, sizeof(response)-1, stdin) == NULL) {
         printf("Could not read the response\n");
         continue;
      }
      menuitem = atol(response);

      if (!strlen(g_Password) && menuitem > 3 && menuitem != 99)
         menuitem = 999;

      switch (menuitem)
      {
         case 99:
            g_Log->LogMessage("Closing Admin socket\n");
            CloseAdminSocket();
            return;

         case 1:
            strcpy(g_ServerName, PromptForValue("Enter server address: ", false));
            g_Password[0] = 0;
            break;

         case 2:
            g_PortID = atol(PromptForValue("Enter port number: ", false));
            g_Password[0] = 0;
            break;

         case 3:
            strcpy(g_Password, PromptForValue("Enter administrator password: ", false));
            break;

         case 4:
            strcpy(response, PromptForValue("Enter file to be imported: ", true));

            if (strlen(response) && OpenAdminSocket())
            {
               SendCompositeFile(response);
               CloseAdminSocket();
            }
            break;

         case 5:
            SetCompositeStatus(true);
            break;

         case 6:
            SetCompositeStatus(false);
            break;

         case 7:
            strcpy(response, PromptForValue("Enter file of curves: ", true));

            if (strlen(response) && OpenAdminSocket())
            {
               SendCurvesFile(response);
               CloseAdminSocket();
            }
            break;

         default:
            printf("Invalid choice\n");
      }
   }

   CloseAdminSocket();
}

char   *PromptForValue(const char *prompt, uint32_t verifyFile)
{
   static char response[200];
   FILE *fPtr;

   printf("%s", prompt);

   if (fgets(response, sizeof(response)-1, stdin) == NULL) {
      printf("Did not get a response.\n");
      return "";
   }

   StripCRLF(response);

   if (!verifyFile)
      return response;

   // Do a lot of pre-validation so that the client doesn't send garbage data to the server
   // The server will re-validate, but this will help reduce wasted bandwidth in case the
   // file is unusable.
   if ((fPtr = fopen(response, "r")) == NULL)
   {
      printf("File [%s] could not be opened.\n", response);
      response[0] = 0;
      return response;
   }

   if (fgets(response, sizeof(response)-1, fPtr) == NULL)
   {
      printf("File [%s] is empty.\n", response);
      response[0] = 0;
   }

   fclose(fPtr);
   return response;
}

void     SetCompositeStatus(bool active)
{
   char  compositeName[NAME_LENGTH+1];
   char *theMessage;

   strcpy(compositeName, PromptForValue("Enter composite name: ", false));

   if (!strlen(compositeName)) return;

   if (!OpenAdminSocket()) return;

   if (active)
      g_Socket->Send("ADMIN_ACTIVE %s %s", g_Password, compositeName);
   else
      g_Socket->Send("ADMIN_INACTIVE %s %s", g_Password, compositeName);

   theMessage = g_Socket->Receive();
   if (theMessage)
      g_Log->LogMessage(theMessage);
   else
      g_Log->LogMessage("The sever did not respond to the command");

   CloseAdminSocket();
}

bool  OpenAdminSocket(void)
{
   char *userID = (char *) "admin_user";
   char *machineID = (char *) "admin_machine";
   char *instanceID = (char *) "admin_instance";

   g_Socket = new ClientSocket(g_Log, g_ServerName, g_PortID);

   if (g_Socket->Open(g_EmailID, userID, machineID, instanceID))
      return true;

   g_Log->LogMessage("Error opening socket");
   delete g_Socket;
   g_Socket = 0;

   return false;
}

void      CloseAdminSocket(void)
{
   if (!g_Socket)
      return;

   g_Socket->Close();

   delete g_Socket;
   g_Socket = 0;
}

bool     VerifyCommand(const char *theCommand)
{
   char    *theMessage;
   bool     verified;

   // Send the command along with the password
   g_Socket->Send("%s %s", theCommand, g_Password);

   // Don't go any further if the password is not accepted
   theMessage = g_Socket->Receive();

   if (!theMessage)
      return false;

   if (memcmp(theMessage, "OK:", 3))
   {
      printf("%s", theMessage);
      verified = false;
   }
   else
      verified = true;

   return verified;
}

void  SendCompositeFile(const char *compositeFileName)
{
   char        readBuf[BUFFER_SIZE], *theMessage;
   bool        finished;
   FILE       *fPtr;

   if (!VerifyCommand("ADMIN_COMPOSITE"))
      return;

   fPtr = fopen(compositeFileName, "r");

   // Send the entire file and have the server parse it
   while (fgets(readBuf, BUFFER_SIZE, fPtr) != NULL)
   {
      StripCRLF(readBuf);
      g_Socket->Send(readBuf);
   }

   fclose(fPtr);

   g_Socket->Send("No more composites");

   finished = false;
   theMessage = g_Socket->Receive();
   while (theMessage)
   {
      if (!strcmp(theMessage, "Done"))
      {
         finished = true;
         break;
      }
      g_Log->LogMessage(theMessage);
      theMessage = g_Socket->Receive();
   }

   if (!finished)
   {
      g_Log->LogMessage("All composites sent, but server has not verified");
      g_Log->LogMessage("Verify what the server has processed and send again only if necessary");
   }
}

void  SendCurvesFile(const char *curvesFileName)
{
   char        readBuf[BUFFER_SIZE], *theMessage;
   bool        finished;
   FILE       *fPtr;

   if (!VerifyCommand("ADMIN_CURVES"))
      return;

   fPtr = fopen(curvesFileName, "r");

   // Send the entire file and have the server parse it
   while (fgets(readBuf, BUFFER_SIZE, fPtr) != NULL)
   {
      StripCRLF(readBuf);
      g_Socket->Send(readBuf);
   }

   fclose(fPtr);

   g_Socket->Send("No more curves");

   finished = false;
   theMessage = g_Socket->Receive();
   while (theMessage)
   {
      if (!strcmp(theMessage, "Done"))
      {
         finished = true;
         break;
      }
      g_Log->LogMessage(theMessage);
      theMessage = g_Socket->Receive();
   }

   if (!finished)
   {
      g_Log->LogMessage("All curves sent, but server has not verified");
      g_Log->LogMessage("Verify what the server has processed and send again only if necessary");
   }
}

void PipeInterrupt(int a)
{
   g_Log->LogMessage("Pipe Interrupt with code %ld.  Processing will continue", a);
}

void SetQuitting(int a)
{
   g_Log->LogMessage("Accepted force quit.  Waiting to close sockets before exiting");
   g_Quit = a = 1;
}

int main(int argc, char *argv[])
{
   char     logFile[200];

#if defined (_MSC_VER) && defined (MEMLEAK)
   _CrtMemState mem_dbg1, mem_dbg2, mem_dbg3;
   HANDLE hLogFile;
   int    deleteIt;

   hLogFile = CreateFile("edmadmin_memleak.txt", GENERIC_WRITE,
      FILE_SHARE_WRITE, NULL, CREATE_ALWAYS,
      FILE_ATTRIBUTE_NORMAL, NULL);

   _CrtSetReportMode( _CRT_WARN, _CRTDBG_MODE_FILE);
   _CrtSetReportFile( _CRT_WARN, hLogFile);
   _CrtSetReportMode( _CRT_ERROR, _CRTDBG_MODE_FILE );
   _CrtSetReportFile( _CRT_ERROR, hLogFile );
   _CrtSetReportMode( _CRT_ASSERT, _CRTDBG_MODE_FILE );
   _CrtSetReportFile( _CRT_ASSERT, hLogFile );

   // Store a memory checkpoint in the s1 memory-state structure
   _CrtMemCheckpoint( &mem_dbg1 );

   // Get the current state of the flag
   int tmpFlag = _CrtSetDbgFlag( _CRTDBG_REPORT_FLAG );
   // Turn Off (AND) - DO NOT Enable debug heap allocations and use of memory block type identifiers such as _CLIENT_BLOCK
   //tmpFlag &= ~_CRTDBG_ALLOC_MEM_DF;
   tmpFlag |= _CRTDBG_ALLOC_MEM_DF;

   // Turn Off (AND) - prevent _CrtCheckMemory from being called at every allocation request.  _CurCheckMemory must be called explicitly
   tmpFlag &= ~_CRTDBG_CHECK_ALWAYS_DF;
   //tmpFlag |= _CRTDBG_CHECK_ALWAYS_DF;

   // Turn Off (AND) - Do NOT include __CRT_BLOCK types in leak detection and memory state differences
   tmpFlag &= ~_CRTDBG_CHECK_CRT_DF;
   //tmpFlag |= _CRTDBG_CHECK_CRT_DF;

   // Turn Off (AND) - DO NOT Keep freed memory blocks in the heap’s linked list and mark them as freed
   tmpFlag &= ~_CRTDBG_DELAY_FREE_MEM_DF;

   // Turn Off (AND) - Do NOT perform leak check at end of program run.
   //tmpFlag &= ~_CRTDBG_LEAK_CHECK_DF;
   tmpFlag |= _CRTDBG_LEAK_CHECK_DF;

   // Set the new state for the flag
   _CrtSetDbgFlag( tmpFlag );
#endif

   sprintf(logFile, "ecmadmin.log");

   // Set default values for the global variables
   strcpy(g_EmailID, "ECMNet_Admin_user@");
   strcpy(g_ServerName, "localhost");
   g_PortID = 7100;
   g_Debug = 0;
   g_Quit = 0;
   g_Password[0] = 0;

   // Get override values from the command line
   ProcessCommandLine(argc, argv);

   g_Log = new Log(10, logFile, g_Debug, true);

   if (strlen(g_EmailID) == 0)
   {
      g_Log->LogMessage("An email address must be configured with -e.");
      exit(-1);
   }

   g_Log->LogMessage("ECMNet Admin application v%s started.", ECMNET_VERSION);
   g_Log->LogMessage("Configured to connect to %s:%ld", g_ServerName, g_PortID);

#ifdef UNIX
   signal(SIGHUP, SIG_IGN);
   signal(SIGINT, SetQuitting);
   signal(SIGQUIT, SetQuitting);
   signal(SIGTERM, SetQuitting);
   signal(SIGPIPE, PipeInterrupt);
#else
   // This has to be done before anything is done with sockets
   WSADATA  wsaData;
   WSAStartup(MAKEWORD(1,1), &wsaData);
#endif

   AdminMenu();

   CloseAdminSocket();

   delete g_Log;

#if defined (_MSC_VER) && defined (MEMLEAK)
   _CrtMemCheckpoint( &mem_dbg2 );
   if ( _CrtMemDifference( &mem_dbg3, &mem_dbg1, &mem_dbg2 ) )
   {
      _RPT0(_CRT_WARN, "\nDump the changes that occurred between two memory checkpoints\n");
      _CrtMemDumpStatistics( &mem_dbg3 );
      _CrtDumpMemoryLeaks( );
      deleteIt = false;
   }
   else
      deleteIt = true;
   CloseHandle(hLogFile);

   if (deleteIt)
      unlink("ecmadmin_memleak.txt");
#endif

   return 0;
}

// Parse the command line to get any overrides of the default values
void  ProcessCommandLine(int count, char *arg[])
{
   int   i = 1;

   while (i < count)
   {
      if (!strcmp(arg[i],"-p"))
      {
         g_PortID = atol(arg[i+1]);
         i++;
      }
      else if (!strcmp(arg[i],"-s"))
      {
         strcpy(g_ServerName, arg[i+1]);
         i++;
      }
      else if (!strcmp(arg[i],"-a"))
      {
         strcpy(g_Password, arg[i+1]);
         i++;
      }
      else if (!strcmp(arg[i], "-debug"))
         g_Debug = 1;
      else
      {
         Usage(arg[0]);
         exit(0);
      }

      i++;
   }
}

void  Usage(char *exeName)
{
   printf("Usage: %s [-s addr] [-p port#] [-a pwd]\n", exeName);
   printf("    -s addr    Specify address of server to connect to.\n");
   printf("    -p port #  Specify port number to connect to.\n");
   printf("    -a pwd     Specify server admin password.\n");
}
