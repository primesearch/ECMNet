/* ecmserver main() for ECMNet.

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
#include <time.h>
#include <ctype.h>
#include "defs.h"

#ifdef WIN32
   #include <io.h>
   #include <direct.h>
#endif

#ifdef UNIX
   #include <signal.h>
#endif

#include "server.h"
#include "ServerThread.h"
#include "ServerSocket.h"
#include "ClientSocket.h"
#include "Mail.h"
#include "DBInterface.h"
#include "SQLStatement.h"
#include "IAmASlave.h"
#include "UpgradeFromV2.h"
#include "Composite.h"

globals_t  *gp_Globals;
Mail       *g_Mail;
IAmASlave  *g_IAmASlave;
char        g_EmailID[100];
char        g_EmailPassword[100];
char        g_SlavePassword[100];
int32_t     g_DestIDCount;
char       *g_DestID[10];
int32_t     g_UpstreamFrequency;

#ifdef WIN32
   BOOL WINAPI HandlerRoutine(DWORD /*dwCtrlType*/);
#endif

void     SetQuitting(int a);
void     ProcessINIFile(const char *iniFile, char *masterServer, char *smtpServer);
void     ReprocessINIFile(const char *iniFile);
void     ProcessB1File(const char *b1File);
void     ExtractStrategy(char *line);
void     ExtractB1(char *line);
bool     ValidateConfiguration(char *masterServer, char *smtpServer);
void     SendEmail(DBInterface *dbInterface);

void     ExpirePendingWork(DBInterface *dbInterface);
void     RecurseFactors(DBInterface *dbInterface);

int   main(int argc, char *argv[])
{
   char     masterServer[200], mailServer[200];
   int32_t  ii, quit, counter;
   bool     showUsage;
   bool     upgrading = false;
   time_t   nextSync;
   strategy_t   *nextStrategy;
   ServerThread *serverThread = 0;
   DBInterface  *dbInterface = 0, *timerDBInterface = 0;

 #if defined (_MSC_VER) && defined (MEMLEAK)
   _CrtMemState mem_dbg1, mem_dbg2, mem_dbg3;
   HANDLE hLogFile;
   int deleteIt;

   hLogFile = CreateFile("ecmserver_memleak.txt", GENERIC_WRITE,
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

   gp_Globals = new globals_t;
   gp_Globals->s_MasterPassword[0] = 0;
   gp_Globals->s_AdminPassword[0] = 0;
   gp_Globals->s_HTMLTitle[0] = 0;
   gp_Globals->s_ProjectName[0] = 0;
   gp_Globals->s_ServerID[0] = 0;
   gp_Globals->d_MinB1 = 0.;
   gp_Globals->d_MaxB1 = 999999999999.;
   gp_Globals->i_PM1DepthOverECM = 0;
   gp_Globals->i_PP1DepthOverECM = 0;
   gp_Globals->p_Strategy = 0;
   gp_Globals->i_MaxClients = 20;
   gp_Globals->b_IAmASlave = false;
   gp_Globals->b_Recurse = true;
   gp_Globals->i_B1Count = 0;
   gp_Globals->i_ExpireHours = 240;

   gp_Globals->p_Quitting = new SharedMemoryItem("quit");
   gp_Globals->p_ClientCounter = new SharedMemoryItem("client_counter");
   gp_Globals->p_Locker = new SharedMemoryItem("candidate_locker");
   gp_Globals->p_TotalWorkSent = new SharedMemoryItem("work_sent");

   masterServer[0] = 0;
   mailServer[0] = 0;
   g_EmailID[0] = 0;
   g_EmailPassword[0] = 0;
   g_DestIDCount = 0;
   g_Mail = 0;
   g_IAmASlave = 0;
   g_DestIDCount = 0;

   showUsage = false;

   gp_Globals->p_Log = new Log(0, "ecmserver.log", DEBUG_NONE, true);

   // Get default values from the configuration file
   ProcessINIFile("ecmserver.ini", masterServer, mailServer);

   ProcessB1File("ecmserver.b1");

   for (ii=1; ii<argc; ii++)
   {

      if (!memcmp(argv[ii], "-u", 2))
         upgrading = true;
      else if (!memcmp(argv[ii], "-d", 2))
         gp_Globals->p_Log->SetDebugLevel(DEBUG_ALL);
      else
         showUsage = true;
   }

   if (showUsage)
   {
      printf("Mark Rodenkirch's ECMNet Server, Version %s\n", ECMNET_VERSION);
      printf("   -d                 Turn on all debugging\n");
      printf("   -h                 Show this menu\n");
      printf("   -u                 Upgrade from 2.x to 3.x\n");
      exit(0);
   }

   dbInterface = new DBInterface(gp_Globals->p_Log, "database.ini");
   if (!dbInterface->Connect(2))
      exit(0);

   if (!ValidateConfiguration(masterServer, mailServer))
   {
      delete gp_Globals->p_Log;
      return -1;
   }

   if (upgrading)
   {
      UpgradeFromV2 *upgradeFromV2;
      upgradeFromV2 = new UpgradeFromV2(gp_Globals->p_Log, gp_Globals->s_ServerID);
      upgradeFromV2->Start("ecmserver.ini.old");
      exit(0);
   }

#ifdef WIN32
   SetPriorityClass(GetCurrentProcess(), IDLE_PRIORITY_CLASS);
   SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_IDLE);

   SetConsoleCtrlHandler(HandlerRoutine, TRUE);
#endif

#ifdef UNIX
   nice(20);

   // Ignore SIGHUP, as to not die on logout.
   // We log to file in most cases anyway.
   signal(SIGHUP, SIG_IGN);
   signal(SIGTERM, SetQuitting);
   signal(SIGINT, SetQuitting);
   signal(SIGPIPE, SIG_IGN);
#endif

   serverThread = new ServerThread();
   serverThread->StartThread(gp_Globals);

   gp_Globals->p_Log->LogMessage("ECMnet Server application v%s started.", ECMNET_VERSION);
   gp_Globals->p_Log->LogMessage("Please contact Mark Rodenkirch at rogumgr@gmail.com for support");
   gp_Globals->p_Log->LogMessage("Waiting for connections on port %ld", gp_Globals->i_PortID);

   timerDBInterface = new DBInterface(gp_Globals->p_Log, "database.ini");

   counter = 0;
   nextSync = 0;
   quit = false;
   while (!quit)
   {
      counter++;
      Sleep(1000);

      // Read ini file every 256 seconds to reset the debug level
      if (!(counter & 0xff))
      {
         ReprocessINIFile("ecmserver.ini");

         if (gp_Globals->b_Recurse && !gp_Globals->b_IAmASlave)
         {
            timerDBInterface->Connect(3);
            RecurseFactors(timerDBInterface);
            timerDBInterface->Disconnect();
         }
      }

      // Send e-mail every 1024 seconds (if new factors have been reported)
      if (!(counter & 0x3ff))
      {
         timerDBInterface->Connect(3);

         if (g_Mail && !gp_Globals->b_IAmASlave)
            SendEmail(timerDBInterface);

         ExpirePendingWork(timerDBInterface);

         timerDBInterface->Disconnect();
      }

      if (g_IAmASlave && nextSync + (g_UpstreamFrequency * 3600) < time(NULL))
      {
         timerDBInterface->Connect(3);

         g_IAmASlave->SyncWithMaster(timerDBInterface);

         timerDBInterface->Disconnect();
      }

      // If the user is trying to terminate the process, then
      // wait until all clients have completed what they are
      // doing before shutting down the server.
      if (gp_Globals->p_Quitting->GetValueNoLock() != 0)
      {
         int32_t threadsLeft = (int32_t) gp_Globals->p_ClientCounter->GetValueNoLock();

         if (threadsLeft == 0)
            quit = true;
         else
         {
            if (!(counter & 0x0f))
               gp_Globals->p_Log->LogMessage("%d threads still executing", threadsLeft);
         }
      }
   }

   delete timerDBInterface;
   delete dbInterface;

   gp_Globals->p_Log->LogMessage("Server is shutting down");

   for (ii=0; ii<g_DestIDCount; ii++)
      delete [] g_DestID[ii];

   while (gp_Globals->p_Strategy)
   {
      nextStrategy = (strategy_t *) gp_Globals->p_Strategy->next;
      delete gp_Globals->p_Strategy;
      gp_Globals->p_Strategy = nextStrategy;
   }

   if (g_IAmASlave) delete g_IAmASlave;
   if (g_Mail) delete g_Mail;
   if (serverThread) delete serverThread;
   if (gp_Globals->p_ServerSocket) delete gp_Globals->p_ServerSocket;
   delete gp_Globals->p_Log;
   delete gp_Globals->p_Quitting;
   delete gp_Globals->p_ClientCounter;
   delete gp_Globals->p_Locker;
   delete gp_Globals->p_TotalWorkSent;
   delete gp_Globals;

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
      unlink("ecmserver_memleak.txt");
#endif

   return 0;
}

#ifdef WIN32
BOOL WINAPI HandlerRoutine(DWORD /*dwCtrlType*/)
{
   gp_Globals->p_Log->LogMessage("Windows control handler called.  Exiting");

   gp_Globals->p_Quitting->SetValueNoLock(1);

   return true;
}
#endif

void PipeInterrupt(int a)
{
   gp_Globals->p_Log->LogMessage("Pipe Interrupt with code %d.  Processing will continue", a);
}

void SetQuitting(int sig)
{
   const char *sigText = "";

   if (gp_Globals->p_Quitting->GetValueNoLock() == 5)
   {
      printf("Couldn't lock memory.  Cannot quit.\n");
      return;
   }

#ifndef WIN32
   if (sig == SIGINT)
      sigText = "SIGINT - Interrupt (Ctrl-C)";
   if (sig == SIGQUIT)
      sigText = "SIGQUIT";
   if (sig == SIGHUP)
      sigText = "SIGHUP - Timeout or Disconnect";
   if (sig == SIGTERM)
      sigText = "SIGTERM - Software termination signal from kill";

   gp_Globals->p_Log->LogMessage("Signal recieved: %s", sigText);
#endif

   gp_Globals->p_Quitting->IncrementValue();

#ifndef WIN32
   signal(sig, SIG_DFL);
   raise(sig);
#endif
}

// Parse the configuration file to get the default values
void  ProcessINIFile(const char *iniFile, char *masterServer, char *mailServer)
{
   FILE *fp;
   char  line[241];

   if ((fp = fopen(iniFile, "r")) == NULL)
   {
      printf("ini file '%s' was not found\n", iniFile);
      return;
   }

   while (fgets(line, 240, fp) != NULL)
   {
      StripCRLF(line);

      if (!memcmp(line, "email=", 6))
         strcpy(g_EmailID, line+6);
      else if (!memcmp(line, "htmltitle=", 10))
         strcpy(gp_Globals->s_HTMLTitle, line+10);
      else if (!memcmp(line, "serverid=", 9))
         strcpy(gp_Globals->s_ServerID, line+9);
      else if (!memcmp(line, "emailpassword=", 14))
         strcpy(g_EmailPassword, line+14);
      else if (!memcmp(line, "port=", 5))
         gp_Globals->i_PortID = atol(line+5);
      else if (!memcmp(line, "debuglevel=", 11))
         gp_Globals->p_Log->SetDebugLevel(atol(line+11));
      else if (!memcmp(line, "upstreamfrequency=", 18))
         g_UpstreamFrequency = atol(line+18);
      else if (!memcmp(line, "expirehours=", 12))
         gp_Globals->i_ExpireHours = atol(line+12);
      else if (!memcmp(line, "loglimit=", 9))
         gp_Globals->p_Log->SetLogLimit(atol(line+9) * 1024 * 1024);
      else if (!memcmp(line, "localtimelog=", 13))
         gp_Globals->p_Log->SetUseLocalTime((atol(line+13) == 0 ? false : true));
      else if (!memcmp(line, "localtimelog=2", 14))
         gp_Globals->p_Log->AppendTimeZone(false);
      else if (!memcmp(line, "minb1=", 6))
         gp_Globals->d_MinB1 = atof(line+6);
      else if (!memcmp(line, "maxb1=", 6))
         gp_Globals->d_MaxB1 = atof(line+6);
      else if (!memcmp(line, "masterpassword=", 15))
         strcpy(gp_Globals->s_MasterPassword, line+15);
      else if (!memcmp(line, "adminpassword=", 14))
         strcpy(gp_Globals->s_AdminPassword, line+14);
      else if (!memcmp(line, "slavepassword=", 14))
         strcpy(g_SlavePassword, line+14);
      else if (!memcmp(line, "smtpserver=", 11))
         strcpy(mailServer, line+11);
      else if (!memcmp(line, "destid=", 7))
      {
         if (strlen(&line[7]) > 0  && g_DestIDCount < 10)
         {
            g_DestID[g_DestIDCount] = new char [strlen(&line[7]) + 2];
            strcpy(g_DestID[g_DestIDCount], &line[7]);
            g_DestIDCount++;
         }
      }
      else if (!memcmp(line, "upstreamserver=", 15))
         strcpy(masterServer, line+15);
      else if (!memcmp(line, "recurse=", 8))
         gp_Globals->b_Recurse = (atol(line+8) == 1 ? true : false);
      else if (!memcmp(line, "strategy=", 9))
         ExtractStrategy(line+9);
      else if (!memcmp(line, "b1=", 3))
         ExtractB1(line+3);
   }

   fclose(fp);
}

// Parse the b1 file to get the default values
void  ProcessB1File(const char *b1File)
{
   FILE *fp;
   char  line[241];

   if ((fp = fopen(b1File, "r")) == NULL)
   {
      printf("b1 file '%s' was not found\n", b1File);
      return;
   }

   while (fgets(line, 240, fp) != NULL)
   {
      StripCRLF(line);

      if (!memcmp(line, "minb1=", 6))
         gp_Globals->d_MinB1 = atof(line+6);
      else if (!memcmp(line, "maxb1=", 6))
         gp_Globals->d_MaxB1 = atof(line+6);
      else if (!memcmp(line, "p-1depthoverecm=", 16))
         gp_Globals->i_PM1DepthOverECM = atol(line+16);
      else if (!memcmp(line, "p+1depthoverecm=", 16))
         gp_Globals->i_PP1DepthOverECM = atol(line+16);
      else if (!memcmp(line, "b1=", 3))
         ExtractB1(line+3);
   }

   fclose(fp);
}

void     ReprocessINIFile(const char *iniFile)
{
   FILE    *fp;
   char     line[2001];

   if ((fp = fopen(iniFile, "r")) == NULL) return;

   // Extract the email ID and count the number of servers on this pass
   while (fgets(line, 2000, fp) != NULL)
   {
      StripCRLF(line);

      if (!memcmp(line, "debuglevel=", 11))
         gp_Globals->p_Log->SetDebugLevel(atol(line+11));
      else if (!memcmp(line, "expirehours=", 12))
         gp_Globals->i_ExpireHours = atol(line+12);
      else if (!memcmp(line, "recurse=", 8))
         gp_Globals->b_Recurse = (atol(line+8) == 1 ? true : false);
   }

   fclose(fp);
}

bool  ValidateConfiguration(char *masterServer, char *mailServer)
{
   char   *ptr;
   double  total = 0., ratio;
   strategy_t *pStrategy;

#ifdef WIN32
   // This has to be done before anything is done with sockets
   WSADATA  wsaData;
   char     processName[200];
   WSAStartup(MAKEWORD(1,1), &wsaData);

   // prevent multiple copies from running
   sprintf(processName, "EcmServer_Port%ld", gp_Globals->i_PortID);
   void *Sem = CreateMutex(NULL, FALSE, processName);
   if (!Sem || ERROR_ALREADY_EXISTS == GetLastError())
   {
      printf("Unable to run multiple copies of this application on port %ld.\n", gp_Globals->i_PortID);
      exit(-1);
   }
#endif

   if (!strlen(gp_Globals->s_HTMLTitle))
   {
      printf("The HTML title must be configured.\n");
      return false;
   }

   if (strlen(g_EmailID) == 0)
   {
      printf("An email address must be configured.\n");
      return false;
   }

   if (!strchr(g_EmailID,'@') || strchr(g_EmailID,':'))
   {
      printf("Email address supplied (%s) is not valid.\n", g_EmailID);
      return false;
   }

   if (strlen(gp_Globals->s_ServerID) == 0)
   {
      printf("A server ID must be configured.\n");
      return false;
   }

   if (gp_Globals->i_B1Count == 0)
   {
      printf("The server is unable to start because no b1 entries were configured.\n");
      return false;
   }

   total = 0.0;
   pStrategy = gp_Globals->p_Strategy;
   while (pStrategy)
   {
      total += pStrategy->selectStrategyPercent;
      pStrategy = (strategy_t *) pStrategy->next;
   }

   if (total != 100.)
   {
      ratio = 100./total;
      pStrategy = gp_Globals->p_Strategy;
      while (pStrategy)
      {
         pStrategy->selectStrategyPercent *= ratio;
         pStrategy = (strategy_t *) pStrategy->next;
      }

      printf("The strategy percents did not add up to exactly 100. Scaling from %f\n", total);
   }

   if (strlen(masterServer) > 0)
   {
      gp_Globals->b_IAmASlave = true;

      ptr = strchr(masterServer, ':');
      if (!ptr && masterServer[0] != ':')
         printf("The Upstream Server %s is not configured correctly.  Results will not be sent upstream.\n", masterServer);
      else if (gp_Globals->s_ServerID[0] == 0)
         printf("A server ID is not specified.  Results will not be sent upstream.\n");
      else if (g_SlavePassword[0] == 0)
         printf("A slave password is not specified.  Results will not be sent upstream.\n");
      else
      {
         *ptr = 0x00;
         ptr++;
         g_IAmASlave = new IAmASlave(gp_Globals->p_Log, masterServer, atol(ptr), gp_Globals->s_ServerID, g_SlavePassword);
      }
   }

   if (strlen(mailServer) > 0)
   {
      ptr = strchr(mailServer, ':');
      if (!ptr && mailServer[0] != ':')
         printf("The SMTP Server %s is not configured correctly.  Found factors will not be emailed to %s.\n", mailServer, g_EmailID);
      else
      {
         *ptr = 0x00;
         ptr++;
         g_Mail = new Mail(gp_Globals->p_Log, mailServer, atol(ptr), g_EmailID, g_EmailPassword, g_DestIDCount, g_DestID);
      }
   }

   return true;
}

void  ExtractStrategy(char *line)
{
   char    *ptr, tempLine[100], sort[50];
   int32_t  fields, length;
   strategy_t *pStrategy, *tempStrategy;

   strcpy(tempLine, line);
   fields = 0;
   ptr = tempLine;
   while (*ptr)
   {
      if (*ptr == ':')
      {
         fields++;
         *ptr = ' ';
      }

      ptr++;
   }

   if (fields != 5)
   {
      printf("Strategy <%s> is configured incorrectly.\n", line);
      exit(-1);
   }

   pStrategy = new strategy_t;
   pStrategy->next = 0;
   pStrategy->workSent = 0;
   pStrategy->sortSequence[0] = 0;

   fields = sscanf(tempLine, "%lf %s %lf %lf %lf %lf", &pStrategy->selectStrategyPercent, sort,
                   &pStrategy->selectCandidatePercent,
                   &pStrategy->hourBreak,
                   &pStrategy->percentAddIfOverHours,
                   &pStrategy->percentSubIfUnderHours);

   if (fields != 6)
   {
      printf("strategy <%s> could not be parsed.\n", line);
      exit(0);
   }

   length = strlen(sort);
   if (!length || length > 15 || !(length & 0x01))
   {
      printf("sorting of strategy <%s> is invalid.  Will set it to l,a (by length then age).\n", sort);
      strcpy(pStrategy->sortSequence, "DecimalLength,LastUpdateTime");
   }
   else
   {
      ptr = sort;
      while (*ptr)
      {
         switch (toupper(*ptr))
         {
            case 'L':
               if (strlen(pStrategy->sortSequence))
                  strcat(pStrategy->sortSequence, ",");
               strcat(pStrategy->sortSequence, "Composite.DecimalLength");
               break;
            case 'A':
               if (strlen(pStrategy->sortSequence))
                  strcat(pStrategy->sortSequence, ",");
               strcat(pStrategy->sortSequence, "MasterComposite.LastUpdateTime");
               break;
            case 'D':
               if (strlen(pStrategy->sortSequence))
                  strcat(pStrategy->sortSequence, ",");
               strcat(pStrategy->sortSequence, "Composite.Difficulty");
               break;
            case 'W':
               if (strlen(pStrategy->sortSequence))
                  strcat(pStrategy->sortSequence, ",");
               strcat(pStrategy->sortSequence, "MasterComposite.TotalWorkDone");
               break;
            default:
               printf("sort option %c is invalid. It will be ignored.\n", *ptr);
         }
         ptr++;
         if (!*ptr)
            break;

         if (*ptr != ',')
            printf("sort option delimiter %c is invalid. It will be ignored.\n", *ptr);
         ptr++;
      }
   }

   // Multiply by the number of seconds per hour
   pStrategy->hourBreak *= 3600;

   if (!gp_Globals->p_Strategy)
      gp_Globals->p_Strategy = pStrategy;
   else
   {
      tempStrategy = gp_Globals->p_Strategy;
      while (tempStrategy->next)
         tempStrategy = (strategy_t *) tempStrategy->next;

      tempStrategy->next = pStrategy;
   }
}

void     ExtractB1(char *line)
{
   int32_t  digits, ecmCurves, pp1, pm1;
   double   b1, prevSumWork, thisWork;

   if (sscanf(line, "%d %lf %d %d %d", &digits, &b1, &ecmCurves, &pp1, &pm1) != 5)
   {
      printf("Line <%s> not formatted correctly.  Exiting\n", line);
      exit(0);
   }

   if (gp_Globals->i_B1Count > 0 && b1 < gp_Globals->p_B1[gp_Globals->i_B1Count-1].b1)
   {
      printf("b1 values should be in ascending sequence.  Exiting\n");
      exit(0);
   }

   thisWork = b1 * (double) ecmCurves;

   gp_Globals->p_B1[gp_Globals->i_B1Count].b1 = b1;
   gp_Globals->p_B1[gp_Globals->i_B1Count].expectedWork = thisWork;
   if (gp_Globals->i_B1Count == 0)
      gp_Globals->p_B1[gp_Globals->i_B1Count].sumWork = thisWork;
   else
   {
      prevSumWork = gp_Globals->p_B1[gp_Globals->i_B1Count-1].sumWork;
      gp_Globals->p_B1[gp_Globals->i_B1Count].sumWork = prevSumWork + thisWork;
   }
   gp_Globals->p_B1[gp_Globals->i_B1Count].curvesECM = ecmCurves;
   gp_Globals->p_B1[gp_Globals->i_B1Count].curvesPP1 = pp1;
   gp_Globals->p_B1[gp_Globals->i_B1Count].curvesPM1 = pm1;

   gp_Globals->i_B1Count++;

   if (gp_Globals->i_B1Count == MAX_B1)
   {
      printf("The server only allows for %d b1 entries.\n", MAX_B1);
      exit(0);
   }
}

void     SendEmail(DBInterface *dbInterface)
{
   SQLStatement  *selectStatement;
   SQLStatement  *deleteStatement;
   const char    *selectSQL = "select cfe.CompositeName, CompositeValue, " \
                              "       FactorEvent, EmailID, Duplicate " \
                              "  from CompositeFactorEvent cfe, Composite " \
                              " where SentEmail = 0 " \
                              "   and Composite.CompositeName = cfe.CompositeName " \
                              " order by FactorEvent ";
   const char    *deleteSQL = "update CompositeFactorEvent " \
                              "   set SentEmail = 1  " \
                              " where CompositeName = ?  " \
                              "   and FactorEvent = ? ";
   char           compositeName[NAME_LENGTH+1];
   char           compositeValue[VALUE_LENGTH+1];
   char           emailID[200];
   bool           success;
   int32_t        duplicate, factorEvent;

   selectStatement = new SQLStatement(gp_Globals->p_Log, dbInterface, selectSQL);
   selectStatement->BindSelectedColumn(compositeName, NAME_LENGTH);
   selectStatement->BindSelectedColumn(compositeValue, VALUE_LENGTH);
   selectStatement->BindSelectedColumn(&factorEvent);
   selectStatement->BindSelectedColumn(emailID, ID_LENGTH);
   selectStatement->BindSelectedColumn(&duplicate);

   deleteStatement = new SQLStatement(gp_Globals->p_Log, dbInterface, deleteSQL);
   deleteStatement->BindInputParameter(compositeName, NAME_LENGTH, false);
   deleteStatement->BindInputParameter(factorEvent);

   while (selectStatement->FetchRow(false))
   {
      success = g_Mail->NotifyUserAboutFactor(dbInterface, compositeName, compositeValue,
                                              factorEvent, emailID, (duplicate ? true : false));

      if (success)
      {
         deleteStatement->SetInputParameterValue(compositeName, true);
         deleteStatement->SetInputParameterValue(factorEvent);
         if (deleteStatement->Execute())
         {
            dbInterface->Commit();
            gp_Globals->p_Log->LogMessage("Sent email about factors for composite %s", compositeName);
         }
         else
            dbInterface->Rollback();
      }
   }

   selectStatement->CloseCursor();
   delete deleteStatement;
   delete selectStatement;

   return;
}

void     ExpirePendingWork(DBInterface *dbInterface)
{
   SQLStatement  *deleteStatement;
   const char    *deleteSQL = "delete from PendingWork " \
                              " where DateAssigned < ? ";
   int64_t        dateAssigned;

   dateAssigned = time(NULL);
   dateAssigned -= (60 * 60 * gp_Globals->i_ExpireHours);

   deleteStatement = new SQLStatement(gp_Globals->p_Log, dbInterface, deleteSQL);
   deleteStatement->BindInputParameter(dateAssigned);

   if (deleteStatement->Execute())
      dbInterface->Commit();
   else
      dbInterface->Rollback();

   delete deleteStatement;

   return;
}

void     RecurseFactors(DBInterface *dbInterface)
{
   SQLStatement *selectStatement;
   SQLStatement *updateStatement1;
   SQLStatement *updateStatement2;
   const char   *selectSQL = "select mc.MasterCompositeName, cfed.CompositeName, " \
                             "       cfed.FactorEvent, cfed.FactorEventSequence, " \
                             "       mc.RecurseIndex, " \
                             "       cfed.FactorValue, cfed.DecimalLength " \
                             "  from MasterComposite mc, Composite, CompositeFactorEvent cfe, " \
                             "       CompositeFactorEventDetail cfed " \
                             " where cfed.PRPTestResult = ? " \
                             "   and cfed.IsRecursed = 0 " \
                             "   and mc.MasterCompositeName = Composite.MasterCompositeName " \
                             "   and Composite.CompositeName = cfe.CompositeName " \
                             "   and Composite.CompositeName = cfed.CompositeName " \
                             "   and cfe.FactorEvent = cfed.FactorEvent ";
   const char   *updateSQL1 = "update MasterComposite " \
                              "   set RecurseIndex = ? " \
                              " where MasterCompositeName = ? ";
   const char   *updateSQL2 = "update CompositeFactorEventDetail " \
                              "   set IsRecursed = 1 " \
                              " where CompositeName = ? "\
                              "   and FactorEvent = ? "  \
                              "   and FactorEventSequence = ? ";
   char    masterCompositeName[NAME_LENGTH+1];
   char    compositeName[NAME_LENGTH+1];
   char    recurseName[NAME_LENGTH+1];
   char    factorValue[VALUE_LENGTH+1];
   int32_t factorEvent, factorEventSequence, recurseIndex, decimalLength;
   bool    success, found;
   Composite *theComposite;

   selectStatement = new SQLStatement(gp_Globals->p_Log, dbInterface, selectSQL);
   selectStatement->BindInputParameter(PRPTEST_AS_COMPOSITE);
   selectStatement->BindSelectedColumn(masterCompositeName, NAME_LENGTH);
   selectStatement->BindSelectedColumn(compositeName, NAME_LENGTH);
   selectStatement->BindSelectedColumn(&factorEvent);
   selectStatement->BindSelectedColumn(&factorEventSequence);
   selectStatement->BindSelectedColumn(&recurseIndex);
   selectStatement->BindSelectedColumn(factorValue, VALUE_LENGTH);
   selectStatement->BindSelectedColumn(&decimalLength);

   updateStatement1 = new SQLStatement(gp_Globals->p_Log, dbInterface, updateSQL1);
   updateStatement1->BindInputParameter(recurseIndex);
   updateStatement1->BindInputParameter(masterCompositeName, NAME_LENGTH, false);

   updateStatement2 = new SQLStatement(gp_Globals->p_Log, dbInterface, updateSQL2);
   updateStatement2->BindInputParameter(compositeName, NAME_LENGTH, false);
   updateStatement2->BindInputParameter(factorEvent);
   updateStatement2->BindInputParameter(factorEventSequence);

   while (selectStatement->FetchRow(false))
   {
      do
      {
         sprintf(recurseName, "%s_R%d", masterCompositeName, ++recurseIndex);
         theComposite = new Composite(dbInterface, gp_Globals->p_Log, recurseName, false);
         found = theComposite->WasFound();
         if (found) delete theComposite;
      } while (found);

      updateStatement1->SetInputParameterValue(recurseIndex, true);
      updateStatement1->SetInputParameterValue(masterCompositeName);
      success = updateStatement1->Execute();

      if (success)
      {
         updateStatement2->SetInputParameterValue(compositeName, true);
         updateStatement2->SetInputParameterValue(factorEvent);
         updateStatement2->SetInputParameterValue(factorEventSequence);
         success = updateStatement2->Execute();
      }

      if (success)
      {
         theComposite->SetMasterCompositeName(masterCompositeName);
         theComposite->SetValue(factorValue);
         success = theComposite->FinalizeChanges();
         delete theComposite;
      }

      if (success)
      {
         dbInterface->Commit();
         gp_Globals->p_Log->LogMessage("Recursed factor from %s as %s", masterCompositeName, recurseName);
      }
      else
         dbInterface->Rollback();
   }

   delete updateStatement1;
   delete updateStatement2;
   delete selectStatement;
}
