/* IAmAMaster Class for ECMNet.

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

#include "IAmAMaster.h"
#include "SQLStatement.h"
#include "UserStats.h"

IAmAMaster::IAmAMaster(DBInterface *dbInterface, HelperSocket *theSocket, Log *theLog,
                       const char *serverID, const char *expectedPassword, bool iAmASlave)
{
   ip_DBInterface = dbInterface;
   ip_Socket = theSocket;
   ip_Log = theLog;
   ib_IAmASlave = iAmASlave;

   strcpy(ic_ServerID, serverID);
   strcpy(ic_ExpectedPassword, expectedPassword);
}

IAmAMaster::~IAmAMaster()
{
}

//****************************************************************************************************//
// Time to synchronize with a slave.
//****************************************************************************************************//
void  IAmAMaster::SyncWithSlave(const char *theMessage)
{
   char  slaveID[ID_LENGTH+1];
   char  password[ID_LENGTH+1];

   if (sscanf(theMessage, "Start Sync %s %s", slaveID, password) != 2)
   {
      ip_Socket->Send("Sync Error: START SYNC not formatted correctly.");
      ip_Log->LogMessage("%d: START SYNC not formatted correctly", ip_Socket->GetSocketID());
      return;
   }

   if (strcmp(password, ic_ExpectedPassword))
   {
      ip_Socket->Send("Sync Error: unable to sync because the wrong password sent.");
      ip_Log->LogMessage("%d: unable to sync because the wrong password sent", ip_Socket->GetSocketID());
      return;
   }

   if (PopulateMasterSlaveSync(slaveID) &&
       GetUpdatesFromSlave(slaveID) &&
       SendUpdatesToSlave(slaveID) &&
       GetUserStatsFromSlave() &&
       SendUserStatsToSlave())
      ip_Log->LogMessage("%d: Synchronization completed successfully", ip_Socket->GetSocketID());

   // Make sure we don't have any leftover transactions
   ip_DBInterface->Rollback();
   ip_DBInterface->Disconnect();

   ip_Socket->Close();
}

//****************************************************************************************************//
// Populate rows into the MasterSlaveSync table.
//****************************************************************************************************//
bool  IAmAMaster::PopulateMasterSlaveSync(const char *slaveID)
{
   SQLStatement  *selectStatement1;
   SQLStatement  *selectStatement2;
   SQLStatement  *insertStatement;
   const char    *selectSQL1 = "select MasterCompositeName from MasterComposite ";
   const char    *selectSQL2 = "select count(*) from MasterCompositeSlaveSync " \
                               " where MasterCompositeName = ? and SlaveID = ? ";
   const char    *insertSQL  = "insert into MasterCompositeSlaveSync " \
                               "(MasterCompositeName, SlaveID, SendUpdatesToSlave) values ( ?,?,1 ) ";
   char     masterCompositeName[NAME_LENGTH+1];
   int32_t  rows;
   bool	   success = true;

   selectStatement1 = new SQLStatement(ip_Log, ip_DBInterface, selectSQL1);
   selectStatement1->BindSelectedColumn(masterCompositeName, NAME_LENGTH);

   selectStatement2 = new SQLStatement(ip_Log, ip_DBInterface, selectSQL2);
   selectStatement2->BindSelectedColumn(&rows);
   selectStatement2->BindInputParameter(masterCompositeName, NAME_LENGTH, false);
   selectStatement2->BindInputParameter(slaveID, ID_LENGTH);

   insertStatement = new SQLStatement(ip_Log, ip_DBInterface, insertSQL);
   insertStatement->BindInputParameter(masterCompositeName, NAME_LENGTH, false);
   insertStatement->BindInputParameter(slaveID, ID_LENGTH);

   success = true;
   while (success && selectStatement1->FetchRow(false))
   {
      selectStatement2->SetInputParameterValue(masterCompositeName, true);
      selectStatement2->SetInputParameterValue(slaveID);
      success = selectStatement2->FetchRow(true);

      if (success && rows == 0)
      {
         insertStatement->SetInputParameterValue(masterCompositeName, true);
         insertStatement->SetInputParameterValue(slaveID);
         success = insertStatement->Execute();
      }
   }

   delete selectStatement1;
   delete selectStatement2;
   delete insertStatement;

   return success;
}

//****************************************************************************************************//
// Get curve/factor updates from a slave.
//****************************************************************************************************//
bool  IAmAMaster::GetUpdatesFromSlave(const char *slaveID)
{
   char    *theMessage;
   bool     success, fatalError;
   int32_t  updatedCount, failedCount;
   char     masterCompositeName[NAME_LENGTH+1];

   theMessage = ip_Socket->Receive();

   if (theMessage && strcmp(theMessage, "Start Master Composites"))
      return false;

   if (!strcmp(theMessage, "No More Master Composites"))
   {
      ip_Log->LogMessage("%d: No work for Phase 1 as slave %s has no master composites.",
                         ip_Socket->GetSocketID(), slaveID);
      return true;
   }

   ip_Log->LogMessage("%d: Phase 1 between master and slave %s started.  Getting work/factors from slave.",
                      ip_Socket->GetSocketID(), slaveID);

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
         success = StartMasterCompositeSync(theMessage, masterCompositeName);

         if (success) success = SyncMasterCompositeFromSlave(masterCompositeName, slaveID, fatalError);

         if (success) success = SendMasterCompositeToSlave(masterCompositeName, slaveID, fatalError);

         success = EndMasterCompositeSync(success, slaveID);

         if (success)
         {
            updatedCount++;
            ip_Socket->Send("Master Composite %s OK", masterCompositeName);
         }
         else
         {
            failedCount++;
            ip_Socket->Send("Master Composite %s Error", masterCompositeName);
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
      ip_Socket->Send("Fatal Sync Error: Terminated because a fatal error occured on the master.  Read the master logs to diagnose");
      ip_Log->LogMessage("Fatal Sync Error: Terminated because a fatal error occured.  Read the logs to diagnose");
      success = false;
   }

   if (failedCount > 0)
      ip_Log->LogMessage("%d: Phase 1 completed.  %d composites were successfully synced.  %d were not synced",
                         ip_Socket->GetSocketID(), updatedCount, failedCount);
   else
      ip_Log->LogMessage("%d: Phase 1 completed.  All %d composites were successfully synced. ",
                         ip_Socket->GetSocketID(), updatedCount);

   return success;
}

//****************************************************************************************************//
// The slave is sending another candidate.  Validate what is being sent and if we can proceed, then
// lock the MasterComposite to prevent updates from other clients/slaves.
//****************************************************************************************************//
bool  IAmAMaster::StartMasterCompositeSync(const char *theMessage, char *masterCompositeName)
{
   int32_t  scanned;
   int32_t  masterCompositeLength;
   char     masterCompositeValue[VALUE_LENGTH+1];

   // Clear any existing transaction
   ip_DBInterface->Rollback();

   scanned = sscanf(theMessage, "Begin Master Composite %s %s %d", masterCompositeName, masterCompositeValue, &masterCompositeLength);
   if (scanned != 3)
   {
      ip_Socket->Send("Sync Error: Could not parse <%s> from slave", theMessage);
      ip_Log->LogMessage("%d: Could not parse <%s> from slave", ip_Socket->GetSocketID(), theMessage);
      return false;
   }

   if (masterCompositeLength != (int32_t) strlen(masterCompositeValue))
   {
      ip_Socket->Send("Sync Error: Composite length doesn't match, expected %d, got %d", masterCompositeLength, strlen(masterCompositeValue));
      ip_Log->LogMessage("%d: Composite length doesn't match, expected %d, got %d", ip_Socket->GetSocketID(), masterCompositeLength, strlen(masterCompositeValue));
      return false;
   }

   ip_MasterComposite = new MasterComposite(ip_DBInterface, ip_Log, masterCompositeName);

   if (!ip_MasterComposite->WasFound())
   {
      ip_Socket->Send("Sync Error: Master Composite %s was not found", masterCompositeName);
      ip_Log->LogMessage("%d: Master Composite %s was not found", ip_Socket->GetSocketID(), masterCompositeName);
      delete ip_MasterComposite;
      ip_MasterComposite = NULL;
      return false;
   }

   if (!ip_MasterComposite->SetValue(masterCompositeValue))
   {
      ip_Socket->Send("Sync Error: Value for Master Composite %s did not match", masterCompositeName);
      ip_Log->LogMessage("%d: Value for Master Composite %s did not match", masterCompositeName);
      delete ip_MasterComposite;
      ip_MasterComposite = NULL;
      return false;
   }

   // Lock this MasterComposite for the remainder of this transaction.  Any other slaves/clients
   // trying to update this MasterComposite will have to wait for the sync to complete.
   ip_MasterComposite->Lock(ic_ServerID);
   return true;
}

//****************************************************************************************************//
// Based upon the success of the synchronization of this MasterComposite, complete the transaction
//****************************************************************************************************//
bool  IAmAMaster::EndMasterCompositeSync(bool success, const char *slaveID)
{
   if (success)
   {
      // Send updates from our slave to our master
      ip_MasterComposite->SetSendUpdatesToMaster(true);
      ip_MasterComposite->SetLastUpdateTime();

      // Send updates for this composite to all slaves
      if (success) success = ip_MasterComposite->SendUpdatesToAllSlaves();
      if (success) success = ip_MasterComposite->FinalizeChanges();
   }

   // Delete the object now that the transaction is complete.
   if (ip_MasterComposite) { delete ip_MasterComposite; ip_MasterComposite = NULL; }

   if (success)
      ip_DBInterface->Commit();
   else
      ip_DBInterface->Rollback();

   return success;
}

//****************************************************************************************************//
// This is the main loop to handle all socket/database processing for a distinct MasterComposite
// being sent from a slave.  The loop will terminate when it gets the "End Master Composite" message
//****************************************************************************************************//
bool  IAmAMaster::SyncMasterCompositeFromSlave(const char *masterCompositeName, const char *slaveID, bool &fatalError)
{
   bool     success;
   char    *theMessage;
   char     endMessage[NAME_LENGTH * 2];
   char     workMessage[NAME_LENGTH * 2];
   char     compositeMessage[NAME_LENGTH * 2];
   char     cfeMessage[NAME_LENGTH * 2];
   char     cfedMessage[NAME_LENGTH * 2];

   success = true;
   fatalError = false;

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
            success = HandleWorkFromSlave(theMessage);
         else if (!memcmp(theMessage, compositeMessage, strlen(compositeMessage)))
         {
            // We have a new composite for this master, apply the database updates from the
            // previous composite then delete the object.
            if (ip_Composite)
            {
               success = ip_Composite->FinalizeChanges();
               delete ip_Composite;
               ip_Composite = NULL;
            }

            success = HandleCompositeFromSlave(theMessage);
         }
         else if (!memcmp(theMessage, cfeMessage, strlen(cfeMessage)))
         {
            // We have a new composite factor event for this composite, apply the database
            // updates from the previous event then delete the object.
            if (ip_CompositeFactorEvent)
            {
               success = ip_CompositeFactorEvent->FinalizeChanges();
               delete ip_CompositeFactorEvent;
               ip_CompositeFactorEvent = NULL;
            }

            if (success) success = HandleCompositeFactorEventFromSlave(theMessage, slaveID);
         }
         else if (!memcmp(theMessage, cfedMessage, strlen(cfedMessage)))
            success = HandleCompositeFactorEventDetailFromSlave(theMessage);
         else
         {
            ip_Socket->Send("Sync Error: Message <%s> was not expected", theMessage);
            success = false;
         }
      }
   } while (!fatalError);

   if (success)
   {
      if (ip_CompositeFactorEvent) success = ip_CompositeFactorEvent->FinalizeChanges();
      if (success && ip_Composite) success = ip_Composite->FinalizeChanges();
   }

   if (ip_CompositeFactorEvent) { delete ip_CompositeFactorEvent; ip_CompositeFactorEvent = NULL; }
   if (ip_Composite) { delete ip_Composite; ip_Composite = NULL; }

   return success;
}

//****************************************************************************************************//
// Process work completed by the slave.
//****************************************************************************************************//
bool  IAmAMaster::HandleWorkFromSlave(const char *theMessage)
{
   double   b1;
   char     masterCompositeName[NAME_LENGTH+1];
   char     factorMethod[METHOD_LENGTH+1];
   int32_t  slaveCurves;
   bool     success;

   if (sscanf(theMessage, "Work for %s %s %lf %d", masterCompositeName, factorMethod, &b1, &slaveCurves) != 4)
   {
      ip_Socket->Send("Sync Error: Could parse <%s>", theMessage);
      ip_Log->LogMessage("%d: Could parse <%s>", ip_Socket->GetSocketID(), theMessage);
      return false;
   }

   success = ip_MasterComposite->AddWorkDone(factorMethod, b1, slaveCurves);

   return success;
}

//****************************************************************************************************//
// Process a composite being factored by the slave.
//****************************************************************************************************//
bool  IAmAMaster::HandleCompositeFromSlave(const char *theMessage)
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

   ip_Composite = new Composite(ip_DBInterface, ip_Log, compositeName, false);

   if (!ip_Composite->WasFound())
   {
      ip_Socket->Send("Sync Error: Composite %s not found on master", compositeName);
      ip_Log->LogMessage("%d: Composite %s not found on master", ip_Socket->GetSocketID(), compositeName);
      return false;
   }

   if (!ip_Composite->SetMasterCompositeName(masterCompositeName) || !ip_Composite->SetValue(compositeValue))
   {
      ip_Socket->Send("Sync Error: Composite value for %s mismatch", compositeName);
      ip_Log->LogMessage("%d: Composite value for %s mismatch", ip_Socket->GetSocketID(), compositeName);
      return false;
   }

   return true;
}
//****************************************************************************************************//
// Process factors found by the slave.
//****************************************************************************************************//
bool  IAmAMaster::HandleCompositeFactorEventFromSlave(const char *theMessage, const char *slaveID)
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
   ip_CompositeFactorEvent->SetSentToMaster(false);

   return true;
}

//****************************************************************************************************//
// Process factors found by the slave.
//****************************************************************************************************//
bool  IAmAMaster::HandleCompositeFactorEventDetailFromSlave(const char *theMessage)
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
// Send the MasterComposite to the slave if the MasterComposite has been updated since the
// last sync.
//****************************************************************************************************//
bool  IAmAMaster::SendUpdatesToSlave(const char *slaveID)
{
   SQLStatement  *selectStatement;
   const char    *selectSQL = "select mc.MasterCompositeName, mc.MasterCompositeValue, mc.IsActive "
                              "  from MasterComposite mc, MasterCompositeSlaveSync mscc " \
                              " where mc.MasterCompositeName = mscc.MasterCompositeName " \
                              "   and mscc.SendUpdatesToSlave = 1 " \
                              "   and mscc.SlaveID = ? " \
                              "order by mc.MasterCompositeName ";
   char           masterCompositeName[NAME_LENGTH+1];
   char           masterCompositeValue[VALUE_LENGTH+1];
   int32_t        updatedCount, failedCount, isActive;
   bool           success = true, fatalError = false;

   selectStatement = new SQLStatement(ip_Log, ip_DBInterface, selectSQL);
   selectStatement->BindInputParameter(slaveID, ID_LENGTH);
   selectStatement->BindSelectedColumn(masterCompositeName, NAME_LENGTH);
   selectStatement->BindSelectedColumn(masterCompositeValue, VALUE_LENGTH);
   selectStatement->BindSelectedColumn(&isActive);

   ip_Socket->Send("Start Master Composites");
   updatedCount = failedCount = 0;
   while (success && !fatalError && selectStatement->FetchRow(false))
   {
      if (updatedCount == 0 && failedCount == 0)
         ip_Log->LogMessage("Phase 2 between slave and master started.  Sending work/factors to master.");

      ip_MasterComposite = new MasterComposite(ip_DBInterface, ip_Log, masterCompositeName);
      ip_MasterComposite->Lock(ic_ServerID);

      success = SendMasterCompositeToSlave(masterCompositeName, slaveID, fatalError);

      if (success)
      {
         ip_DBInterface->Commit();
         updatedCount++;
      }
      else
      {
         ip_DBInterface->Rollback();
         failedCount++;
      }

      delete ip_MasterComposite;
   }

   selectStatement->CloseCursor();

   // Clear out last transaction (there shouldn't be any)
   ip_DBInterface->Rollback();

   if (fatalError)
   {      
      ip_Socket->Send("Fatal Sync Error: Terminated because a fatal error occured on the master.  Read the master logs to diagnose");
      ip_Log->LogMessage("Fatal Sync Error: Terminated because a fatal error occured.  Read the logs to diagnose");
      success = false;
   }

   ip_Socket->Send("No More Master Composites");

   if (failedCount > 0)
      ip_Log->LogMessage("%d: Phase 2 with slave %s completed with errors.  %d of %d composites were successfully synced.",
                         ip_Socket->GetSocketID(), slaveID, updatedCount, failedCount + updatedCount);
   else
      ip_Log->LogMessage("%d: Phase 2 with slave %s completed.  All %d composites were successfully synced",
                         ip_Socket->GetSocketID(), slaveID, updatedCount);

   delete selectStatement;

   return success;
}

//****************************************************************************************************//
// Send everything for the master composite to the master.
//****************************************************************************************************//
bool  IAmAMaster::SendMasterCompositeToSlave(const char *masterCompositeName, const char *slaveID, bool &fatalError)
{
   char          *theMessage;
   char           goodMessage[NAME_LENGTH * 2];
   char           badMessage[NAME_LENGTH * 2];
   bool           success = true;

   ip_Socket->Send("Begin Master Composite %s %s %d %d",
                   masterCompositeName, 
                   ip_MasterComposite->GetValue().c_str(),
                   (ip_MasterComposite->IsActive() ? 1 : 0),
                   strlen(ip_MasterComposite->GetValue().c_str()));

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

   // At this point we have confirmed that the slave has the same MasterComposite
   // that the master is trying to sync.

   ip_Socket->StartBuffering();

   SendWorkToSlave(masterCompositeName);
   SendCompositesToSlave(masterCompositeName, slaveID);

   ip_Socket->Send("End Master Composite %s", masterCompositeName);
   ip_Socket->SendBuffer();

   // Build the messages that we are expecting
   sprintf(goodMessage, "Master Composite %s OK", masterCompositeName);
   sprintf(badMessage, "Master Composite %s Error", masterCompositeName);

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
         success = ip_MasterComposite->SentUpdateToSlave(slaveID);
         break;
      }

      // Any other messages should just be logged.  They don't indicate
      // success or failure for the sync.
      ip_Log->LogMessage(theMessage);
   } while (!fatalError);

   return success;
}

//****************************************************************************************************//
// Send all work for this master composite to the slave.
//****************************************************************************************************//
void  IAmAMaster::SendWorkToSlave(const char *masterCompositeName)
{
   SQLStatement  *selectStatement;
   const char    *selectSQL = "select FactorMethod, B1, LocalCurves " \
                              "  from MasterCompositeWorkDone " \
                              " where MasterCompositeName = ? ";
   char           factorMethod[METHOD_LENGTH+1];
   double         b1;
   int64_t        localCurves;

   selectStatement = new SQLStatement(ip_Log, ip_DBInterface, selectSQL);
   selectStatement->BindInputParameter(masterCompositeName, NAME_LENGTH);
   selectStatement->BindSelectedColumn(factorMethod, sizeof(factorMethod));
   selectStatement->BindSelectedColumn(&b1);
   selectStatement->BindSelectedColumn(&localCurves);

   while (selectStatement->FetchRow(false))
      ip_Socket->Send("Work for %s %s %.0lf %"PRId64"", masterCompositeName, factorMethod, b1, localCurves);

   selectStatement->CloseCursor();

   delete selectStatement;
}

//****************************************************************************************************//
// Send all composites for this master composite to the slave.
//****************************************************************************************************//
void  IAmAMaster::SendCompositesToSlave(const char *masterCompositeName, const char *slaveID)
{
   SQLStatement  *selectStatement;
   const char    *selectSQL = "select CompositeName, CompositeValue, DecimalLength " \
                              "  from Composite " \
                              " where MasterCompositeName = ? ";
   char           compositeName[METHOD_LENGTH+1];
   char           compositeValue[VALUE_LENGTH+1];
   int64_t        decimalLength;

   selectStatement = new SQLStatement(ip_Log, ip_DBInterface, selectSQL);
   selectStatement->BindInputParameter(masterCompositeName, NAME_LENGTH);
   selectStatement->BindSelectedColumn(compositeName, METHOD_LENGTH);
   selectStatement->BindSelectedColumn(compositeValue, VALUE_LENGTH);
   selectStatement->BindSelectedColumn(&decimalLength);

   while (selectStatement->FetchRow(false))
   {
      ip_Socket->Send("Composite for %s %s %s %d", masterCompositeName, compositeName, compositeValue, decimalLength);

      SendCompositeFactorEventsToSlave(masterCompositeName, compositeName);
   }

   selectStatement->CloseCursor();

   delete selectStatement;
}


//****************************************************************************************************//
// Send all factors for this composite to the slave.
//****************************************************************************************************//
void  IAmAMaster::SendCompositeFactorEventsToSlave(const char *masterCompositeName, const char *compositeName)
{
   SQLStatement  *selectStatement;
   const char    *selectSQL = "select cfe.FactorEvent, cfe.ReportedDate, " \
                              "       cfe.EmailID, cfe.UserID, cfe.MachineID, cfe.InstanceID, " \
                              "       cfe.ServerID, cfe.FactorMethod, cfe.FactorStep, " \
                              "       cfe.B1, cfe.B1Time, cfe.B2, cfe.B2Time, cfe.Sigma, cfe.X0 " \
                              "  from CompositeFactorEvent cfe, Composite " \
                              " where cfe.CompositeName = Composite.CompositeName " \
                              "   and Composite.MasterCompositeName = ? " \
                              "   and Composite.CompositeName = ? ";
   char           emailID[ID_LENGTH+1];
   char           userID[ID_LENGTH+1];
   char           machineID[ID_LENGTH+1];
   char           instanceID[ID_LENGTH+1];
   char           serverID[ID_LENGTH+1];
   char           factorMethod[METHOD_LENGTH+1];
   int32_t        factorEvent, factorStep;
   double         b1, b1Time, b2, b2Time;
   int64_t        sigma, x0, reportedDate;

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

      SendCompositeFactorEventDetailsToSlave(masterCompositeName, compositeName, factorEvent);
   }

   selectStatement->CloseCursor();

   delete selectStatement;
}

//****************************************************************************************************//
// Send all factors for this composite to the slave.
//****************************************************************************************************//
void  IAmAMaster::SendCompositeFactorEventDetailsToSlave(const char *masterCompositeName, const char *compositeName, int32_t factorEvent)
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
bool  IAmAMaster::GetUserStatsFromSlave(void)
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
         userStats->IncrementWorkDone(workDone);
         userStats->IncrementFactorCount(factorCount);
         userStats->IncrementTimeSpent(timeSpent);
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

   return success;
}

//****************************************************************************************************//
bool  IAmAMaster::SendUserStatsToSlave(void)
{
   SQLStatement  *selectStatement;
   const char    *selectSQL = "select UserID, TotalWorkDone, TotalFactorsFound, TotalTime " \
                              "  from UserStats " \
                              " where TotalWorkDone > 0 " \
                              "order by UserID ";
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

   ip_Socket->Send("Start User Stats");
   success = true;
   while (success && selectStatement->FetchRow(false))
   {
      ip_Socket->Send("User Stats %s %.0lf %d %lf", userID, workToSend, factorsToSend, timeSpent);
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

   ip_Log->LogMessage("%d: Phase 3 completed.  User stats have been synced", ip_Socket->GetSocketID());

   return success;
}
