/* MasterComposite Class for ECMNet.

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

#include "MasterComposite.h"
#include "SQLStatement.h"

MasterComposite::MasterComposite(DBInterface *dbInterface, Log *theLog, string masterCompositeName)
{
   SQLStatement *selectStatement;
   const char *selectSQL = "select MasterCompositeValue, FullyFactored, " \
                           "       FactorCount, RecurseIndex, DecimalLength, " \
                           "       LastUpdateTime, SendUpdatesToMaster, IsActive " \
                           "  from MasterComposite " \
                           " where MasterCompositeName = ? ";
   char  masterCompositeValue[VALUE_LENGTH+1];
   int32_t  fullyFactored;
   int32_t  isActive;
   int32_t  sendUpdatesToMaster;

   ip_DBInterface = dbInterface;
   ip_Log = theLog;
   is_MasterCompositeName = masterCompositeName;

   id_TotalWorkDone = 0.0;

   selectStatement = new SQLStatement(ip_Log, ip_DBInterface, selectSQL);
   selectStatement->BindInputParameter(masterCompositeName, NAME_LENGTH);
   selectStatement->BindSelectedColumn(masterCompositeValue, VALUE_LENGTH);
   selectStatement->BindSelectedColumn(&fullyFactored);
   selectStatement->BindSelectedColumn(&ii_FactorCount);
   selectStatement->BindSelectedColumn(&ii_RecurseIndex);
   selectStatement->BindSelectedColumn(&ii_DecimalLength);
   selectStatement->BindSelectedColumn(&il_LastUpdateTime);
   selectStatement->BindSelectedColumn(&sendUpdatesToMaster);
   selectStatement->BindSelectedColumn(&isActive);

   ib_Found = selectStatement->FetchRow(true);

   if (ib_Found)
   {
      ib_Active = ((isActive == 1) ? true : false);
      ib_SendUpdatesToMaster = ((sendUpdatesToMaster == 1) ? true : false);
      ib_FullyFactored = (fullyFactored == 1);
      is_MasterCompositeValue = masterCompositeValue;
   }
   else
   {
      ib_Active = true;
      ib_FullyFactored = false;
      ii_RecurseIndex = 0;
      ii_FactorCount = 0;
      il_LastUpdateTime = time(NULL);
      ii_DecimalLength = 0;
   }

   ip_Locker = NULL;
   delete selectStatement;
}

MasterComposite::~MasterComposite()
{
   if (ip_Locker)
   {
      ip_Locker->Release();
      delete ip_Locker;
   }
}

void     MasterComposite::Lock(const char *serverID)
{
   char  lockName[NAME_LENGTH * 2];

   sprintf(lockName, "%s_%s", serverID, is_MasterCompositeName.c_str());
   ip_Locker = new SharedMemoryItem(lockName);
   ip_Locker->Lock();
}

bool     MasterComposite::SetValue(const char *masterCompositeValue)
{
   if (ib_Found && (is_MasterCompositeName != (string) masterCompositeValue))
   {
      ip_Log->LogMessage("Error: Trying to override master composite value for %s", is_MasterCompositeName.c_str());

      return false;
   }

   is_MasterCompositeValue = masterCompositeValue;
   ComputeDecimalLength();
   return true;
}

bool     MasterComposite::FinalizeChanges(void)
{
   SQLStatement  *selectStatement;
   const char    *selectSQL = "select sum(LocalCurves * B1) " \
                              "  from MasterCompositeWorkDone " \
                              " where MasterCompositeName = ? " \
                              "   and FactorMethod = ? ";
   bool     success = true;

   if (ii_DecimalLength == 0)
   {
      ip_Log->LogMessage("Error: Master composite has no length %s", is_MasterCompositeName.c_str());
      return false;
   }

   if (ib_Found)
   {
      selectStatement = new SQLStatement(ip_Log, ip_DBInterface, selectSQL);
      selectStatement->BindInputParameter(is_MasterCompositeName, NAME_LENGTH);
      selectStatement->BindInputParameter((char *) FM_ECM, METHOD_LENGTH);
      selectStatement->BindSelectedColumn(&id_TotalWorkDone);
      success = selectStatement->FetchRow(true);
      delete selectStatement;
   }

   if (!success) return false;

   if (ib_Found)
      return Update();
   else
      return Insert();
}

bool     MasterComposite::Update(void)
{
   SQLStatement *updateStatement;
   const char   *updateSQL = "update MasterComposite " \
                             "   set FullyFactored = ?, " \
                             "       FactorCount = ?, " \
                             "       RecurseIndex = ?, " \
                             "       DecimalLength = ?, " \
                             "       TotalWorkDone = ?, " \
                             "       LastUpdateTime = ?, " \
                             "       SendUpdatesToMaster = ?, " \
                             "       IsActive = ? " \
                             " where MasterCompositeName = ? ";
   bool     success;

   updateStatement = new SQLStatement(ip_Log, ip_DBInterface, updateSQL);
   updateStatement->BindInputParameter((ib_FullyFactored ? 1 : 0));
   updateStatement->BindInputParameter(ii_FactorCount);
   updateStatement->BindInputParameter(ii_RecurseIndex);
   updateStatement->BindInputParameter(ii_DecimalLength);
   updateStatement->BindInputParameter(id_TotalWorkDone);
   updateStatement->BindInputParameter(il_LastUpdateTime);
   updateStatement->BindInputParameter((ib_SendUpdatesToMaster ? 1 : 0));
   updateStatement->BindInputParameter((ib_Active ? 1 : 0));
   updateStatement->BindInputParameter(is_MasterCompositeName, NAME_LENGTH);

   success = updateStatement->Execute();

   delete updateStatement;

   return success;
}

bool     MasterComposite::Insert(void)
{
   SQLStatement *insertStatement;
   const char   *insertSQL = "insert into MasterComposite " \
                             "( MasterCompositeName, MasterCompositeValue, FullyFactored, " \
                             "  FactorCount, RecurseIndex, DecimalLength, TotalWorkDone, " \
                             "  LastUpdateTime, SendUpdatesToMaster, IsActive ) " \
                             "values ( ?,?,?,?,?,?,?,?,0,? ) ";
   bool     success;

   insertStatement = new SQLStatement(ip_Log, ip_DBInterface, insertSQL);
   insertStatement->BindInputParameter(is_MasterCompositeName, NAME_LENGTH);
   insertStatement->BindInputParameter(is_MasterCompositeValue, VALUE_LENGTH);
   insertStatement->BindInputParameter((ib_FullyFactored ? 1 : 0));
   insertStatement->BindInputParameter(ii_FactorCount);
   insertStatement->BindInputParameter(ii_RecurseIndex);
   insertStatement->BindInputParameter(ii_DecimalLength);
   insertStatement->BindInputParameter(id_TotalWorkDone);
   insertStatement->BindInputParameter(il_LastUpdateTime);
   insertStatement->BindInputParameter((ib_Active ? 1 : 0));

   success = insertStatement->Execute();

   delete insertStatement;

   if (success) ib_Found = true;

   return success;
}

bool     MasterComposite::InsertWorkDoneFromMaster(const char *factorMethod, double b1, int32_t curvesDone)
{
   SQLStatement *insertStatement;
   const char *insertSQL = "insert into MasterCompositeWorkDone " \
                           " ( MasterCompositeName, FactorMethod, B1, CurvesSinceLastReport, LocalCurves ) " \
                           "values ( ?,?,?,0,? ) ";
   bool     success;

   insertStatement = new SQLStatement(ip_Log, ip_DBInterface, insertSQL);
   insertStatement->BindInputParameter(is_MasterCompositeName, NAME_LENGTH);
   insertStatement->BindInputParameter(factorMethod, METHOD_LENGTH);
   insertStatement->BindInputParameter(b1);
   insertStatement->BindInputParameter(curvesDone);

   success = insertStatement->Execute();

   delete insertStatement;

   return success;
}

bool     MasterComposite::AddWorkDone(const char *factorMethod, double b1, int32_t curvesToMaster, int32_t curvesDone)
{
   SQLStatement *insertStatement;
   const char *insertSQL = "insert into MasterCompositeWorkDone " \
                           " ( MasterCompositeName, FactorMethod, B1, CurvesSinceLastReport, LocalCurves ) " \
                           "values ( ?,?,?,?,? ) ";

   bool     success;

   insertStatement = new SQLStatement(ip_Log, ip_DBInterface, insertSQL);
   insertStatement->BindInputParameter(is_MasterCompositeName, NAME_LENGTH);
   insertStatement->BindInputParameter(factorMethod, METHOD_LENGTH);
   insertStatement->BindInputParameter(b1);
   insertStatement->BindInputParameter(curvesToMaster);
   insertStatement->BindInputParameter(curvesDone);

   success = insertStatement->Execute();

   delete insertStatement;

   return success;
}

bool     MasterComposite::AddWorkDone(const char *factorMethod, double b1, int32_t curvesDone)
{
   SQLStatement *selectStatement;
   SQLStatement *insertStatement;
   SQLStatement *updateStatement;
   const char *selectSQL = "select count(*) " \
                           "  from MasterCompositeWorkDone " \
                           " where MasterCompositeName = ? " \
                           "   and FactorMethod = ? " \
                           "   and B1 = ? ";
   const char *updateSQL = "update MasterCompositeWorkDone " \
                           "   set CurvesSinceLastReport = CurvesSinceLastReport + ?, " \
                           "       LocalCurves = LocalCurves + ? " \
                           " where MasterCompositeName = ? " \
                           "   and FactorMethod = ? " \
                           "   and B1 = ? ";
   const char *insertSQL = "insert into MasterCompositeWorkDone " \
                           " ( MasterCompositeName, FactorMethod, B1, CurvesSinceLastReport, LocalCurves ) " \
                           "values ( ?,?,?,?,? ) ";
   int32_t  rows;
   bool     success;

   selectStatement = new SQLStatement(ip_Log, ip_DBInterface, selectSQL);
   selectStatement->BindSelectedColumn(&rows);
   selectStatement->BindInputParameter(is_MasterCompositeName, NAME_LENGTH);
   selectStatement->BindInputParameter(factorMethod, METHOD_LENGTH);
   selectStatement->BindInputParameter(b1);

   if (selectStatement->FetchRow(true) && rows > 0)
   {
      updateStatement = new SQLStatement(ip_Log, ip_DBInterface, updateSQL);
      updateStatement->BindInputParameter(curvesDone);
      updateStatement->BindInputParameter(curvesDone);
      updateStatement->BindInputParameter(is_MasterCompositeName, NAME_LENGTH);
      updateStatement->BindInputParameter(factorMethod, METHOD_LENGTH);
      updateStatement->BindInputParameter(b1);

      success = updateStatement->Execute();

      delete updateStatement;
   }
   else
   {
      insertStatement = new SQLStatement(ip_Log, ip_DBInterface, insertSQL);
      insertStatement->BindInputParameter(is_MasterCompositeName, NAME_LENGTH);
      insertStatement->BindInputParameter(factorMethod, METHOD_LENGTH);
      insertStatement->BindInputParameter(b1);
      insertStatement->BindInputParameter(curvesDone);
      insertStatement->BindInputParameter(curvesDone);

      success = insertStatement->Execute();

      delete insertStatement;
   }

   delete selectStatement;

   return success;
}

void     MasterComposite::ComputeDecimalLength(void)
{
   int32_t  ignore;

   ii_DecimalLength = eval_str(is_MasterCompositeValue.c_str(), &ignore);
}

bool     MasterComposite::UpdateIfFullyFactored(void)
{
   SQLStatement *selectStatement;
   const char *selectSQL1 = "select count(*) " \
                            "  from Composite " \
                            " where MasterCompositeName = ? " \
                            "   and FactorEventCount = 0 ";
   const char *selectSQL2 = "select count(*) " \
                            "  from Composite, CompositeFactorEvent cfe, CompositeFactorEventDetail cfed " \
                            " where MasterCompositeName = ? " \
                            "   and Composite.CompositeName = cfe.CompositeName " \
                            "   and Composite.CompositeName = cfed.CompositeName " \
                            "   and cfed.FactorEvent = cfe.FactorEvent " \
                            "   and cfed.IsRecursed = 0 " \
                            "   and cfed.PRPTestResult = ? ";
   int32_t  rows;
   bool     success;

   selectStatement = new SQLStatement(ip_Log, ip_DBInterface, selectSQL1);
   selectStatement->BindSelectedColumn(&rows);
   selectStatement->BindInputParameter(is_MasterCompositeName, NAME_LENGTH);

   success = selectStatement->FetchRow(true);

   delete selectStatement;

   // If we have unfactored composites, return
   if (!success || rows > 0)
      return true;

   selectStatement = new SQLStatement(ip_Log, ip_DBInterface, selectSQL2);
   selectStatement->BindSelectedColumn(&rows);
   selectStatement->BindInputParameter(is_MasterCompositeName, NAME_LENGTH);
   selectStatement->BindInputParameter(PRPTEST_AS_COMPOSITE);

   success = selectStatement->FetchRow(true);

   delete selectStatement;

   // If factored composites have composite factor that has not been recursed, return
   if (!success || rows > 0)
      return true;

   ib_FullyFactored = true;

   return Update();  
}

bool     MasterComposite::SendUpdatesToAllSlaves(void)
{
   SQLStatement *updateStatement;
   const char *updateSQL = "update MasterCompositeSlaveSync " \
                           "   set SendUpdatesToSlave = 1 " \
                           " where MasterCompositeName = ? ";
   bool     success;

   updateStatement = new SQLStatement(ip_Log, ip_DBInterface, updateSQL);
   updateStatement->BindInputParameter(is_MasterCompositeName, NAME_LENGTH);

   success = updateStatement->Execute();

   delete updateStatement;

   return success;
}

bool     MasterComposite::SentUpdateToSlave(const char *slaveID)
{
   SQLStatement *updateStatement;
   const char *updateSQL = "update MasterCompositeSlaveSync " \
                           "   set SendUpdatesToSlave = 0 " \
                           " where MasterCompositeName = ? " \
                           "   and SlaveID = ? ";
   bool     success;

   updateStatement = new SQLStatement(ip_Log, ip_DBInterface, updateSQL);
   updateStatement->BindInputParameter(is_MasterCompositeName, NAME_LENGTH);
   updateStatement->BindInputParameter(slaveID, ID_LENGTH);

   success = updateStatement->Execute();

   delete updateStatement;

   return success;
}
