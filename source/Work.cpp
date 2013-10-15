/* Work Class for ECMNet.

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

#include <ctype.h>
#include "Work.h"

Work::Work(Log *theLog, const char *emailID, const char *userID, const char *machineID, 
           const char *instanceID, const char *gmpExe,
           const char *serverConfiguration, int32_t kValue, int32_t maxmemValue)
{
   char *inLine, *ptr;
   char  first[50], second[50];
   int   colonCount;

   ip_Log = theLog;

   strcpy(is_EmailID, emailID);
   strcpy(is_UserID, userID);
   strcpy(is_MachineID, machineID);
   strcpy(is_Instance, instanceID);
   strcpy(is_GMPExe, gmpExe);

   ii_KValue = kValue;
   ii_MaxmemValue = maxmemValue;
   im_FirstWorkUnit = NULL;

   inLine = new char [strlen(serverConfiguration) + 1];
   strcpy(inLine, serverConfiguration);

   colonCount = 0;
   ptr = inLine;
   while (*ptr)
   {
      if (*ptr == ':')
      {
         *ptr = ' ';
          colonCount++;
      }
      ptr++;

      if (colonCount > 3)
         break;
   }

   if (colonCount != 3)
   {
      printf("server configuration [%s] is invalid.  Exiting", serverConfiguration);
      exit(0);
   }

   // For 3.x and later, the prefix is first followed by the percentage, but
   // for older releases, the percentage is first.
   sscanf(inLine, "%s %s %s %d", first, second, is_ServerName, &ii_PortID);
   if (isdigit(*first) && !isdigit(*second))
   {
      strcpy(is_WorkSuffix, second);
      sscanf(first, "%lf", &id_WorkPercent);
   }
   else
   {
      strcpy(is_WorkSuffix, first);
      sscanf(second, "%lf", &id_WorkPercent);
   }

   ip_Socket = new ClientSocket(ip_Log, is_ServerName, ii_PortID);
   sprintf(is_SaveFileName, "work_%s_%s_%s.save", is_MachineID, is_Instance, is_WorkSuffix);

   Load();

   GetGMPEXEVersion();
   delete [] inLine;
}

Work::~Work()
{
   Save();

   delete ip_Socket;
}

double   Work::DoCurve()
{
   WorkUnit *wu, *tempWU;
   double    workDone = 0.0;

   wu = im_FirstWorkUnit;
   while (wu)
   {
      if (wu->GetJustDidCurve())
      {
         wu->SetJustDidCurve(false);
         tempWU = (WorkUnit *) wu->GetNextWorkUnit();
         if (tempWU && tempWU->CanDoAnotherCurve(false))
            break;
      }

      wu = (WorkUnit *) wu->GetNextWorkUnit();
   }

   if (!wu) wu = im_FirstWorkUnit;

   if (wu->CanDoAnotherCurve(false))
   {
      workDone = wu->DoCurve(false);

      Save();
   }

   return workDone;
}

bool  Work::CanDoAnotherCurve(void)
{
   WorkUnit *wu;

   wu = im_FirstWorkUnit;
   while (wu)
   {
      if (wu->CanDoAnotherCurve(false))
         return true;

      wu = (WorkUnit *) wu->GetNextWorkUnit();
   }

   return false;
}

bool  Work::GetWork(void)
{
   char *serverVersion, *readBuf;
   bool  accepted;
   WorkUnit *wu;

   if (im_FirstWorkUnit) return false;

   if (!ip_Socket->Open(is_EmailID, is_UserID, is_MachineID, is_Instance))
      return false;

   ip_Log->LogMessage("%s: Getting work from server %s at port %d",
                      is_WorkSuffix, ip_Socket->GetServerName(), ip_Socket->GetPortID());

   ip_Socket->Send("GETWORK %s %s", ECMNET_VERSION, is_GMPECMVersion);

   serverVersion = ip_Socket->GetServerVersion();

   while (1 == 1)
   {
      wu = new WorkUnit(ip_Log, is_MachineID, is_Instance, is_WorkSuffix, is_GMPExe, ii_KValue, ii_MaxmemValue);
      accepted = wu->GetWorkUnitFromServer(ip_Socket);

      if (!accepted)
      {
         delete wu;
         break;
      }

      ip_Log->Debug(DEBUG_WORK, "%s: Received %d curves of %s work for B1 of %.0lf for %s", is_WorkSuffix,
                    wu->GetCurvesToDo(), wu->GetFactorMethod(), wu->GetB1(), wu->GetWorkUnitName());
      AddWorkUnit(wu);

      // Server version 2.x can only send one work unit
      if (serverVersion[0] == '2') break;
   }

   Save();

   // Get greeting
   readBuf = ip_Socket->Receive(2);
   while (readBuf && memcmp(readBuf, "End Greeting", 12))
   {
      if (memcmp(readBuf, "Start Greeting", 14))
         printf("%s\n", readBuf);
      readBuf = ip_Socket->Receive(2);
   }

   ip_Socket->Close();
   return true;
}

bool  Work::ReturnWork(void)
{
   WorkUnit *wu, *nextWU;
   char *readBuf;

   if (!im_FirstWorkUnit)
      return true;

   if (!ip_Socket->Open(is_EmailID, is_UserID, is_MachineID, is_Instance))
      return false;

   ip_Log->LogMessage("%s: Returning work to server %s at port %ld",
                      is_WorkSuffix, ip_Socket->GetServerName(), ip_Socket->GetPortID());

   if (memcmp(ip_Socket->GetServerVersion(), "3", 1) >= 0)
      ip_Socket->Send("Start Return");

   wu = im_FirstWorkUnit;
   im_FirstWorkUnit = NULL;
   while (wu)
   {
      nextWU = (WorkUnit *) wu->GetNextWorkUnit();

      if (wu->ReturnWorkUnit(ip_Socket))
         delete wu;
      else
      {
         wu->SetNextWorkUnit(NULL);
         AddWorkUnit(wu);
      }

      wu = nextWU;
   }

   if (memcmp(ip_Socket->GetServerVersion(), "3", 1) >= 0)
   {
      ip_Socket->Send("End of Return Work");
      readBuf = ip_Socket->Receive(2);
      while (readBuf)
      {
         if (!strcmp(readBuf, "End of Message")) break;
         if (memcmp(readBuf, "INFO", 4))
            ip_Log->LogMessage(readBuf);
         readBuf = ip_Socket->Receive(2);
      }
   }

   ip_Socket->Close();

   if (im_FirstWorkUnit)
      return false;

   unlink(is_SaveFileName);

   return true;
}

// Save the current status
void  Work::Save()
{
   FILE *fp;
   WorkUnit *wu;

   if (!im_FirstWorkUnit)
      return;

   if ((fp = fopen(is_SaveFileName, "wt")) == NULL)
   {
      printf("Could not file [%s] for writing.  Exiting program", is_SaveFileName);
      exit(-1);
   }

   fprintf(fp, "ServerName=%s\n", ip_Socket->GetServerName());
   fprintf(fp, "PortID=%d\n", ip_Socket->GetPortID());
   fprintf(fp, "ServerVersion=%s\n", ip_Socket->GetServerVersion());

   wu = im_FirstWorkUnit;
   while (wu)
   {
      wu->Save(fp);
      wu = (WorkUnit *) wu->GetNextWorkUnit();
   }

   fclose(fp);
}

// Load the previous status
void  Work::Load()
{
   FILE          *fp;
   Socket        *theSocket;
   char          *line, serverName[200];
   int32_t        portID = 0;
   WorkUnit      *wu;
   struct stat    buf;

   ib_DeadWork = false;

   // If there is not save file, then do nothing
   if (stat(is_SaveFileName, &buf) == -1)
      return;

   if ((fp = fopen(is_SaveFileName, "r")) == NULL)
   {
      printf("Could not open file [%s] for reading", is_SaveFileName);
      exit(-1);
   }

   line = new char[BUFFER_SIZE];

   while (fgets(line, BUFFER_SIZE, fp) != NULL)
   {
      StripCRLF(line);

      if (!memcmp(line, "ServerName=", 11))
         strcpy(serverName, line+11);
      else if (!memcmp(line, "PortID=", 7))
         portID = atol(line+7);
      else if (!memcmp(line, "ServerVersion=", 14))
         ip_Socket->SetServerVersion(line+14);
      else
      {
         wu = new WorkUnit(ip_Log, is_MachineID, is_Instance, is_WorkSuffix, is_GMPExe, ii_KValue, ii_MaxmemValue);
         wu->Load(line);
         AddWorkUnit(wu);
      }
   }

   delete [] line;

   fclose(fp);

   if (strcmp(ip_Socket->GetServerName(), serverName) || ip_Socket->GetPortID() != portID)
   {
      ip_Log->LogMessage("%s: The server (%s) and port (%ld) in save file do not match the input configuration.",
                         is_WorkSuffix, serverName, portID, is_SaveFileName);
      ip_Log->LogMessage("%s: server (%s) and port (%ld).  Attempting to notify correct server of work done.",
                         is_WorkSuffix, ip_Socket->GetServerName(), ip_Socket->GetPortID());

      theSocket = new ClientSocket(ip_Log, serverName, portID);
      if (theSocket->Open())
      {
         if (!ReturnWork())
         {
            ip_Log->LogMessage("%s: Completed work cannot be sent back to the the original server.", is_WorkSuffix);
            ib_DeadWork = true;
         }
         theSocket->Close();
      }

      delete theSocket;
   }
}

// Get the current version of GMP-ECM being used
void  Work::GetGMPEXEVersion(void)
{
   char     inFile[50], outFile[50], line[241], *ptr;
   FILE    *fp;

   if (strlen(is_GMPExe) == 0)
   {
      strcpy(is_GMPECMVersion, "none");
      return;
   }

   sprintf(inFile, "temp_%s_%s.in", is_MachineID, is_Instance);
   sprintf(outFile, "temp_%s_%s.out", is_MachineID, is_Instance);
   if ((fp = fopen(inFile, "wt")) == NULL)
   {
      printf("Could not file [%s] for writing.  Exiting program", inFile);
      exit(-1);
   }
   fclose(fp);

   sprintf(line, "%s 100 < %s >> %s", is_GMPExe, inFile, outFile);
   system(line);

   unlink("temp.in");

   is_GMPECMVersion[0] = 0;

   if ((fp = fopen(outFile, "r")) == NULL)
      return;

   // Keep reading until we find the line with the version number
   while (fgets(line, 240, fp) != NULL)
   {
      if (!memcmp(line, "GMP-ECM", 7))
      {
         strncpy(is_GMPECMVersion, line+8, sizeof(is_GMPECMVersion));
         break;
      }
   }
   fclose(fp);

   ptr = strchr(is_GMPECMVersion, ' ');
   if (ptr) *ptr = 0;

   unlink(inFile);
   unlink(outFile);
}

void  Work::AddWorkUnit(WorkUnit *wu)
{
   WorkUnit *lastWU;

   if (!im_FirstWorkUnit)
   {
      im_FirstWorkUnit = wu;
      return;
   }

   lastWU = im_FirstWorkUnit;
   while (lastWU->GetNextWorkUnit())
      lastWU = (WorkUnit *) lastWU->GetNextWorkUnit();

   lastWU->SetNextWorkUnit(wu);
}

bool  Work::WasFactorFound(void)
{
   WorkUnit *wu;

   wu = im_FirstWorkUnit;
   while (wu)
   {
      if (wu->WasFactorFound()) return true;
      wu = (WorkUnit *) wu->GetNextWorkUnit();
   }

   return false;
}

int32_t  Work::GetCurvesDone(void)
{
   WorkUnit *wu;
   int32_t  totalCurves = 0;

   wu = im_FirstWorkUnit;
   while (wu)
   {
      totalCurves += wu->GetCurvesDone();
      wu = (WorkUnit *) wu->GetNextWorkUnit();
   }

   return totalCurves;
}

int32_t  Work::GetCurvesToDo(void)
{
   WorkUnit *wu;
   int32_t  totalCurves = 0;

   wu = im_FirstWorkUnit;
   while (wu)
   {
      totalCurves += wu->GetCurvesToDo();
      wu = (WorkUnit *) wu->GetNextWorkUnit();
   }

   return totalCurves;
}
