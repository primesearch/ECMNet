/* WorkReceiver Class for ECMNet.

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

#include "WorkReceiver.h"

WorkReceiver::WorkReceiver(DBInterface *dbInterface, HelperSocket *theSocket, Log *theLog, const char *serverID, bool recurse)
{
   ip_DBInterface = dbInterface;
   ip_Socket = theSocket;
   ip_Log = theLog;

   is_ServerID = serverID;
   ib_Recurse = recurse;

   ip_Composite = NULL;
   ip_MasterComposite = NULL;
   ip_UserStats = NULL;
   ip_CompositeFactorEvent = NULL;
}

WorkReceiver::~WorkReceiver()
{
}

// Process messages being returned by a 2.x client.
void  WorkReceiver::ProcessMessageOld(const char *theMessage)
{
   int32_t  testsReturned = 0;
   bool     success, factorIsNumber = false;
   int32_t  curvesDone = 0;
   int64_t  sigma = 0;
   double   b1 = 0.0;
   char     junk[100];
   char     compositeName[NAME_LENGTH+1];
   char     compositeValue[VALUE_LENGTH+1];
   char     factorMethod[METHOD_LENGTH+1];
   char     factorValue[VALUE_LENGTH+1];
   char     cofactorValue[VALUE_LENGTH+1];
   char     factorType[METHOD_LENGTH+1];
   char     cofactorType[METHOD_LENGTH+1];
   char    *readBuf, *ptr;
   Log     *factorLog;

   success = false;
   readBuf = ip_Socket->Receive(2);
   while (readBuf)
   {
      if (!strcmp(readBuf, "End of Message"))
      {
         success = true;
         break;
      }
      else if (!memcmp(readBuf, "return_ECMname: ", 16))
         strcpy(compositeName, readBuf+16);
      else if (!memcmp(readBuf, "return_ECMnumber: ", 18))
         strcpy(compositeValue, readBuf+18);
      else if (!memcmp(readBuf, "return_FactorMethod: ", 21))
         strcpy(factorMethod, readBuf+21);
      else if (!memcmp(readBuf, "return_FactorIsNumber", 21))
         factorIsNumber = true;
      else if (!memcmp(readBuf, "return_B1: ", 11))
         b1 = atof(readBuf+11);
      else if (!memcmp(readBuf, "return_Curves: ", 15))
         curvesDone = atol(readBuf+15);

      readBuf = ip_Socket->Receive(2);
   }

   if (success)
   {
      StartWork(compositeName);

      success = ProcessWork(compositeValue, factorMethod, factorIsNumber, b1, curvesDone, 0);

      if (EndWork(success))
         testsReturned++;
   }

   factorValue[0] = 0;
   readBuf = ip_Socket->Receive(2);
   while (readBuf)
   {
      if (!strcmp(readBuf, "End of Message"))
      {
         ip_Socket->Send("OK.");
         break;
      }
      else if (!memcmp(readBuf, "factor_ECMnumber: ", 18))
         strcpy(compositeValue, readBuf+18);
      else if (!memcmp(readBuf, "factor_FactorMethod: ", 21))
         strcpy(factorMethod, readBuf+21);
      else if (!memcmp(readBuf, "factor_B1: ", 11))
         b1 = atof(readBuf+11);
      else if (!memcmp(readBuf, "factor_Factor: ", 15))
         strcpy(factorValue, readBuf+15);
      else if (!memcmp(readBuf, "factor_FactorType: ", 19))
         strcpy(factorType, readBuf+19);
      else if (!memcmp(readBuf, "factor_CoFactor: ", 17))
         strcpy(cofactorValue, readBuf+17);
      else if (!memcmp(readBuf, "factor_CoFactorType: ", 21))
         strcpy(cofactorType, readBuf+21);
      else if (!memcmp(readBuf, "factor_FACTOR: ", 15))
      {
         ptr = readBuf;
         while (*ptr)
         {
            if (*ptr == ':') *ptr = ' ';
            ptr++;
         }
         if (sscanf(readBuf+15, "%s %s %s %s", factorValue, factorType, cofactorValue, cofactorType) != 4)
         {
            ip_Socket->Send("ERR parsing <%s> readBuf");
            break;
         }
      }
      else if (!memcmp(readBuf, "factor_FINDER: ", 15))
         strcpy(junk, readBuf+15);
      else if (!memcmp(readBuf, "factor_sigma: ", 14))
         sigma = (int64_t) atof(readBuf+14);

      readBuf = ip_Socket->Receive(2);
   }

   if (success && strlen(factorValue))
   {
      StartWork(compositeName);

      ip_CompositeFactorEvent = new CompositeFactorEvent(ip_DBInterface, ip_Log, ip_Composite->GetCompositeName(),
                                                         ip_Composite->NextFactorEvent(), il_ReportedDate);
      ip_CompositeFactorEvent->SetIDs(ip_Socket->GetEmailID(), ip_Socket->GetUserID(), ip_Socket->GetMachineID(),
                                      ip_Socket->GetInstanceID(), is_ServerID);
      ip_CompositeFactorEvent->SetCurveDetail(factorMethod, b1);
      ip_CompositeFactorEvent->SetSigma(sigma);

      factorLog = new Log(0, "Factors.log", 0, true);
      ip_Log->LogMessage("%d: User %s factored %s using %s B1=%.0lf",
                         ip_Socket->GetSocketID(), ip_Socket->GetUserID(), 
                         compositeName, factorMethod, b1);

      AddFactor(factorLog, factorValue, factorType);
      AddFactor(factorLog, cofactorValue, cofactorType);

      delete factorLog;

      EndWork(success);
   }

   if (!testsReturned)
      ip_Socket->Send("INFO: There were no identifiable test results");
   else
      ip_Socket->Send("INFO: All results were accepted");

   ip_Socket->Send("End of Message");
}

void  WorkReceiver::StartWork(const char *compositeName)
{
   ip_Composite = new Composite(ip_DBInterface, ip_Log, compositeName, true);
   ip_MasterComposite = new MasterComposite(ip_DBInterface, ip_Log, ip_Composite->GetMasterCompositeName());
   ip_UserStats = new UserStats(ip_DBInterface, ip_Log, ip_Socket->GetUserID());

   ip_MasterComposite->Lock(is_ServerID.c_str());

   il_ReportedDate = time(NULL);
   ip_CompositeFactorEvent = NULL;
}

// Process messages being returned by a 3.0 and newer client.
void  WorkReceiver::ProcessMessage(void)
{
   char    *readBuf, goodMessage[100];
   char     compositeName[NAME_LENGTH+1];
   bool     success = true;

   ii_WorkAccepted = ii_FactorsAccepted = 0;

   readBuf = ip_Socket->Receive();
   while (readBuf)
   {
      if (!strcmp(readBuf, "End of Return Work")) break;

      if (!strcmp(readBuf, goodMessage))
         EndWork(success);
      else if (!memcmp(readBuf, "Abandon Work", 11))
      {
         EndWork(success);
         AbandonWork(readBuf);
      }
      else if (!memcmp(readBuf, "Return Work", 11))
      {
         EndWork(false);

         success = ProcessWork(readBuf, compositeName);
         if (success)
            sprintf(goodMessage, "End Return Work for %s", compositeName);
         else
            sprintf(goodMessage, "not gonna match");
      }
      else if (success && !memcmp(readBuf, "Return Factor Event for ", 24))
         success = ProcessFactorEvent(readBuf, compositeName);
      else if (success && !memcmp(readBuf, "Return Factor Event Detail for ", 31))
         success = ProcessFactorEventDetail(readBuf, compositeName);
      else
      {
         success = false;
         ip_Socket->Send("ERROR: Did not understand message <%s>", readBuf);
         ip_Log->LogMessage("%d: Did not understand message <%s>", ip_Socket->GetSocketID(), readBuf);
      }

      readBuf = ip_Socket->Receive(2);
   }

   ip_DBInterface->Rollback();

   if (!ii_WorkAccepted)
      ip_Socket->Send("INFO: There were no identifiable test results");
   else
   {
      if (ii_FactorsAccepted)
         ip_Socket->Send("INFO: Work for %d composite%s accepted.  %d factor%s accepted",
                         ii_WorkAccepted, (ii_WorkAccepted > 1 ? "s" : ""),
                         ii_FactorsAccepted, (ii_FactorsAccepted > 1 ? "s" : ""));
      else
         ip_Socket->Send("INFO: Work for %d composite%s accepted.  No factors reported",
                         ii_WorkAccepted, (ii_WorkAccepted > 1 ? "s" : ""));
   }

   ip_Socket->Send("End of Message");
}

bool  WorkReceiver::EndWork(bool success)
{
   if (!ip_MasterComposite) return true;

   if (success)
   {
      ip_MasterComposite->SetLastUpdateTime();
      success = ip_MasterComposite->FinalizeChanges();
      if (success) success = ip_Composite->FinalizeChanges();
      if (success) success = ip_UserStats->FinalizeChanges();
      if (success && ip_CompositeFactorEvent) success = ip_CompositeFactorEvent->FinalizeChanges();
      if (success) success = ip_MasterComposite->UpdateIfFullyFactored();
   }

   if (success)
   {
      ii_WorkAccepted++;
      ip_DBInterface->Commit();

      if (!memcmp(ip_Socket->GetClientVersion(), "2.x", 3))
         ip_Socket->Send("OK.");
      else
         ip_Socket->Send("Accepted Work for %s", ip_Composite->GetCompositeName().c_str());
   }
   else
      ip_DBInterface->Rollback();

   if (ip_MasterComposite) { delete ip_MasterComposite; ip_MasterComposite = NULL; }
   if (ip_Composite) { delete ip_Composite; ip_Composite = NULL; }
   if (ip_UserStats) { delete ip_UserStats; ip_UserStats = NULL; }
   if (ip_CompositeFactorEvent) { delete ip_CompositeFactorEvent; ip_CompositeFactorEvent = NULL; }

   return success;
}

// Given a 3.0 client, parse the returned work message and do all of the updates.
bool  WorkReceiver::ProcessWork(const char *buffer, char *compositeName)
{
   int32_t  scanned;
   char     compositeValue[VALUE_LENGTH+1];
   char     factorMethod[METHOD_LENGTH+1];
   int32_t  factorIsNumber;
   int32_t  curvesDone;
   int32_t  length;
   double   b1, timeSpent;

   scanned = sscanf(buffer, "Return Work for %s %s %s %d %lf %d %lf %d",
                    compositeName, compositeValue, factorMethod,
                    &factorIsNumber, &b1, &curvesDone, &timeSpent, &length);

   if (scanned != 8)
   {
      ip_Socket->Send("Message <%s> could not be parsed", buffer);
      ip_Log->LogMessage("%d: Message <%s> could not be parsed", ip_Socket->GetSocketID(), buffer); 
      return false;
   }

   if (length != (int32_t) strlen(compositeValue))
   {
      ip_Socket->Send("Composite length mismatch.  Message ignored");
      ip_Log->LogMessage("%d: Composite length mismatch.  Message ignored", ip_Socket->GetSocketID()); 
      return false;
   }

   StartWork(compositeName);

   return ProcessWork(compositeValue, factorMethod, factorIsNumber, b1, curvesDone, timeSpent);
}

void  WorkReceiver::AbandonWork(const char *buffer)
{
   int32_t  scanned;
   char     compositeName[NAME_LENGTH+1];
   char     factorMethod[METHOD_LENGTH+1];
   double   b1;
   SQLStatement *deleteStatement;
   const char *deleteSQL = "delete from PendingWork " \
                           " where UserID = ? " \
                           "   and MachineID = ? " \
                           "   and ClientID = ? " \
                           "   and CompositeName = ? " \
                           "   and FactorMethod = ? " \
                           "   and B1 = ?;";

   scanned = sscanf(buffer, "Abandon Work for %s %s %lf",
                    compositeName, factorMethod, &b1);

   if (scanned != 3)
   {
      ip_Socket->Send("Message <%s> could not be parsed", buffer);
      ip_Log->LogMessage("%d: Message <%s> could not be parsed", ip_Socket->GetSocketID(), buffer); 
      return;
   }

   deleteStatement = new SQLStatement(ip_Log, ip_DBInterface, deleteSQL);
   deleteStatement->BindInputParameter(ip_Socket->GetUserID(), ID_LENGTH);
   deleteStatement->BindInputParameter(ip_Socket->GetMachineID(), ID_LENGTH);
   deleteStatement->BindInputParameter(ip_Socket->GetInstanceID(), ID_LENGTH);
   deleteStatement->BindInputParameter(compositeName, NAME_LENGTH);
   deleteStatement->BindInputParameter(factorMethod, METHOD_LENGTH);
   deleteStatement->BindInputParameter(b1);

   if (deleteStatement->Execute())
      ip_DBInterface->Commit();
   else
      ip_DBInterface->Rollback();

   delete deleteStatement;
}

// Given a 3.0 client, parse the returned factor message and do all of the updates.
bool  WorkReceiver::ProcessFactorEvent(const char *buffer, const char *compositeName)
{
   int32_t  scanned;
   char     factorCompositeName[NAME_LENGTH+1];
   char     factorMethod[METHOD_LENGTH+1];
   int32_t  factorStep;
   int64_t  sigma, x0;
   double   b1, b2, factorB1Time, factorB2Time;
   Log     *factorLog;

   scanned = sscanf(buffer, "Return Factor Event for %s %s %d %lf %lf %lf %lf %"PRId64" %"PRId64"",
                    factorCompositeName, factorMethod, &factorStep,
                    &b1, &factorB1Time, &b2, &factorB2Time, &sigma, &x0);

   if (scanned != 9)
   {
      ip_Socket->Send("Message <%s> could not be parsed", buffer);
      ip_Log->LogMessage("%d: Message <%s> could not be parsed", ip_Socket->GetSocketID(), buffer); 
      return false;
   }

   if (ip_Composite->GetCompositeName() != (string) factorCompositeName)
   {
      ip_Socket->Send("Factor is for a different composite.  Message ignored");
      ip_Log->LogMessage("%d: Factor Event is for a different composite.  Message ignored", ip_Socket->GetSocketID()); 
      return false;
   }

   ip_CompositeFactorEvent = new CompositeFactorEvent(ip_DBInterface, ip_Log, ip_Composite->GetCompositeName(),
                                                      ip_Composite->NextFactorEvent(), il_ReportedDate);
   ip_CompositeFactorEvent->SetIDs(ip_Socket->GetEmailID(), ip_Socket->GetUserID(), ip_Socket->GetMachineID(),
                                   ip_Socket->GetInstanceID(),  is_ServerID);
   ip_CompositeFactorEvent->SetCurveDetail(factorMethod, factorStep, b1, factorB1Time, b2, factorB2Time,sigma, x0);

   factorLog = new Log(0, "Factors.log", 0, true);

   if (!strcmp(factorMethod, FM_ECM))
      factorLog->LogMessage("%d: %s factored using %s B1=%.0lf B2=%.0lf sigma=%"PRId64" (found in step %d)",
                            ip_Socket->GetSocketID(), factorCompositeName, factorMethod, b1, b2, sigma, factorStep);
   else
      factorLog->LogMessage("%d: %s factored using %s B1=%.0lf B2=%.0lf x0=%"PRId64" (found in step %d)",
                            ip_Socket->GetSocketID(), factorCompositeName, factorMethod, b1, b2, x0, factorStep);

   delete factorLog;

   return true;
}

// Given a 3.0 client, parse the returned factor message and do all of the updates.
bool  WorkReceiver::ProcessFactorEventDetail(const char *buffer, const char *compositeName)
{
   int32_t  scanned;
   char     factorCompositeName[NAME_LENGTH+1];
   char     factorValue[VALUE_LENGTH+1];
   char     factorType[METHOD_LENGTH+1];
   Log     *factorLog;

   scanned = sscanf(buffer, "Return Factor Event Detail for %s %s %s",
                    factorCompositeName, factorValue, factorType);

   if (scanned != 3)
   {
      ip_Socket->Send("Message <%s> could not be parsed", buffer);
      ip_Log->LogMessage("%d: Message <%s> could not be parsed", ip_Socket->GetSocketID(), buffer); 
      return false;
   }

   if (ip_Composite->GetCompositeName() != (string) factorCompositeName)
   {
      ip_Socket->Send("Factor is for a different composite.  Message ignored");
      ip_Log->LogMessage("%d: Factor Event Detail is for a different composite.  Message ignored", ip_Socket->GetSocketID()); 
      return false;
   }

   factorLog = new Log(0, "Factors.log", 0, true);
   AddFactor(factorLog, factorValue, factorType);
   delete factorLog;

   return true;
}

// This function is version agnostic.  Now that we have all of the pieces, do the database updates.
bool  WorkReceiver::ProcessWork(const char *compositeValue, const char *factorMethod,
                                int32_t factorIsNumber, double b1, int32_t curvesDone, double timeSpent)
{
   SQLStatement *deleteStatement;
   const char *deleteSQL = "delete from PendingWork " \
                           " where UserID = ? " \
                           "   and MachineID = ? " \
                           "   and InstanceID = ? " \
                           "   and CompositeName = ? " \
                           "   and FactorMethod = ? " \
                           "   and B1 = ?;";
   bool  success;

   if (!ip_Composite->WasFound())
   {
      ip_Socket->Send("ERROR: Composite <%s> not found on server", ip_Composite->GetCompositeName().c_str());
      return false;
   }

   if (!ip_Composite->SetValue(compositeValue))
   {
      ip_Socket->Send("ERROR: Value for composite <%s> does not match value found on server", ip_Composite->GetCompositeName().c_str());
      return false;
   }

   if (factorIsNumber)
   {
      ip_MasterComposite->SetActive(false);
      ip_Log->LogMessage("%d: Client detected factor as number.  Either %s is prime or B1 is too high.  The candidate has been deactivated.",
                         ip_Socket->GetSocketID(), ip_Composite->GetCompositeName().c_str());
   }

   success = ip_MasterComposite->AddWorkDone(factorMethod, b1, curvesDone);
   if (success) success = ip_MasterComposite->SendUpdatesToAllSlaves();

   ip_MasterComposite->SetSendUpdatesToMaster(true);
   ip_UserStats->IncrementTimeSpent(timeSpent);
   ip_UserStats->IncrementWorkDone(b1, curvesDone);

   if (success)
   {
      deleteStatement = new SQLStatement(ip_Log, ip_DBInterface, deleteSQL);
      deleteStatement->BindInputParameter(ip_Socket->GetUserID(), ID_LENGTH);
      deleteStatement->BindInputParameter(ip_Socket->GetMachineID(), ID_LENGTH);
      deleteStatement->BindInputParameter(ip_Socket->GetInstanceID(), ID_LENGTH);
      deleteStatement->BindInputParameter(ip_Composite->GetCompositeName(), NAME_LENGTH);
      deleteStatement->BindInputParameter(factorMethod, METHOD_LENGTH);
      deleteStatement->BindInputParameter(b1);

      success = deleteStatement->Execute();
      delete deleteStatement;
   }

   ip_Log->LogMessage("%d: Received %s: %d %s curves for B1=%.0lf from %s",
                      ip_Socket->GetSocketID(), ip_Composite->GetCompositeName().c_str(),
                      curvesDone, factorMethod, b1, ip_Socket->GetFromAddress());

   return success;
}

void  WorkReceiver::AddFactor(Log *factorLog, const char *factorValue, const char *factorType)
{
   ip_UserStats->IncrementFactorCount();

   // If a composite co-factor is factored, then don't count the first factor of that co-factor as factored
   // co-factors have already been counted as a factor themselves.
   if (ip_Composite->CompositeIsMasterComposite() || ip_CompositeFactorEvent->GetFactorCount() > 0)
      ip_MasterComposite->IncrementFactorCount();

   ip_CompositeFactorEvent->AddEventDetail(factorValue);

   ii_FactorsAccepted++;

   if (!ib_Recurse)
      ip_MasterComposite->SetActive(false);

   factorLog->LogMessage("%d: Factor %d: %s %s", ip_Socket->GetSocketID(), 
                         ip_CompositeFactorEvent->GetFactorCount(), factorType, factorValue);
}

