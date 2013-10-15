/* UpgradeFromV2 Class for ECMNet.

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

#include "UpgradeFromV2.h"
#include "CompositeFactorEvent.h"
#include "SQLStatement.h"

UpgradeFromV2::UpgradeFromV2(Log *theLog, const char *serverID)
{
   ip_Log = theLog;
   ip_DBInterface = new DBInterface(ip_Log, "database.ini");
   ip_DBInterface->Connect(0);

   ic_PreviousName[0] = 0;
   ii_Successful = ii_Failed = 0;
   strcpy(ic_ServerID, serverID);
}

UpgradeFromV2::~UpgradeFromV2()
{
   delete ip_DBInterface;
}

void  UpgradeFromV2::Start(const char *saveFileName)
{
   FILE       *fPtr;
   bool        success = true;
   int32_t     factorEventIndex = 0;
   char       *readBuf, *ptr;

   if ((fPtr = fopen(saveFileName, "r")) == NULL)
   {
      ip_Log->LogMessage("Could not open file [%s].  Unable to run this server", saveFileName);
      exit(0);
   }
   readBuf = new char[BUFFER_SIZE];

   while (fgets(readBuf, BUFFER_SIZE, fPtr) != NULL)
   {
      StripCRLF(readBuf);

      // Empty line, skip it
      if (strlen(readBuf) == 0)
         continue;

      ptr = strchr(readBuf, ' ');
      if (!ptr)
      {
         printf("Line [%s] in INI file cannot be parsed\n", readBuf);
         continue;
      }

      *ptr = 0x00;

      // The 'N' line for each candidate must be first and each successive line that is
      // not 'N' must be for the same candidate
      if ((*(ptr+1) != 'N') && strcmp(ic_PreviousName, readBuf))
      {
         printf("Name [%s] is out of sequence.  Please fix the issue and restart the server\n", readBuf);
         exit(-1);
      }

      switch (*(ptr+1))
      {
         case 'N':
            CommitPreviousComposite();

            if ((ii_Successful + ii_Failed) % 50 == 0)
               printf("Current update stats: %d successful, %d failed\r", ii_Successful, ii_Failed);

            NewComposite(readBuf, ptr+3);
            factorEventIndex = 0;
            break;

         case 'B':
            if (success) success = InsertWorkDone(ptr+3);
            break;

         case 'P':
            if (success) success = ExtractParameters(ptr+3);
            break;

         case 'F':
            if (!ip_CompositeFactorEvent)
               ip_CompositeFactorEvent = new CompositeFactorEvent(ip_DBInterface, ip_Log, ip_Composite->GetCompositeName().c_str(),
                                                                  ip_Composite->NextFactorEvent(), time(NULL));

            if (success) success = InsertFactor(ptr+3, factorEventIndex);
            break;
      }
   }

   CommitPreviousComposite();

   delete [] readBuf;
   fclose(fPtr);

   printf("Final Update stats:  %d successful, %d failed   \n", ii_Successful, ii_Failed);
   if (ii_Failed)
   {
      printf("Due to the failure to upgrade all of the original composites, the server\n");
      printf("is going to shut down.  It is recommended that you fix the problems in the\n");
      printf("source file then remove the records from the database tables, then try again.\n");
      exit(0);
   }

   printf("Although the upgrade did not encounter any errors, the server is now going to\n");
   printf("shut down so that you can verify the contents of the database manually.  Once\n");
   printf("you have done that, you can restart the server, but without the -u option.\n");
   exit(0);
}

void     UpgradeFromV2::CommitPreviousComposite(void)
{
   if (strlen(ic_PreviousName) > 0)
   {
      if (ib_Success) ib_Success = ip_MasterComposite->FinalizeChanges();
      if (ib_Success) ib_Success = ip_Composite->FinalizeChanges();
      if (ib_Success && ip_CompositeFactorEvent) ib_Success = ip_CompositeFactorEvent->FinalizeChanges();
      if (ib_Success)
      {
         ip_DBInterface->Commit();
         ii_Successful++;
      }
      else
      {
         ip_DBInterface->Rollback();
         ii_Failed++;
      }
      delete ip_Composite;
      delete ip_MasterComposite;
      if (ip_CompositeFactorEvent) delete ip_CompositeFactorEvent;
   }

   ip_MasterComposite = NULL;
   ip_Composite = NULL;
   ip_CompositeFactorEvent = NULL;
}

void     UpgradeFromV2::NewComposite(const char *name, const char *value)
{
   ib_Success = true;
   strcpy(ic_PreviousName, name);

   if (strchr(value, ' '))
   {
      ip_Log->LogMessage("Error: Value for composite %s has an embedded space (%s)", name, value);
      ib_Success = false;
   }

   ip_MasterComposite = new MasterComposite(ip_DBInterface, ip_Log, name);
   ip_Composite = new Composite(ip_DBInterface, ip_Log, name, true);

   if (ip_MasterComposite->WasFound() || ip_Composite->WasFound())
   {
      ip_Log->LogMessage("Error: Composite %s already exists", name);
      ib_Success = false;
   }

   // Create our objects
   if (ib_Success) ib_Success = ip_MasterComposite->SetValue(value);
   if (ib_Success) ib_Success = ip_Composite->SetValue(value);
   if (ib_Success) ib_Success = ip_Composite->SetMasterCompositeName(ip_MasterComposite->GetName());

   // These will insert the records, but not commit them.  That allows us to insert
   // child records without running into a referential integrity error.
   if (ib_Success) ib_Success = ip_MasterComposite->FinalizeChanges();
   if (ib_Success) ib_Success = ip_Composite->FinalizeChanges();
}

bool     UpgradeFromV2::InsertWorkDone(const char *b1Info)
{
   double   b1;
   int32_t  count;
   int32_t  curvesECM, toMasterECM;
   int32_t  curvesPP1, toMasterPP1;
   int32_t  curvesPM1, toMasterPM1;

   count = sscanf(b1Info, "%lf %d:%d %d:%d %d:%d", &b1,
                   &curvesECM, &toMasterECM,
                   &curvesPP1, &toMasterPP1,
                   &curvesPM1, &toMasterPM1);

   if (count != 7)
   {
      ip_Log->LogMessage("Line <%s> could not be scanned", b1Info);
      return false;
   }

   if (ib_Success) ib_Success = ip_MasterComposite->AddWorkDone(FM_ECM, b1, toMasterECM, curvesECM);
   if (ib_Success) ib_Success = ip_MasterComposite->AddWorkDone(FM_PM1, b1, toMasterPM1, curvesPM1);
   if (ib_Success) ib_Success = ip_MasterComposite->AddWorkDone(FM_PP1, b1, toMasterPP1, curvesPP1);

   return true;
}

bool     UpgradeFromV2::InsertFactor(const char *factorInfo, int32_t factorEventIndex)
{
   char     factorMethod[20];
   char     factorType[20];
   char     factorValue[VALUE_LENGTH+1];
   char     emailID[200];
   char     userID[200];
   char     instanceID[200];
   char     *pos;
   double   b1 = 0.0;
   int32_t  suffix, upstream;
   int32_t  count = 0;
   int64_t  sigma;

   count = sscanf(factorInfo, "%s %s %s %s %s %lf %"PRId64" %d %d",
                  factorMethod, factorValue, factorType, emailID, instanceID, &b1, &sigma, &suffix, &upstream);

   if (count != 9)
   {
      ip_Log->LogMessage("Line <%s> could not be scanned", factorInfo);
      return false;
   }

   strcpy(userID, emailID);
   pos = strchr(userID, '@');
   if (pos) *pos = 0;

   ip_MasterComposite->IncrementFactorCount();

   ip_CompositeFactorEvent->SetSentToMaster((upstream ? true : false));
   ip_CompositeFactorEvent->SetIDs(emailID, userID, instanceID, instanceID, ic_ServerID);
   ip_CompositeFactorEvent->SetCurveDetail(factorMethod, 0, b1, 0.0, 0.0, 0.0, sigma, 0);

   ip_CompositeFactorEvent->AddEventDetail(factorValue);

   return true;
}

bool  UpgradeFromV2::ExtractParameters(char *paramInfo)
{
   uint32_t fields;
   int32_t  ignore;
   int64_t  theTime;
   char     active[30], ignoreStr[100];

   for (fields=0; fields<strlen(paramInfo); fields++)
      if (paramInfo[fields] == ',')
         paramInfo[fields] = ' ';

   fields = sscanf(paramInfo, "%"PRId64" %d %d %s %s %s", &theTime, &ignore, &ignore, active, ignoreStr, ignoreStr);

   if (fields != 6)
   {
      ip_Log->LogMessage("%s: Line %s cannot be parsed\n", ip_MasterComposite->GetName().c_str(), paramInfo);
      return false;
   }

   if (!strcmp(active, "inactive"))
      ip_MasterComposite->SetActive(false);

   return true;
}
