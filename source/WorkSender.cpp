/* WorkSender Class for ECMNet.

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

#include "WorkSender.h"
#include "SQLStatement.h"
#include "server.h"
#include "KeepAliveThread.h"

WorkSender::WorkSender(DBInterface *dbInterface, HelperSocket *theSocket, Log *theLog,
                       double minB1, double maxB1, int32_t pm1OverECM, int32_t pp1OverECM,
                       b1_t *b1, int32_t b1Count, SharedMemoryItem *totalWorkSent,
                       strategy_t *pStrategy, SharedMemoryItem *locker)
{
   ip_DBInterface = dbInterface;
   ip_Socket = theSocket;
   ip_Log = theLog;
   ip_Locker = locker;
   ip_Strategy = pStrategy;
   id_MinB1 = minB1;
   id_MaxB1 = maxB1;
   ip_B1 = b1;
   ii_B1Count = b1Count;
   ii_PM1OverECM = pm1OverECM;
   ii_PP1OverECM = pp1OverECM;

   ip_TotalWorkSent = totalWorkSent;
}

WorkSender::~WorkSender()
{
}

void  WorkSender::ProcessMessage(char *theMessage)
{
   char        clientVersion[20], gmpECMVersion[80];
   char       *ptr;
   int32_t     sentWorkUnits;
   strategy_t *pStrategy;
   double      workDone = 0.0;

   ip_Locker->Lock();

   pStrategy = ip_Strategy;
   while (pStrategy)
   {
      workDone = ip_TotalWorkSent->GetDoubleNoLock() * pStrategy->selectStrategyPercent * .01;

      // If we have sent less than needed, then use this strategy
      if (pStrategy->workSent < workDone)
         break;

      pStrategy = (strategy_t *) pStrategy->next;
   }

   if (!pStrategy)
      pStrategy = ip_Strategy;

   ip_Log->Debug(DEBUG_WORK, "%d: Using strategy %s (%d < %d)",
                 ip_Socket->GetSocketID(), pStrategy->sortSequence, pStrategy->workSent, workDone);

   ip_Locker->Release();

   ptr = strchr(theMessage, ':');
   while (ptr)
   {
      *ptr = ' ';
      ptr = strchr(theMessage, ':');
   }

   sscanf(theMessage, "GETWORK %s %s", clientVersion, gmpECMVersion);

   sentWorkUnits = SendUsingStrategy(pStrategy);

   if (sentWorkUnits == 0)
      ip_Socket->Send("INFO: No available candidates are left on this server.");

   ip_Socket->Send("End of Message");
}

bool  WorkSender::SendUsingStrategy(strategy_t *pStrategy)
{
   KeepAliveThread *keepAliveThread;
   SharedMemoryItem *threadWaiter;
   SQLStatement  *selectStatement;
   const char    *selectSQL = "select Composite.MasterCompositeName, Composite.CompositeName, " \
                              "       Composite.DecimalLength, MasterComposite.TotalWorkDone, " \
                              "       Composite.LastSendTime, Composite.CompositeValue " \
                              "  from MasterComposite, Composite " \
                              " where MasterComposite.IsActive = 1 " \
                              "   and MasterComposite.MasterCompositeName = Composite.MasterCompositeName " \
                              "   and Composite.FactorEventCount = 0 " \
                              "order by %s limit 100 ";
   double   lastSendTime, baseTime, totalWorkDone, workSent;
   double   selectPercent, loopAdjustPercent, adjustPercent, randomPercent;
   int32_t  decimalLength;
   char     masterCompositeName[NAME_LENGTH+1];
   char     compositeName[NAME_LENGTH+1];
   char     compositeValue[VALUE_LENGTH+1];
   bool     endLoop;
   int32_t  rowsFetched;

   threadWaiter = ip_Socket->GetThreadWaiter();
   threadWaiter->SetValueNoLock(KAT_WAITING);
   keepAliveThread = new KeepAliveThread();
   keepAliveThread->StartThread(ip_Socket);

   srand((uint32_t) time(NULL));
   baseTime = (double) time(NULL) - (pStrategy->hourBreak * 3600.0);

   selectStatement = new SQLStatement(ip_Log, ip_DBInterface, selectSQL, pStrategy->sortSequence);
   selectStatement->BindSelectedColumn(masterCompositeName, NAME_LENGTH);
   selectStatement->BindSelectedColumn(compositeName, NAME_LENGTH);
   selectStatement->BindSelectedColumn(&decimalLength);
   selectStatement->BindSelectedColumn(&totalWorkDone);
   selectStatement->BindSelectedColumn(&lastSendTime);
   selectStatement->BindSelectedColumn(compositeValue, VALUE_LENGTH);

   workSent = 0.0;
   loopAdjustPercent = 0.0;
   while (workSent == 0.0)
   {
      rowsFetched = 0;
      while (workSent == 0.0 && selectStatement->FetchRow(false))
      {
         rowsFetched++;
         selectPercent = pStrategy->selectCandidatePercent;
         adjustPercent = 0.0;

         if (lastSendTime == 0)
            selectPercent = 100.0;
         else if (lastSendTime < baseTime)
            adjustPercent = pStrategy->percentAddIfOverHours  * (int32_t) ((baseTime - lastSendTime) / 3600);
         else
            adjustPercent = pStrategy->percentSubIfUnderHours * (int32_t) ((baseTime - lastSendTime) / 3600);

         randomPercent = ((double) (rand() % 100));
         ip_Log->Debug(DEBUG_WORK, "%d: %s: (Random - Loop) %.0lf - %.0lf : %.0lf + %.0lf (Select + Adjust)",
                       ip_Socket->GetSocketID(), compositeName, randomPercent, loopAdjustPercent, selectPercent, adjustPercent);

         if ((randomPercent - loopAdjustPercent) <= (selectPercent + adjustPercent))
         {
            ip_TotalWorkSent->Lock();

            workSent = SendWork(masterCompositeName, compositeName, compositeValue, decimalLength, totalWorkDone);
            if (workSent > 0.0)
            {
               pStrategy->workSent += workSent;
               ip_TotalWorkSent->IncrementDoubleHaveLock(workSent);
               ip_DBInterface->Commit();
            }
            else
               ip_DBInterface->Rollback();

            ip_TotalWorkSent->Release();
         }
      }

      selectStatement->CloseCursor();

      if (rowsFetched == 0)
         break;

      // The next time through the cursor, increase probability of work selection by 10%
      loopAdjustPercent += 10.0;
   }

   delete selectStatement;

   // When we know that the keepalive thread has terminated, we can then exit
   // this loop and delete the memory associated with that thread.
   endLoop = false;
   while (!endLoop)
   {
      threadWaiter->Lock();

      if (threadWaiter->GetValueHaveLock() == KAT_TERMINATED)
      {
         endLoop = true;
         threadWaiter->Release();
      }
      else
      {
         threadWaiter->SetValueHaveLock(KAT_TERMINATE);
         threadWaiter->Release();
         Sleep(1000);
      }
   }

   delete keepAliveThread;

   return (workSent > 0.0 ? true : false);
}

// Determine the current B1 for ECM work, but before sending any curves, verify that all P-1 and P+1
// has been done to the appropriate B1 level.
double   WorkSender::SendWork(const char *masterCompositeName, const char *compositeName, const char *compositeValue,
                              int32_t decimalLength, double totalWorkDone)
{
   int32_t  ecmIndex, ecmCurves, curvesDone, curvesLeft;
   int32_t  maxPM1Index, pm1Index;
   int32_t  maxPP1Index, pp1Index;

   if (!GetECMIndexAndCurves(totalWorkDone, ecmIndex, ecmCurves))
   {
      ip_Log->Debug(DEBUG_WORK, "%d: All ECM work has been done on composite %s",
                    ip_Socket->GetSocketID(), compositeName);
      return 0.0;
   }

   maxPM1Index = ecmIndex + ii_PM1OverECM;
   maxPP1Index = ecmIndex + ii_PP1OverECM;
   pm1Index = pp1Index = ecmIndex;

   while (pm1Index < maxPM1Index && pp1Index < maxPP1Index)
   {
      // If we need to send P-1 work for a higher B1, now is the time to do it
      if (pm1Index < maxPM1Index && pm1Index < ii_B1Count)
      {
         curvesDone = GetCurvesDone(masterCompositeName, FM_PM1, ip_B1[pm1Index].b1);
         curvesLeft = ip_B1[pm1Index].curvesPM1 - curvesDone;
         if (curvesLeft > 0 && ip_B1[pm1Index].b1 >= id_MinB1 && ip_B1[pm1Index].b1 <= id_MaxB1)
         {
            SendWorkUnit(compositeName, compositeValue, decimalLength, FM_PM1, ip_B1[pm1Index].b1, curvesLeft);
            return curvesLeft * ip_B1[pm1Index].b1;
         }
      }

      // If we need to send P+1 work for a higher B1, now is the time to do it
      if (pp1Index < maxPP1Index && pp1Index < ii_B1Count)
      {
         curvesDone = GetCurvesDone(masterCompositeName, FM_PP1, ip_B1[pp1Index].b1);
         curvesLeft = ip_B1[pp1Index].curvesPP1 - curvesDone;
         if (curvesLeft > 0 && ip_B1[pp1Index].b1 >= id_MinB1 && ip_B1[pp1Index].b1 <= id_MaxB1)
         {
            SendWorkUnit(compositeName, compositeValue, decimalLength, FM_PP1, ip_B1[pp1Index].b1, curvesLeft);
            return curvesLeft * ip_B1[pp1Index].b1;
         }
      }

      pm1Index++;
      pp1Index++;
   }

   // If there wasn't any P-1 or P+1 work to be done, send the ECM work to be done.
   SendWorkUnit(compositeName, compositeValue, decimalLength, FM_ECM, ip_B1[ecmIndex].b1, ecmCurves);
   return ecmCurves * ip_B1[ecmIndex].b1;
}

// Determine the current B1 that needs ECM work and return the number of curves needed for that B1
bool     WorkSender::GetECMIndexAndCurves(double totalWorkDone, int32_t &ecmIndex, int32_t &ecmCurves)
{
   double   workLeft;

   ecmIndex = 0;
   while (ecmIndex < ii_B1Count)
   {
      // Can't do work at higher B1 values
      if (ip_B1[ecmIndex].b1 > id_MaxB1)
         return false;

      // Can't do work at lower B1 values
      if (ip_B1[ecmIndex].b1 < id_MinB1)
      {
         ecmIndex++;
         continue;
      }

      // sumWork is the total amount of ECM work needed to complete all curves through
      // this B1.  If sumWork > totalWorkDone then it means that we have not completed
      // all ECM curves for this B1.
      if (ip_B1[ecmIndex].sumWork > totalWorkDone)
      {
         workLeft = ip_B1[ecmIndex].sumWork - totalWorkDone;
         ecmCurves = (int32_t) (workLeft / ip_B1[ecmIndex].b1);
         break;
      }

      // If all ECM work has been done on this composite, then we can't send it
      if (++ecmIndex == ii_B1Count)
         return false;
   }

   // We have work to do for this B1, so make sure that we have at least 1 curve.
   if (!ecmCurves) ecmCurves++;

   return true;
}

// Return the number of curves done for the given factoring method and B1
int32_t  WorkSender::GetCurvesDone(const char *masterCompositeName, const char *factorMethod, double b1)
{
   SQLStatement  *selectStatement;
   const char    *selectSQL = "select LocalCurves " \
                              "  from MasterCompositeWorkDone " \
                              " where MasterCompositeName = ? " \
                              "   and FactorMethod = ? " \
                              "   and B1 = ? ";
   int32_t  curvesDone = 0;

   selectStatement = new SQLStatement(ip_Log, ip_DBInterface, selectSQL);
   selectStatement->BindInputParameter(masterCompositeName, NAME_LENGTH);
   selectStatement->BindInputParameter(factorMethod, METHOD_LENGTH);
   selectStatement->BindInputParameter(b1);
   selectStatement->BindSelectedColumn(&curvesDone);

   selectStatement->FetchRow(true);

   ip_Log->Debug(DEBUG_WORK, "%d: %d %s curves done for B1=%.0lf",
                 ip_Socket->GetSocketID(), curvesDone, factorMethod, b1);

   delete selectStatement;

   return curvesDone;
}

// Send the workunit to the client.  Make sure to send using the 2.x or 3.0 protocol depending upon
// the version of the client.  Then insert a record into PendingWork so that we can track who has
// an active factoring assignment.
double   WorkSender::SendWorkUnit(const char *compositeName, const char *compositeValue,
                                  int32_t decimalLength, const char *factorMethod,
                                  double b1, int32_t curvesToDo)
{
   SQLStatement *updateStatement;
   SQLStatement *insertStatement;
   const char *updateSQL = "update Composite set LastSendTime = ? where CompositeName = ?;";
   const char *insertSQL = "insert into PendingWork ( UserID, MachineID, InstanceID, EmailID, CompositeName, " \
                           " FactorMethod, B1, Curves, DateAssigned ) " \
                           " values ( ?,?,?,?,?,?,?,? ) ";
   bool success;

   if (!memcmp(ip_Socket->GetClientVersion(), "2.x", 3))
   {
      ip_Socket->Send("ServerVersion: %s", ECMNET_VERSION);
      ip_Socket->Send("ServerType: GMP-ECM");

      ip_Socket->Send("ECMname: %s", compositeName);
      ip_Socket->Send("ECMnumber: %s", compositeValue);

      ip_Socket->Send("ECMB1: %.0lf", b1);
      ip_Socket->Send("FactorMethod: %s", factorMethod);

      ip_Socket->Send("ECMCurves: 1:%ld", curvesToDo);
   }
   else
   {
      ip_Socket->Send("Factoring Work for %s %s %d %s %.0lf %d %d",
                      compositeName, compositeValue, decimalLength, factorMethod,
                      b1, curvesToDo, strlen(compositeValue));
   }

   insertStatement = new SQLStatement(ip_Log, ip_DBInterface, insertSQL);
   insertStatement->BindInputParameter(ip_Socket->GetUserID(), ID_LENGTH);
   insertStatement->BindInputParameter(ip_Socket->GetMachineID(), ID_LENGTH);
   insertStatement->BindInputParameter(ip_Socket->GetInstanceID(), ID_LENGTH);
   insertStatement->BindInputParameter(ip_Socket->GetEmailID(), ID_LENGTH);
   insertStatement->BindInputParameter(compositeName, NAME_LENGTH);
   insertStatement->BindInputParameter(factorMethod, METHOD_LENGTH);
   insertStatement->BindInputParameter(b1);
   insertStatement->BindInputParameter(curvesToDo);
   insertStatement->BindInputParameter((int64_t) time(NULL));
   success = insertStatement->Execute();
   delete insertStatement;

   // Hard failure if we can't update.  In other words, if this fails, then something is
   // seriously borked with the code or database.
   if (!success)
   {
      ip_Log->LogMessage("Failed to insert pending work.  Exiting.");
      exit(0);
   }

   updateStatement = new SQLStatement(ip_Log, ip_DBInterface, updateSQL);
   updateStatement->BindInputParameter((int64_t) time(NULL));
   updateStatement->BindInputParameter(compositeName, NAME_LENGTH);
   success = updateStatement->Execute();
   delete updateStatement;

   // Hard failure if we can't update.  In other words, if this fails, then something is
   // seriously borked with the code or database.
   if (!success)
   {
      ip_Log->LogMessage("Failed to set LastSendTime.  Exiting.");
      exit(0);
   }

   ip_Log->LogMessage("%d: Sent %s: %d %s curves for B1=%.0lf to %s",
                      ip_Socket->GetSocketID(), compositeName, curvesToDo, 
                      factorMethod, b1, ip_Socket->GetFromAddress());

   return (b1 * (double) curvesToDo);
}

