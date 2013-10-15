/* IAmASlave Class for ECMNet.

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

#include "IAmASlave.h"
#include "SQLStatement.h"
#include "UserStats.h"

// Constructor
IAmASlave::IAmASlave(Log *theLog, const char *masterServerName, int32_t masterPortID,
                     const char *serverID, const char *masterPassword)
{
   ip_Log = theLog;

   is_ServerID = serverID;
   is_MasterPassword = masterPassword;

   ip_Socket = new SlaveSocket(ip_Log, masterServerName, masterPortID);
}

IAmASlave::~IAmASlave(void)
{
   delete ip_Socket;
}

//****************************************************************************************************//
// Time to synchronize with the master.
//****************************************************************************************************//
void  IAmASlave::SyncWithMaster(DBInterface *dbInterface)
{
   ip_DBInterface = dbInterface;

   ip_Log->LogMessage("Attempting to synchronize with Master ECMNet Server at %s:%ld",
                      ip_Socket->GetServerName(), ip_Socket->GetPortID());

   if (!ip_Socket->Open())
      return;

   if (!ip_DBInterface->Connect(1))
   {
      ip_Socket->Close();
      return;
   }

   ip_Socket->Send("Start Sync %s %s", is_ServerID.c_str(), is_MasterPassword.c_str());

   if (SendUpdatesToMaster() && 
       GetUpdatesFromMaster() &&
       SendUserStatsToMaster() &&
       GetUserStatsFromMaster())
      ip_Log->LogMessage("Synchronization completed successfully");

   // Make sure we don't have any leftover transactions
   ip_DBInterface->Rollback();
   ip_DBInterface->Disconnect();

   ip_Socket->Close();
}

//****************************************************************************************************//
// Find all Composites which have had updates since the last time the slave synced them with
// the server and try to sync them.
//****************************************************************************************************//
bool  IAmASlave::SendUpdatesToMaster(void)
{
   SQLStatement  *selectStatement;
   const char    *selectSQL = "select MasterCompositeName, MasterCompositeValue " \
                              "  from MasterComposite " \
                              " where SendUpdatesToMaster = 1 " \
                            "order by MasterCompositeName ";
   char           masterCompositeName[NAME_LENGTH+1];
   char           masterCompositeValue[VALUE_LENGTH+1];
   int32_t        updatedCount, failedCount;
   bool           fatalError = false, success = true;

   selectStatement = new SQLStatement(ip_Log, ip_DBInterface, selectSQL);
   selectStatement->BindSelectedColumn(masterCompositeName, NAME_LENGTH);
   selectStatement->BindSelectedColumn(masterCompositeValue, VALUE_LENGTH);

   ip_Socket->Send("Start Master Composites");
   updatedCount = failedCount = 0;
   while (!fatalError && selectStatement->FetchRow(false))
   {
      if (updatedCount == 0 && failedCount == 0)
         ip_Log->LogMessage("%d: Phase 1 between slave and master started.  Sending work/factors to master.", ip_Socket->GetSocketID());

      if (SendMasterCompositeToMaster(masterCompositeName, masterCompositeValue, fatalError))
      {
         ip_DBInterface->Commit();
         updatedCount++;
      }
      else
      {
         ip_DBInterface->Rollback();
         failedCount++;
      }
   }

   selectStatement->CloseCursor();

   // Clear out last transaction
   ip_DBInterface->Rollback();

   if (fatalError)
   {      
      ip_Socket->Send("Fatal Sync Error: Terminated because a fatal error occured on the slave.  Read the slave logs to diagnose");
      ip_Log->LogMessage("Fatal Sync Error: Terminated because a fatal error occured.  Read the logs to diagnose");
      success = false;
   }

   ip_Socket->Send("No More Master Composites");

   if (failedCount > 0)
      ip_Log->LogMessage("%d: Phase 1 completed with errors.  %d of %d composites were successfully synced.",
                         ip_Socket->GetSocketID(), updatedCount, failedCount + updatedCount);
   else
      ip_Log->LogMessage("%d: Phase 1 completed.  All %d composites were successfully synced",
                         ip_Socket->GetSocketID(), updatedCount);

   delete selectStatement;

   return success;
}

//****************************************************************************************************//
// Send everything for the master composite to the master.
//****************************************************************************************************//
bool  IAmASlave::SendMasterCompositeToMaster(char *masterCompositeName, const char *masterCompositeValue, bool &fatalError)
{
   char          *theMessage;
   char           goodMessage[NAME_LENGTH * 2];
   char           badMessage[NAME_LENGTH * 2];
   char           beginMessage[NAME_LENGTH * 2];
   bool           success;

   ip_Socket->Send("Begin Master Composite %s %s %d", masterCompositeName, masterCompositeValue, strlen(masterCompositeValue));

   theMessage = ip_Socket->Receive(10);
   if (!theMessage) 
   {
      fatalError = true;
      return false;
   }

   // Build the "success" message that we are expecting
   sprintf(goodMessage, "Master Composite %s Found", masterCompositeName);
   if (strcmp(theMessage, goodMessage))
   {
      ip_Log->LogMessage(theMessage);
      return false;
   }

   // At this point we have confirmed that the master has the same MasterComposite
   // that the slave is trying to sync.

   // The constructor will not only read the MasterComposite into memory, but it will
   // also lock the MasterComposite, thus preventing updates from other clients/slaves
   // that migth be connecting to this server.
   ip_MasterComposite = new MasterComposite(ip_DBInterface, ip_Log, masterCompositeName);

   ip_MasterComposite->Lock(is_ServerID.c_str());

   ip_Socket->StartBuffering();

   // Send work that has been completed
   success = SendWorkToMaster(masterCompositeName);

   if (success) success = SendCompositesToMaster(masterCompositeName);

   // If we had a failure before this point then we cannot continue syncing.
   if (!success)
      fatalError = true;
   else
   {
      ip_Socket->Send("End Master Composite %s", masterCompositeName);
      ip_Socket->SendBuffer();

      // Build the messages that we are expecting
      sprintf(goodMessage, "Master Composite %s OK", masterCompositeName);
      sprintf(badMessage, "Master Composite %s Error", masterCompositeName);
      sprintf(beginMessage, "Begin Master Composite %s", masterCompositeName);

      do
      {
         theMessage = ip_Socket->Receive(10);

         // If no message, then we must have a socket problem, which is fatal
         if (!theMessage)
         {
            fatalError = true;
            success = false;
            break;
         }

         // Now that we have sent updates to the master for this MasterComposite, it is 
         // time to get updates for this MasterComposite from the master before returning.
         // If this is successful, then we will commit all changes to the MasterComposite
         // as the sync will have been successful.
         if (!memcmp(beginMessage, theMessage, strlen(beginMessage)))
         {
            success = HandleMasterCompositeFromMaster(theMessage, masterCompositeName, fatalError);
            if (success) ip_Socket->Send("Master Composite %s OK", masterCompositeName);
            continue;
         }

         // The "bad" message indicates an error occurred on the server
         // and thus the updates were not committed
         if (!strcmp(theMessage, badMessage))
         {
            success = false;
            break;
         }

         // The "good" message indicates that all updates on the server were
         // accepted and committed.
         if (!strcmp(theMessage, goodMessage))
         {
            success = true;
            break;
         }

         // Any other messages should just be logged.  They don't indicate
         // success or failure for the sync.
         ip_Log->LogMessage(theMessage);
      } while (!fatalError);
   }

   if (success)
   {
      ip_MasterComposite->SetSendUpdatesToMaster(false);
      ip_MasterComposite->FinalizeChanges();
   }

   delete ip_MasterComposite;
   ip_MasterComposite = NULL;

   return success;
}

//****************************************************************************************************//
// Find any work that hasn't been reported to the master and report it.  Set CurvesSinceLastReport in this
// transaction in case the socket is closed before getting updates from the master in phase 2.
//****************************************************************************************************//
bool  IAmASlave::SendWorkToMaster(const char *masterCompositeName)
{
   SQLStatement  *selectWorkStatement;
   SQLStatement  *updateWorkStatement;
   const char    *selectWork = "select FactorMethod, B1, CurvesSinceLastReport " \
                               "  from MasterCompositeWorkDone " \
                               " where MasterCompositeName = ? " \
                               "   and CurvesSinceLastReport > 0 ";
   const char    *updateWork = "update MasterCompositeWorkDone " \
                               "   set CurvesSinceLastReport = 0 " \
                               " where MasterCompositeName = ? " \
                               "   and FactorMethod = ? " \
                               "   and B1 = ? ";
   bool           success;
   char           factorMethod[20];
   double         b1;
   int64_t        curvesToReport;

   selectWorkStatement = new SQLStatement(ip_Log, ip_DBInterface, selectWork);
   selectWorkStatement->BindInputParameter(masterCompositeName, NAME_LENGTH);
   selectWorkStatement->BindSelectedColumn(factorMethod, sizeof(factorMethod));
   selectWorkStatement->BindSelectedColumn(&b1);
   selectWorkStatement->BindSelectedColumn(&curvesToReport);

   updateWorkStatement = new SQLStatement(ip_Log, ip_DBInterface, updateWork);
   updateWorkStatement->BindInputParameter(masterCompositeName, NAME_LENGTH);
   updateWorkStatement->BindInputParameter(factorMethod, sizeof(factorMethod), false);
   updateWorkStatement->BindInputParameter(b1);

   success = true;

   while (success && selectWorkStatement->FetchRow(false))
   {
      ip_Socket->Send("Work for %s %s %.0lf %"PRId64"", masterCompositeName, factorMethod, b1, curvesToReport);

      updateWorkStatement->SetInputParameterValue(masterCompositeName, true);
      updateWorkStatement->SetInputParameterValue(factorMethod);
      updateWorkStatement->SetInputParameterValue(b1);

      success = updateWorkStatement->Execute();
   }

   selectWorkStatement->CloseCursor();

   delete selectWorkStatement;
   delete updateWorkStatement;
   return success;
}

//****************************************************************************************************//
// Send all composites for this master composite to the slave.
//****************************************************************************************************//
bool  IAmASlave::SendCompositesToMaster(const char *masterCompositeName)
{
   SQLStatement  *selectStatement;
   const char    *selectSQL = "select CompositeName, CompositeValue, DecimalLength " \
                              "  from Composite " \
                              " where MasterCompositeName = ? ";
   char           compositeName[METHOD_LENGTH+1];
   char           compositeValue[VALUE_LENGTH+1];
   int64_t        decimalLength;
   bool           success = true;

   selectStatement = new SQLStatement(ip_Log, ip_DBInterface, selectSQL);
   selectStatement->BindInputParameter(masterCompositeName, NAME_LENGTH);
   selectStatement->BindSelectedColumn(compositeName, METHOD_LENGTH);
   selectStatement->BindSelectedColumn(compositeValue, VALUE_LENGTH);
   selectStatement->BindSelectedColumn(&decimalLength);

   while (selectStatement->FetchRow(false))
   {
      ip_Socket->Send("Composite for %s %s %s %d", masterCompositeName, compositeName, compositeValue, decimalLength);

      if (success) success = SendCompositeFactorEventsToMaster(masterCompositeName, compositeName);
   }

   selectStatement->CloseCursor();

   delete selectStatement;

   return success;
}

//****************************************************************************************************//
// Send all factors for this composite to the slave.
//****************************************************************************************************//
bool  IAmASlave::SendCompositeFactorEventsToMaster(const char *masterCompositeName, const char *compositeName)
{
   SQLStatement  *selectStatement;
   SQLStatement  *updateStatement;
   const char    *selectSQL = "select cfe.FactorEvent, cfe.ReportedDate, " \
                              "       cfe.EmailID, cfe.UserID, cfe.MachineID, cfe.ClientID, " \
                              "       cfe.ServerID, cfe.FactorMethod, cfe.FactorStep, " \
                              "       cfe.B1, cfe.B1Time, cfe.B2, cfe.B2Time, cfe.Sigma, cfe.X0 " \
                              "  from CompositeFactorEvent cfe, Composite " \
                              " where cfe.CompositeName = Composite.CompositeName " \
                              "   and Composite.MasterCompositeName = ? " \
                              "   and Composite.CompositeName = ? " \
                              "   and cfe.SentToMaster = 0 ";
   const char    *updateSQL = "update CompositeFactorEvent " \
                              "   set SentToMaster = 1 " \
                              " where CompositeName = ? " \
                              "   and FactorEvent = ? ";
   char           emailID[ID_LENGTH+1];
   char           userID[ID_LENGTH+1];
   char           machineID[ID_LENGTH+1];
   char           instanceID[ID_LENGTH+1];
   char           serverID[ID_LENGTH+1];
   char           factorMethod[METHOD_LENGTH+1];
   int32_t        factorEvent, factorStep;
   double         b1, b1Time, b2, b2Time;
   int64_t        sigma, x0, reportedDate;
   bool           success = true;

   selectStatement = new SQLStatement(ip_Log, ip_DBInterface, selectSQL);
   selectStatement->BindInputParameter(masterCompositeName, NAME_LENGTH);
   selectStatement->BindInputParameter(compositeName, NAME_LENGTH);
   selectStatement->BindSelectedColumn(&factorEvent);
   selectStatement->BindSelectedColumn(&reportedDate);
   selectStatement->BindSelectedColumn(emailID, ID_LENGTH);
   selectStatement->BindSelectedColumn(userID, ID_LENGTH);
   selectStatement->BindSelectedColumn(machineID, ID_LENGTH);
   selectStatement->BindSelectedColumn(instanceID, ID_LENGTH);
   selectStatement->BindSelectedColumn(serverID, ID_LENGTH);
   selectStatement->BindSelectedColumn(factorMethod, METHOD_LENGTH);
   selectStatement->BindSelectedColumn(&factorStep);
   selectStatement->BindSelectedColumn(&b1);
   selectStatement->BindSelectedColumn(&b1Time);
   selectStatement->BindSelectedColumn(&b2);
   selectStatement->BindSelectedColumn(&b2Time);
   selectStatement->BindSelectedColumn(&sigma);
   selectStatement->BindSelectedColumn(&x0);

   while (selectStatement->FetchRow(false))
   {
      ip_Socket->Send("CFE for %s %s %"PRId64" %s %s %s %s %s %s %d %lf %lf %lf %lf %"PRId64" %"PRId64"",
                      masterCompositeName, compositeName, reportedDate,
                      emailID, userID, machineID, instanceID, serverID,
                      factorMethod, factorStep,
                      b1, b1Time, b2, b2Time, sigma, x0);

      if (success)
      {
         updateStatement = new SQLStatement(ip_Log, ip_DBInterface, updateSQL);
         updateStatement->BindInputParameter(compositeName, NAME_LENGTH);
         updateStatement->BindInputParameter(factorEvent);

         success = updateStatement->Execute();
         delete updateStatement;
      }

      SendCompositeFactorEventDetailsToMaster(masterCompositeName, compositeName, factorEvent);
   }

   selectStatement->CloseCursor();

   delete selectStatement;

   return success;
}

//****************************************************************************************************//
// Send all factors for this composite to the slave.
//****************************************************************************************************//
void  IAmASlave::SendCompositeFactorEventDetailsToMaster(const char *masterCompositeName, const char *compositeName, int32_t factorEvent)
{
   SQLStatement  *selectStatement;
   const char    *selectSQL = "select cfed.FactorValue, cfed.DecimalLength, cfed.PRPTestResult " \
                              "  from CompositeFactorEventDetail cfed, Composite " \
                              " where cfed.CompositeName = Composite.CompositeName " \
                              "   and Composite.MasterCompositeName = ? " \
                              "   and Composite.CompositeName = ? " \
                              "   and cfed.FactorEvent = ? ";
   char           factorValue[VALUE_LENGTH+1];
   int32_t        prpTestResult, decimalLength;

   selectStatement = new SQLStatement(ip_Log, ip_DBInterface, selectSQL);
   selectStatement->BindInputParameter(masterCompositeName, NAME_LENGTH);
   selectStatement->BindInputParameter(compositeName, NAME_LENGTH);
   selectStatement->BindInputParameter(factorEvent);
   selectStatement->BindSelectedColumn(factorValue, VALUE_LENGTH);
   selectStatement->BindSelectedColumn(&decimalLength);
   selectStatement->BindSelectedColumn(&prpTestResult);

   while (selectStatement->FetchRow(false))
   {
      ip_Socket->Send("CFE Detail for %s %s %s %d %d",
                      masterCompositeName, compositeName, factorValue, decimalLength, prpTestResult);
   }

   selectStatement->CloseCursor();

   delete selectStatement;
}

//****************************************************************************************************//
// Get all updates from the master
//****************************************************************************************************//
bool  IAmASlave::GetUpdatesFromMaster(void)
{
   char    *theMessage;
   bool     success, fatalError;
   int32_t  updatedCount, failedCount;
   char     masterCompositeName[NAME_LENGTH+1];

   theMessage = ip_Socket->Receive();

   if (theMessage && strcmp(theMessage, "Start Master Composites"))
      return false;

   ip_Log->LogMessage("%d: Phase 2 between slave and master started.  Getting work/factors from master.", ip_Socket->GetSocketID());

   ip_MasterComposite = NULL;
   updatedCount = failedCount = 0;
   fatalError = false;
   do
   {
      theMessage = ip_Socket->Receive(10);
      if (!theMessage)
      {
         fatalError = true;
         break;
      }

      if (!strcmp(theMessage, "No More Master Composites"))
         break;

      if (!memcmp(theMessage, "Begin Master Composite ", 23))
      {
         // Clear any existing transaction
         ip_DBInterface->Rollback();

         success = HandleMasterCompositeFromMaster(theMessage, masterCompositeName, fatalError);

         if (success)
         {
            ip_DBInterface->Commit();
            updatedCount++;
            ip_Socket->Send("Master Composite %s OK", masterCompositeName);
         }
         else
         {
            ip_DBInterface->Rollback();
            failedCount++;
            ip_Socket->Send("Master Composite %s Error", masterCompositeName);
         }

         if (ip_MasterComposite)
         {
            delete ip_MasterComposite;
            ip_MasterComposite = NULL;
         }
      }
      else
      {
         ip_Socket->Send("Sync Error: Expected 'Begin Master Composite', but got <%s>", theMessage);
         ip_Log->LogMessage("%d: Expected 'Begin Master Composite', but got <%s>", ip_Socket->GetSocketID(), theMessage);
      }
   } while (!fatalError);

   success = true;
   if (fatalError)
   {      
      ip_Socket->Send("Fatal Sync Error: Terminated because a fatal error occured on the slave.  Read the slave logs to diagnose");
      ip_Log->LogMessage("Fatal Sync Error: Terminated because a fatal error occured.  Read the logs to diagnose");
      success = false;
   }

   if (failedCount > 0)
      ip_Log->LogMessage("%d: Phase 2 completed.  %d composites were successfully synced.  %d were not synced",
                         ip_Socket->GetSocketID(), updatedCount, failedCount);
   else
      ip_Log->LogMessage("%d: Phase 2 completed.  All %d composites were successfully synced. ",
                         ip_Socket->GetSocketID(), updatedCount);

   return success;
}

//****************************************************************************************************//
// The slave is sending another candidate.  This routine will only return false if there
// is a socket or database error.
//****************************************************************************************************//
bool  IAmASlave::HandleMasterCompositeFromMaster(const char *startMessage, char *masterCompositeName, bool &fatalError)
{
   int32_t  scanned, isActive;
   int32_t  masterCompositeLength;
   bool     success;
   char     masterCompositeValue[VALUE_LENGTH];
   char    *theMessage;
   char     endMessage[NAME_LENGTH * 2];
   char     workMessage[NAME_LENGTH * 2];
   char     compositeMessage[NAME_LENGTH * 2];
   char     cfeMessage[NAME_LENGTH * 2];
   char     cfedMessage[NAME_LENGTH * 2];

   success = true;
   fatalError = false;

   scanned = sscanf(startMessage, "Begin Master Composite %s %s %d %d", masterCompositeName, masterCompositeValue, &isActive, &masterCompositeLength);
   if (scanned != 4)
   {
      ip_Socket->Send("Sync Error: Could not parse <%s> from master", startMessage);
      ip_Log->LogMessage("%d: Could not parse <%s> from master", ip_Socket->GetSocketID(), startMessage);
      return false;
   }
   if (masterCompositeLength != (int32_t) strlen(masterCompositeValue))
   {
      ip_Socket->Send("Sync Error: Composite length doesn't match, expected %d, got %d", masterCompositeLength, strlen(masterCompositeValue));
      ip_Log->LogMessage("%d: Composite length doesn't match, expected %d, got %d", ip_Socket->GetSocketID(), masterCompositeLength, strlen(masterCompositeValue));
      return false;
   }

   if (!ip_MasterComposite)
   {
      ip_MasterComposite = new MasterComposite(ip_DBInterface, ip_Log, masterCompositeName);
      ip_MasterComposite->Lock(is_ServerID.c_str());
   }

   if (!ip_MasterComposite->SetValue(masterCompositeValue))
   {
      ip_Socket->Send("Sync Error: Value for Master Composite %s did not match", masterCompositeName);
      return false;
   }

   // If the MasterComposite doesn't exist in the slave, then insert it
   // into the database so that we can create child records.
   if (!ip_MasterComposite->WasFound())
      if (!ip_MasterComposite->FinalizeChanges())
         return false;

   ip_MasterComposite->SetActive((isActive > 0) ? true : false);

   if (!DeleteMasterCompositeDetails())
      return false;

   ip_MasterComposite->ResetFactorCount();

   ip_Socket->Send("Master Composite %s Found", masterCompositeName);

   sprintf(endMessage, "End Master Composite %s", masterCompositeName);
   sprintf(workMessage, "Work for %s", masterCompositeName);
   sprintf(compositeMessage, "Composite for %s", masterCompositeName);
   sprintf(cfeMessage, "CFE for %s", masterCompositeName);
   sprintf(cfedMessage, "CFE Detail for %s", masterCompositeName);

   ip_Composite = NULL;
   ip_CompositeFactorEvent = NULL;
   do
   {
      theMessage = ip_Socket->Receive(10);
      if (!theMessage)
      {
         success = false;
         fatalError = true;
         break;
      }

      if (!strcmp(theMessage, endMessage))
         break;

      // Stop processing once a failure has occurred
      if (success)
      {
         if (!memcmp(theMessage, workMessage, strlen(workMessage)))
            success = HandleWorkFromMaster(theMessage);
         else if (!memcmp(theMessage, compositeMessage, strlen(compositeMessage)))
         {
            // We have a new composite for this master, apply the database updates from the previous composite
            if (ip_Composite)
            {
               success = ip_Composite->FinalizeChanges();
               delete ip_Composite;
               ip_Composite = NULL;
            }

            success = HandleCompositeFromMaster(theMessage);
			}
         else if (!memcmp(theMessage, cfeMessage, strlen(cfeMessage)))
         {
            // We have a new composite factor event for this composite, apply the database updates from the previous event
            if (ip_CompositeFactorEvent)
            {
					success = ip_CompositeFactorEvent->FinalizeChanges();
					delete ip_CompositeFactorEvent;
               ip_CompositeFactorEvent = NULL;
            }

            if (success) success = HandleCompositeFactorEventFromMaster(theMessage);
         }
         else if (!memcmp(theMessage, cfedMessage, strlen(cfedMessage)))
            success = HandleCompositeFactorEventDetailFromMaster(theMessage);
         else
         {
            ip_Socket->Send("Sync Error: Message <%s> was not expected", theMessage);
            success = false;
         }
      }
   } while (!fatalError);

   if (success)
   {
      ip_MasterComposite->SetLastUpdateTime();

      if (ip_CompositeFactorEvent) success = ip_CompositeFactorEvent->FinalizeChanges();
      if (success && ip_Composite) success = ip_Composite->FinalizeChanges();
      if (success) success = ip_MasterComposite->SendUpdatesToAllSlaves();
      if (success) success = ip_MasterComposite->FinalizeChanges();
   }

   if (ip_CompositeFactorEvent) { delete ip_CompositeFactorEvent; ip_CompositeFactorEvent = NULL; }
   if (ip_Composite) { delete ip_Composite; ip_Composite = NULL; }

   return success;
}

//****************************************************************************************************//
// Delete all detail information for this master composite as syncing will re-create it.  This 
// may seem a little heavy handed, but since the master has to send everything to the slave anyways,
// this will guarantee that the slave has up-to-date information.  Also note tht since there is a
// resource lock on the master composite that no work can be returned for the master composite
// while it is being synced with the master server.
//****************************************************************************************************//
bool	IAmASlave::DeleteMasterCompositeDetails(void)
{
   SQLStatement  *deleteStatement1, *deleteStatement2, *deleteStatement3, *deleteStatement4;
   SQLStatement  *selectStatement;
   const char    *selectSQL = "select CompositeName " \
                              "  from Composite " \
                              " where MasterCompositeName = ? ";
   const char *deleteSQL1 = "delete from CompositeFactorEventDetail where CompositeName = ? ";
   const char *deleteSQL2 = "delete from CompositeFactorEvent where CompositeName = ? ";
   const char *deleteSQL3 = "delete from Composite where MasterCompositeName = ? ";
   const char *deleteSQL4 = "delete from MasterCompositeWorkDone where MasterCompositeName = ? ";
   char  compositeName[NAME_LENGTH+1];
   bool	success = true;

   deleteStatement1 = new SQLStatement(ip_Log, ip_DBInterface, deleteSQL1);
   deleteStatement2 = new SQLStatement(ip_Log, ip_DBInterface, deleteSQL2);
   deleteStatement3 = new SQLStatement(ip_Log, ip_DBInterface, deleteSQL3);

   deleteStatement1->BindInputParameter(compositeName, NAME_LENGTH, false);
   deleteStatement2->BindInputParameter(compositeName, NAME_LENGTH, false);
   deleteStatement3->BindInputParameter(compositeName, NAME_LENGTH, false);

   selectStatement = new SQLStatement(ip_Log, ip_DBInterface, selectSQL);
   selectStatement->BindInputParameter(ip_MasterComposite->GetName(), NAME_LENGTH);
   selectStatement->BindSelectedColumn(compositeName, NAME_LENGTH);

   while (success && selectStatement->FetchRow(false))
   {
      deleteStatement1->SetInputParameterValue(compositeName, true);
      deleteStatement2->SetInputParameterValue(compositeName, true);
      deleteStatement3->SetInputParameterValue(compositeName, true);

      success = deleteStatement1->Execute();
      if (success) success = deleteStatement2->Execute();
      if (success) success = deleteStatement3->Execute();
   }
 
   if (success)
   {
      deleteStatement4 = new SQLStatement(ip_Log, ip_DBInterface, deleteSQL4);
      deleteStatement4->BindInputParameter(ip_MasterComposite->GetName(), NAME_LENGTH);
      success = deleteStatement4->Execute();
      delete deleteStatement4;
   }

   delete selectStatement;
   delete deleteStatement1;
   delete deleteStatement2;
   delete deleteStatement3;

   return success;
}

//****************************************************************************************************//
// Process work found by the master.
//****************************************************************************************************//
bool  IAmASlave::HandleWorkFromMaster(const char *theMessage)
{
   double   b1;
   char     masterCompositeName[NAME_LENGTH+1];
   char     factorMethod[METHOD_LENGTH+1];
   int32_t  masterCurves;

   if (sscanf(theMessage, "Work for %s %s %lf %d", masterCompositeName, factorMethod, &b1, &masterCurves) != 4)
   {
      ip_Socket->Send("Sync Error: Could not parse <%s>", theMessage);
      ip_Log->LogMessage("%d: Could parse <%s>", ip_Socket->GetSocketID(), theMessage);
      return false;
   }

   return ip_MasterComposite->InsertWorkDoneFromMaster(factorMethod, b1, masterCurves);
}

//****************************************************************************************************//
// Process a composite being factored by the master.
//****************************************************************************************************//
bool  IAmASlave::HandleCompositeFromMaster(const char *theMessage)
{
   char     masterCompositeName[NAME_LENGTH+1];
   char     compositeName[NAME_LENGTH+1];
   char     compositeValue[VALUE_LENGTH+1];
   int32_t  decimalLength;

	if (sscanf(theMessage, "Composite for %s %s %s %d", masterCompositeName, compositeName, compositeValue, &decimalLength) != 4)
   {
      ip_Socket->Send("Sync Error: Could not parse <%s>", theMessage);
      ip_Log->LogMessage("%d: Could not parse <%s>", ip_Socket->GetSocketID(), theMessage);
      return false;
   }

   ip_Composite = new Composite(ip_DBInterface, ip_Log, compositeName, true);

   if (!ip_Composite->SetValue(compositeValue))
   {
      ip_Socket->Send("Sync Error: Composite value for %s mismatch", compositeName);
      ip_Log->LogMessage("%d: Composite value for %s mismatch", ip_Socket->GetSocketID(), compositeName);
      return false;
   }

   if (!ip_Composite->SetMasterCompositeName(masterCompositeName))
   {
      ip_Socket->Send("Sync Error: Composite name for %s mismatch", compositeName);
      ip_Log->LogMessage("%d: Composite name for %s mismatch", ip_Socket->GetSocketID(), compositeName);
      return false;
   }

   // If the composite didn't exist, this will insert it.
   if (!ip_Composite->WasFound())
      ip_Composite->FinalizeChanges();

   return true;
}

//****************************************************************************************************//
// Process factors found by the master.
//****************************************************************************************************//
bool  IAmASlave::HandleCompositeFactorEventFromMaster(const char *theMessage)
{
   char     masterCompositeName[NAME_LENGTH+1];
   char     compositeName[NAME_LENGTH+1];
   char     emailID[ID_LENGTH+1];
   char     userID[ID_LENGTH+1];
   char     machineID[ID_LENGTH+1];
   char     instanceID[ID_LENGTH+1];
   char     serverID[ID_LENGTH+1];
   char     factorMethod[METHOD_LENGTH+1];
   double   b1, b1Time, b2, b2Time;
   int64_t  sigma, x0, reportedDate;
   int32_t  factorStep;

   if (sscanf(theMessage, "CFE for %s %s %"PRId64" %s %s %s %s %s %s %d %lf %lf %lf %lf %"PRId64" %"PRId64"",
              masterCompositeName, compositeName, &reportedDate,
              emailID, userID, machineID, instanceID, serverID,
              factorMethod, &factorStep,
              &b1, &b1Time, &b2, &b2Time, &sigma, &x0) != 16)
   {
      ip_Socket->Send("Sync Error: Could not parse <%s>", theMessage);
      ip_Log->LogMessage("%d: Could not parse <%s>", ip_Socket->GetSocketID(), theMessage);
      return false;
   }

   if (strcmp(compositeName, ip_Composite->GetCompositeName().c_str()))
   {
      ip_Socket->Send("Sync Error: Composite name did not match", theMessage);
      ip_Log->LogMessage("%d: Composite name did not match", ip_Socket->GetSocketID(), theMessage);
      return false;
   }

   ip_CompositeFactorEvent = new CompositeFactorEvent(ip_DBInterface, ip_Log, compositeName,
                                                      ip_Composite->NextFactorEvent(), reportedDate);
   ip_CompositeFactorEvent->SetIDs(emailID, userID, machineID, instanceID, serverID);
   ip_CompositeFactorEvent->SetCurveDetail(factorMethod, factorStep, b1, b1Time, b2, b2Time, sigma, x0);
   ip_CompositeFactorEvent->SetSentToMaster(true);

   return true;
}

//****************************************************************************************************//
// Process factors found by the master.
//****************************************************************************************************//
bool  IAmASlave::HandleCompositeFactorEventDetailFromMaster(const char *theMessage)
{
   char     masterCompositeName[NAME_LENGTH+1];
   char     compositeName[NAME_LENGTH+1];
   char     factorValue[VALUE_LENGTH+1];
   int32_t  decimalLength, prpTestResult;
   bool     success = true;

   if (sscanf(theMessage, "CFE Detail for %s %s %s %d %d",
              masterCompositeName, compositeName, factorValue, &decimalLength, &prpTestResult) != 5)
   {
      ip_Socket->Send("Sync Error: Could not parse <%s>", theMessage);
      ip_Log->LogMessage("%d: Could not parse <%s>", ip_Socket->GetSocketID(), theMessage);
      return false;
   }

   if (strcmp(compositeName, ip_Composite->GetCompositeName().c_str()))
   {
      ip_Socket->Send("Sync Error: Composite name did not match", theMessage);
      ip_Log->LogMessage("%d: Composite name did not match", ip_Socket->GetSocketID(), theMessage);
      return false;
   }

   ip_CompositeFactorEvent->AddEventDetail(factorValue, decimalLength, prpTestResult);
   ip_MasterComposite->IncrementFactorCount();

   return success;
}

//****************************************************************************************************//
bool  IAmASlave::SendUserStatsToMaster(void)
{
   SQLStatement  *selectStatement;
   SQLStatement  *updateStatement;
   const char    *selectSQL = "select UserID, WorkSinceLastReport, FactorsSinceLastReport, TimeSinceLastReport " \
                              "  from UserStats " \
                              " where WorkSinceLastReport > 0 " \
                              "order by UserID ";
   const char    *updateSQL = "update UserStats " \
                              "   set WorkSinceLastReport = 0, " \
                              "       FactorsSinceLastReport = 0, " \
                              "       TimeSinceLastReport = 0 " \
                              " where UserID = ? ";
   char          *theMessage;
   char           userID[ID_LENGTH+1];
   double         workToSend;
   int32_t        factorsToSend;
   double         timeSpent;
   bool           success;

   selectStatement = new SQLStatement(ip_Log, ip_DBInterface, selectSQL);
   selectStatement->BindSelectedColumn(userID, ID_LENGTH);
   selectStatement->BindSelectedColumn(&workToSend);
   selectStatement->BindSelectedColumn(&factorsToSend);
   selectStatement->BindSelectedColumn(&timeSpent);

   updateStatement = new SQLStatement(ip_Log, ip_DBInterface, updateSQL);
   updateStatement->BindInputParameter(userID, ID_LENGTH, false);

   ip_Socket->Send("Start User Stats");
   success = true;
   while (success && selectStatement->FetchRow(false))
   {
      ip_Socket->Send("User Stats %s %.0lf %d %lf", userID, workToSend, factorsToSend, timeSpent);

      updateStatement->SetInputParameterValue(userID, true);

      success = updateStatement->Execute();
   }

   selectStatement->CloseCursor();
   ip_Socket->Send("End User Stats");

   theMessage = ip_Socket->Receive(2);
   if (!theMessage) success = false;
   if (success && strcmp(theMessage, "User Stats Synced"))
      success = false;

   // Clear out last transaction
   if (!success)
      ip_DBInterface->Rollback();
   else
      ip_DBInterface->Commit();

   delete selectStatement;
   delete updateStatement;

   return success;
}

//****************************************************************************************************//
bool  IAmASlave::GetUserStatsFromMaster(void)
{
   char    *theMessage;
   char     userID[ID_LENGTH+1];
   double   workDone;
   int32_t  factorCount;
   double   timeSpent;
   bool     success = true;
   UserStats *userStats;

   theMessage = ip_Socket->Receive(2);
   if (theMessage && !strcmp(theMessage, "Start User Stats"))
   {
      theMessage = ip_Socket->Receive(2);
      while (success && theMessage && strcmp(theMessage, "End User Stats"))
      {
         if (sscanf(theMessage, "User Stats %s %lf %d %lf", userID, &workDone, &factorCount, &timeSpent) != 4)
         {
            success = false;
            ip_Socket->Send("Error parsing <%s>", theMessage);
            break;
         }

         userStats = new UserStats(ip_DBInterface, ip_Log, userID);
         userStats->SetTotalWorkDone(workDone);
         userStats->SetTotalFactorCount(factorCount);
         userStats->SetTotalTimeSpent(timeSpent);
         success = userStats->FinalizeChanges();
         delete userStats;

         theMessage = ip_Socket->Receive(2);
      }
   }

   if (success)
   {
      ip_DBInterface->Commit();
      ip_Socket->Send("User Stats Synced");
   }

   ip_Log->LogMessage("%d: Phase 3 completed.  User stats have been synced", ip_Socket->GetSocketID());

   return success;
}
