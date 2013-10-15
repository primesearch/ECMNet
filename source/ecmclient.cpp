/* ecmclient main() for ECMNet.

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
#include <time.h>
#include "defs.h"

#ifdef WIN32
   #include <io.h>
   #include <direct.h>
#endif

#ifdef UNIX
   #include <signal.h>
#endif

#include "Log.h"
#include "Socket.h"
#include "Work.h"

// global variables
Log     *g_Log;
time_t   g_startTime;
Work   **g_Work;
double  *g_WorkTotalWork;
int32_t  g_WorkCount;
char     g_EmailID[100];
char     g_UserID[100];
char     g_MachineID[100];
char     g_InstanceID[100];
char     g_GMPExe[100];
int32_t  g_LogLimit;
bool     g_IsQuitting;
bool     g_IsTerminating;
bool     g_IsDoingCurve;
int32_t  g_ConnectFrequency;
int32_t  g_ErrorTimeout;
int32_t  g_DebugLevel;
int32_t  g_TotalCurves;
int32_t  g_TotalFactors;
double   g_TotalWork;
int32_t  g_KValue;
int32_t  g_MaxmemValue;

int32_t  g_StartOption;
int32_t  g_StopOption;
int32_t  g_StopASAPOption;

#define  SO_PROMPT               0     // prompt for value
#define  SO_REPORT_WORK          1     // report and get more work
#define  SO_REPORT_QUIT          2     // report and shut down
#define  SO_CONTINUE             9     // continue

#ifdef WIN32
   BOOL WINAPI HandlerRoutine(DWORD /*dwCtrlType*/);
#endif

// procedure declarations
int   FindNextServerForWork(double d_overWorkLevel);
int   FindAnyAvailableServerForWork();
int   FindAnyIncompleteWork();
bool  DoWork(uint32_t workID, uint32_t doOneCurve);
void  ProcessINIFile(const char *configFile);
void  ReprocessINIFile(const char *iniFile);
void  ProcessCommandLine(int count, char *arg[]);
void  ValidateConfiguration();
void  Usage(char *exeName);
void  AllocateWorkClasses(const char *configFile);
void  SetQuitting(int a);
void  CheckForIncompleteWork(void);
int32_t  PromptForStartOption(Work *work);

// Find the next server that we want to get work from based upon
// the percentage of work to be done for that server
int  FindNextServerForWork(double d_overWorkLevel)
{
   int32_t  workID;

   g_Log->Debug(DEBUG_WORK, "in FindNextServerForWork: total work done for client=%.0lf", g_TotalWork);

   for (workID=0; workID<g_WorkCount; workID++)
   {
      if (g_Work[workID]->GetCurvesToDo())
         return workID;

      if (g_Work[workID]->GetWorkPercent() > 0. && !g_IsQuitting)
      {
         if (g_TotalWork == 0.)
         {
            g_Log->Debug(DEBUG_WORK, "suffix: %s, no work done yet, target pct work done=%.0lf",
                         g_Work[workID]->GetWorkSuffix(), g_Work[workID]->GetWorkPercent());

            if (g_Work[workID]->GetWork())
               return workID;
         }
         else if ((g_WorkTotalWork[workID] * 100.)/g_TotalWork < g_Work[workID]->GetWorkPercent() * d_overWorkLevel)
         {
            g_Log->Debug(DEBUG_WORK, "suffix: %s, work done=%.0lf, pct work done=%0lf, target pct work done=%.0lf",
                         g_Work[workID]->GetWorkSuffix(), g_WorkTotalWork[workID],
                         (g_WorkTotalWork[workID] * 100.)/g_TotalWork, g_Work[workID]->GetWorkPercent());

            if (g_Work[workID]->GetWork())
               return workID;
         }
      }
   }

   return -1;
}

// Since we could not connect to our first choice for work, find any
// server that we can connect to
int   FindAnyAvailableServerForWork()
{
   int32_t  workID;

   // Start with servers that we want work from (work percent > 0)
   for (workID=0; workID<g_WorkCount; workID++)
   {
      // Check for quitting before getting work
      if (g_IsQuitting)
         return -1;

      // Only select if there is no incomplete work
      if (!g_Work[workID]->CanDoAnotherCurve())
         if (g_Work[workID]->GetWorkPercent() > 0.)
            if (g_Work[workID]->GetWork())
               return workID;
   }

   // Then try servers with work percent = 0
   for (workID=0; workID<g_WorkCount; workID++)
   {
      // Check for quitting before getting work
      if (g_IsQuitting)
         return -1;

      // Only select if there is no incomplete work
      if (!g_Work[workID]->CanDoAnotherCurve())
         if (g_Work[workID]->GetWorkPercent() == 0.)
            if (g_Work[workID]->GetWork())
               return workID;
   }

   return -1;
}

// Since we could not connect to our first choice for work, find any
// server that we can connect to
int   FindAnyIncompleteWork()
{
   int32_t  workID;

   // If we couldn't connect to any servers then look for any factoring
   // assignments that have not been returned to their respective server
   // and see if we can do another curve
   for (workID=0; workID<g_WorkCount; workID++)
      if (g_Work[workID]->CanDoAnotherCurve())
         return workID;

   return -1;
}

// Take the given work and do it
bool  DoWork(uint32_t workID, uint32_t doOneCurve)
{
   int      factorWasFound = 0;
   int      elapsed, timeh, timem, times;
   double   workDone = 0.;
   time_t   loopStart;

   loopStart = time(NULL);

   while (!g_IsQuitting)
   {
      if (g_IsTerminating || !g_Work[workID]->CanDoAnotherCurve())
         break;

      g_IsDoingCurve = true;
      workDone = g_Work[workID]->DoCurve();
      g_IsDoingCurve = false;

      // If no work was done, that means that the process was interrupted
      if (workDone == 0.0 || g_IsTerminating)
         break;

      ReprocessINIFile("ecmclient.ini");
      g_WorkTotalWork[workID] += workDone;
      g_TotalWork += workDone;
      g_TotalCurves++;
      factorWasFound = g_Work[workID]->WasFactorFound();

      // If a factor was found, report it to the server
      if (factorWasFound)
         break;

      // If there are no more curves to process, return the work done to the server
      if (g_Work[workID]->GetCurvesToDo() <= 0)
         break;

      // If we only need to do one curve
      if (doOneCurve)
         break;

      // If we have exceeded the maximum amount of time between communications to the server
      if (difftime(time(NULL), loopStart) > g_ConnectFrequency*60.)
         break;
   }

   if (g_IsTerminating && !g_IsQuitting)
      SetQuitting(1);

   g_TotalFactors += factorWasFound;

   if (g_Work[workID]->GetCurvesToDo() == 0)
      g_Log->LogMessage("%s: Contacting server due to maximum curves processed for this B1",
                         g_Work[workID]->GetWorkSuffix());

   elapsed = (int) difftime(time(NULL), g_startTime);
   timeh = elapsed/3600;
   timem = (elapsed - timeh*3600) / 60;
   times = elapsed - timeh*3600 - timem*60;
   g_Log->LogMessage("Total Time:%3d:%02d:%02d  Curves: %d  Total Work: %.0lf  Total Composites Factored: %d",
                      timeh, timem, times, g_TotalCurves, g_TotalWork, g_TotalFactors);

   return (workDone > 0.0);
}

#ifdef WIN32
BOOL WINAPI HandlerRoutine(DWORD /*dwCtrlType*/)
{
   if (!g_IsDoingCurve)
   {
      SetQuitting(1);
      return FALSE;
   }

   g_IsTerminating = true;
   return TRUE;
}
#endif

void SetQuitting(int sig)
{
   int   theChar = 0;
   int   workID, workToDo;

   g_IsTerminating = true;
   if (g_IsQuitting && g_StopOption != SO_PROMPT)
   {
      fprintf(stderr, "CTRL-C already hit, so I'm going to terminate now.\n");
      exit(0);
   }

   ReprocessINIFile("ecmclient.ini");

#ifdef WIN32
   if (sig == 1 && g_StopOption == SO_PROMPT)
#else
   if (sig == SIGINT && g_StopOption == SO_PROMPT)
#endif
   {
      workToDo = 0;
      for (workID=0; workID<g_WorkCount; workID++)
         workToDo += (g_Work[workID]->GetCurvesDone() + g_Work[workID]->GetCurvesToDo());

      if (workToDo)
      {
         g_StopOption = SO_PROMPT;

         fprintf(stderr, "You have requested the client to terminate.  What do you want to do\n");
         fprintf(stderr, "  2 = Return completed factoring work then shut down\n");
         fprintf(stderr, "  9 = Do nothing and shut down");

         while (g_StopOption != SO_REPORT_QUIT &&
                g_StopOption != SO_CONTINUE)
         {
            if (theChar != '\n' && theChar != '\r')
               fprintf(stderr, "\nChoose option: ");
            theChar = getchar();

            if (isdigit(theChar))
               g_StopOption = theChar - '0';
         }
      }
   }

   g_StopASAPOption = SO_PROMPT;
   g_IsQuitting = true;
}

int main(int argc, char *argv[])
{
   int32_t  workID;
   int32_t  seconds;
   bool     didWork;
   char     logFileName[200];

   time(&g_startTime);

   // Set default values for the global variables
   g_EmailID[0] = 0;
   g_UserID[0] = 0;
   g_MachineID[0] = 0;
   g_InstanceID[0] = 0;
   g_GMPExe[0] = 0;
   g_WorkCount = 0;
   g_ConnectFrequency = 720;
   g_ErrorTimeout = 20;
   g_LogLimit = 0;
   g_TotalCurves = 0;
   g_TotalFactors = 0;
   g_TotalWork = 0.;
   g_KValue = 0;
   g_MaxmemValue = 0;
   g_StartOption = SO_PROMPT;
   g_StopOption = SO_PROMPT;
   g_StopASAPOption = SO_PROMPT;

    // Get default values from the configuration file
   ProcessINIFile("ecmclient.ini");

   // Get override values from the command line
   ProcessCommandLine(argc, argv);

   ValidateConfiguration();

   sprintf(logFileName, "ecmclient_%s_%s.log", g_MachineID, g_InstanceID);
   g_Log = new Log(g_LogLimit, logFileName, g_DebugLevel, true);

#ifdef WIN32
   SetPriorityClass(GetCurrentProcess(), IDLE_PRIORITY_CLASS);
   SetThreadPriority(GetCurrentThread() ,THREAD_PRIORITY_IDLE);

   /* miscellaneous windows initializations */
   WSADATA wsaData;
   WSAStartup(MAKEWORD(1,1), &wsaData);

   SetConsoleCtrlHandler(HandlerRoutine, TRUE);
#endif

#ifdef UNIX
   nice(20);

   // Ignore SIGHUP, as to not die on logout.
   // We log to file in most cases anyway.
   signal(SIGHUP, SIG_IGN);
   signal(SIGINT, SetQuitting);
   signal(SIGQUIT, SetQuitting);
   signal(SIGTERM, SetQuitting);
#endif

   AllocateWorkClasses("ecmclient.ini");

   // Do not allow these values to be zero
   if (g_ConnectFrequency < 1)
      g_ConnectFrequency = 1;
   if (g_ErrorTimeout < 1)
      g_ErrorTimeout = 1;

   g_Log->LogMessage("ECMNet Client application v%s started", ECMNET_VERSION);
   g_Log->LogMessage("User email address is %s", g_EmailID);

   CheckForIncompleteWork();

   // The main loop...
   // 1. Connect and get some work.
   // 2. Do the work
   // 3. Return the work details (completed curve + possible factors)

   g_IsQuitting = 0;

   while (!g_IsQuitting)
   {
      // Find a server based upon how it is divvied up amongst servers
      workID = FindNextServerForWork(1.0);

      // If we could not get work from the desired server,
      // Try for less oversubscribed servers
      if (workID < 0) workID = FindNextServerForWork(2.0);
      if (workID < 0) workID = FindNextServerForWork(5.0);
      if (workID < 0) workID = FindNextServerForWork(10.0);
      if (workID < 0) workID = FindNextServerForWork(25.0);

      // If we could not get work from the desired server, find any available server
      // that we want to get work from
      if (workID < 0) workID = FindAnyAvailableServerForWork();

      // If we cannot connect to any servers, then find any work that has not been
      // completed, i.e., g_IsQuitting factor has not been found and  doing P+1 or ECM factoring
      if (workID < 0)
      {
         workID = FindAnyIncompleteWork();

         if (workID < 0)
         {
            g_Log->LogMessage("Could not connect to any servers and no work is pending.  Pausing %d minute%s",
                               g_ErrorTimeout, ((g_ErrorTimeout == 1) ? "" : "s"));

            for (seconds=0; seconds<=g_ErrorTimeout*60; seconds++)
            {
               if (g_IsQuitting)
                   break;

               Sleep(1000);
            }
         }
         else
         {
            g_Log->LogMessage("Could not connect to any servers.  Processing another curve for %s",
                               g_Work[workID]->GetWorkSuffix());
            didWork = DoWork(workID, 1);
         }
      }
      else
         didWork = DoWork(workID, 0);

      // If we need to stop ASAP, then do so now
      if (!didWork || g_IsQuitting || g_StopASAPOption != SO_PROMPT)
          break;

      // Clear any socket errors and return all completed work.
      for (workID=0; workID<g_WorkCount; workID++)
      {
         if (!g_Work[workID]->IsDeadWork())
            g_Work[workID]->ReturnWork();
      }
   }

   // Now that we are out of the loop, what do we need to do with any existing workunits
   if (g_StopASAPOption == SO_REPORT_QUIT ||
       (g_StopASAPOption == SO_PROMPT && g_StopOption == SO_REPORT_QUIT))
   {
      for (workID=0; workID<g_WorkCount; workID++)
         g_Work[workID]->ReturnWork();
   }

   g_Log->LogMessage("Client shutdown complete");

   for (workID=0; workID<g_WorkCount; workID++)
      delete g_Work[workID];

   delete [] g_Work;
   delete [] g_WorkTotalWork;

   delete g_Log;

   return 0;
}

// Parse the configuration file to get the default values
void  ProcessINIFile(const char *iniFile)
{
   FILE          *fp;
   char           line[2001];

   if ((fp = fopen(iniFile, "r")) == NULL)
   {
      printf("ini file '%s' could not be opened for reading.\n", iniFile);
      return;
   }

   // Extract the email ID and count the number of servers on this pass
   while (fgets(line, 2000, fp) != NULL)
   {
      StripCRLF(line);

      if (!memcmp(line, "userid=", 7))
         strcpy(g_UserID, line+7);
      else if (!memcmp(line, "email=", 6))
         strcpy(g_EmailID, line+6);
      else if (!memcmp(line, "machineid=", 10))
         strcpy(g_MachineID, line+10);
      else if (!memcmp(line, "instanceid=", 11))
         strcpy(g_InstanceID, line+110);
      else if (!memcmp(line, "server=", 7))
         g_WorkCount++;
      else if (!memcmp(line, "gmpecmexe=", 10))
         strcpy(g_GMPExe, line+10);
      else if (!memcmp(line, "gmpecmkvalue=", 13))
         g_KValue = atol(&line[13]);
      else if (!memcmp(line, "gmpecmmaxmemvalue=", 18))
         g_MaxmemValue = atol(&line[18]);
      else if (!memcmp(line, "debuglevel=", 11))
         g_DebugLevel = atol(&line[11]);
      else if (!memcmp(line, "frequency=", 10))
         g_ConnectFrequency = atol(line+10);
      else if (!memcmp(line, "loglimit=", 9))
         g_LogLimit = (atol(line+9) * 1024 * 1024);
      else if (!memcmp(line, "errtimeout=",11))
         g_ErrorTimeout = atol(line+11);
      else if (!memcmp(line, "startoption=", 12))
         g_StartOption = atol(line+12);
      else if (!memcmp(line, "stopoption=", 11))
         g_StopOption = atol(line+11);
      else if (!memcmp(line, "stopasapoption=", 15))
         g_StopASAPOption = atol(line+15);
   }

   fclose(fp);
}

void     ReprocessINIFile(const char *iniFile)
{
   FILE    *fp;
   char     line[2001];
   int32_t  stopOption = SO_PROMPT;
   int32_t  stopASAPOption = SO_PROMPT;

   if ((fp = fopen(iniFile, "r")) == NULL) return;

   // Extract the email ID and count the number of servers on this pass
   while (fgets(line, 2000, fp) != NULL)
   {
      StripCRLF(line);

      if (!memcmp(line, "debuglevel=", 11))
         g_Log->SetDebugLevel(atol(line+11));
      else if (!memcmp(line, "stopoption=", 11))
         g_StopOption = atol(line+11);
      else if (!memcmp(line, "stopasapoption=", 15))
         g_StopASAPOption = atol(line+15);
   }

   fclose(fp);

   if (stopOption != SO_PROMPT &&
       stopOption != SO_REPORT_QUIT &&
       stopOption != SO_CONTINUE)
   {
      printf("stopoption was updated and is now invalid.  Please fix for it to take affect\n");
   }
   else
      g_StopOption = stopOption;

   if (stopASAPOption != SO_PROMPT &&
       stopASAPOption != SO_REPORT_QUIT &&
       stopASAPOption != SO_CONTINUE)
   {
      printf("stopasapoption was updated and is now invalid.  Please fix for it to take affect\n");
   }
   else
      g_StopASAPOption = stopASAPOption;
}

// Parse the command line to get any overrides of the default values
void  ProcessCommandLine(int count, char *arg[])
{
   int   i = 1;

   while (i < count)
   {
      if (!strcmp(arg[i], "-e"))
      {
         strcpy(g_EmailID, arg[i+1]);
         i++;
      }
      if (!strcmp(arg[i], "-debug"))
         g_DebugLevel = 1;
      else if (!strcmp(arg[i], "-debuglevel"))
      {
         g_DebugLevel = atol(arg[i+1]);
         i++;
      }
      else if (!strcmp(arg[i], "-loglimit"))
      {
         g_LogLimit = atol(arg[i+1]);
         i++;
      }
      else if (!strcmp(arg[i], "-errtimeout"))
      {
         g_ErrorTimeout = atol(arg[i+1]);
         i++;
      }
      else if (!strcmp(arg[i], "-id"))
      {
         strcpy(g_MachineID, arg[i+1]);
         strcpy(g_InstanceID, arg[i+1]);
         i++;
      }
      else
      {
         Usage(arg[0]);
         exit(0);
      }

      i++;
   }
}

void   ValidateConfiguration()
{
   struct stat buf;

   if (strlen(g_EmailID) == 0)
   {
      printf("An email address must be configured.\n");
      exit(-1);
   }

   if (!strchr(g_EmailID,'@') || strchr(g_EmailID,':') || strchr(g_EmailID, ' '))
   {
      printf("Email address supplied (%s) is not valid as it has an embedded ':' or ' '.\n", g_EmailID);
      exit(-1);
   }

   if (!strlen(g_UserID))
   {
      printf("No user ID specified, will use e-mail ID\n");
      strcpy(g_UserID, g_EmailID);
   }

   if (!strlen(g_MachineID))
   {
      printf("No machine ID supplied\n");
      exit(-1);
   }

   if (!strlen(g_InstanceID))
   {
      printf("No instance ID supplied\n");
      exit(-1);
   }

   if (strlen(g_GMPExe) > 0 && (stat(g_GMPExe, &buf) == -1))
   {
      printf("Could not find gmpecmexe executable '%s'.\n", g_GMPExe);
      g_GMPExe[0] = 0;
   }

   if (strlen(g_GMPExe) == 0)
   {
      printf("gmpecmvalue must be specified.\n");
      exit(-1);
   }

   if (g_StartOption != SO_PROMPT &&
       g_StartOption != SO_CONTINUE &&
       g_StartOption != SO_REPORT_WORK &&
       g_StartOption != SO_REPORT_QUIT)
   {
      g_StartOption = SO_PROMPT;
      printf("startoption is invalid, setting it to 0 (prompt).\n");
   }

   if (g_StartOption != SO_PROMPT &&
       g_StartOption != SO_CONTINUE &&
       g_StartOption != SO_REPORT_QUIT)
   {
      g_StopOption = SO_PROMPT;
      printf("stopoption is invalid, setting it to 0 (prompt).\n");
   }

   if (g_StopASAPOption != SO_PROMPT)
   {
      printf("stopasapoption is invalid.  Reset to 0, then restart the client.\n");
      exit(0);
   }
}

void  Usage(char *exeName)
{
   printf("Usage: %s [-errtimeout n] [-loglimit n] [-id clientid]\n", exeName);
   printf("    -errtimeout n Set delay on communication errors to n minutes\n");
   printf("    -loglimit n   Limit log size to 'n' bytes\n");
   printf("    -id clientid  Override client ID in ecmclient.ini\n");
}

void  AllocateWorkClasses(const char *configFile)
{
   FILE     *fp;
   char      line[2001];
   int32_t   workID;
   double    workPercent = 0.0;

   if ((fp = fopen(configFile, "r")) == NULL)
   {
      printf("Configuration file '%s' could not be opened for reading.\n", configFile);
      return;
   }

   g_Work = new Work *[g_WorkCount];
   g_WorkTotalWork = new double[g_WorkCount];

   workID = 0;
   while (fgets(line, 2000, fp) != NULL)
   {
      StripCRLF(line);

      if (!memcmp(line, "server=", 7))
      {
         g_WorkTotalWork[workID] = 0.;
         g_Work[workID] = new Work(g_Log, g_EmailID, g_UserID, g_MachineID, g_InstanceID,
                                   g_GMPExe, line+7, g_KValue, g_MaxmemValue);
         workPercent += g_Work[workID]->GetWorkPercent();
         workID++;
      }
   }

   fclose(fp);

   if (workPercent != 100.0)
   {
      printf("Total work percent must equal 100.\n");
      exit(-1);
   }
}

void  CheckForIncompleteWork(void)
{
   int32_t  workID;
   int32_t  startOption = g_StartOption;
   bool     shutDown;

   shutDown = false;
   for (workID=0; workID<g_WorkCount; workID++)
   {
      // If all work completed, return here otherwise client will sleep right away
      // if there is no work to be done.
      if (!g_Work[workID]->CanDoAnotherCurve())
         g_Work[workID]->ReturnWork();

      if (g_Work[workID]->GetCurvesDone())
      {
         // This allows each one with remaining work to be prompted separately, but
         // if any say "shut down", then the client will exit when done.
         if (g_StartOption == SO_PROMPT)
            startOption = PromptForStartOption(g_Work[workID]);

         switch (startOption)
         {
            case SO_REPORT_WORK:
               g_Work[workID]->ReturnWork();
               break;

            case SO_REPORT_QUIT:
               g_Work[workID]->ReturnWork();
               shutDown = true;
               break;
         }
      }
   }

   if (shutDown)
      exit(0);
}

int32_t  PromptForStartOption(Work *work)
{
   int32_t  option = SO_PROMPT;
   char     theChar = ' ';

   fprintf(stderr, "It appears that the ECMNet client was aborted without completing\n");
   fprintf(stderr, "the workunits asigned by server %s.  What do you want to do with them?\n", work->GetWorkSuffix());
   fprintf(stderr, "  1 = Report completed factoring work, then get more work\n");
   fprintf(stderr, "  2 = Report completed factoring work, then shut down\n");
   fprintf(stderr, "  9 = Continue from client left off when it was shut down\n");

   while (option != SO_CONTINUE &&
          option != SO_REPORT_WORK &&
          option != SO_REPORT_QUIT)
   {
      if (theChar != '\n' && theChar != '\r')
         fprintf(stderr, "\nChoose option: ");
      theChar = (char) getchar();

      if (isdigit(theChar))
         option = (theChar - '0');
   }

   return option;
}
