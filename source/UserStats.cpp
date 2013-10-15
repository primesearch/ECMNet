/* UserStats Class for ECMNet.

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

#include "UserStats.h"
#include "SQLStatement.h"

UserStats::UserStats(DBInterface *dbInterface, Log *theLog, string userID)
{
   SQLStatement *selectStatement;
   const char *selectSQL = "select WorkSinceLastReport, FactorsSinceLastReport, TimeSinceLastReport, " \
                           "       TotalWorkDone, TotalFactorsFound, TotalTime " \
                           "  from UserStats " \
                           " where UserID = ? ";

   ip_DBInterface = dbInterface;
   ip_Log = theLog;
   is_UserID = userID;

   selectStatement = new SQLStatement(ip_Log, ip_DBInterface, selectSQL);
   selectStatement->BindInputParameter(is_UserID, NAME_LENGTH);
   selectStatement->BindSelectedColumn(&id_WorkDone);
   selectStatement->BindSelectedColumn(&ii_FactorCount);
   selectStatement->BindSelectedColumn(&id_TimeSpent);
   selectStatement->BindSelectedColumn(&id_TotalWorkDone);
   selectStatement->BindSelectedColumn(&ii_TotalFactorCount);
   selectStatement->BindSelectedColumn(&id_TotalTimeSpent);

   ib_Found = selectStatement->FetchRow(true);

   delete selectStatement;

   if (!ib_Found)
   {
      ii_FactorCount = ii_TotalFactorCount = 0;
      id_WorkDone = id_TotalWorkDone = 0.0;
      id_TimeSpent = id_TotalTimeSpent = 0.0;
   }
}

UserStats::~UserStats()
{
}

bool     UserStats::FinalizeChanges(void)
{
   if (ib_Found)
      return Update();
   else
      return Insert();
}

bool     UserStats::Update(void)
{
   SQLStatement *updateStatement;
   const char *updateSQL = "update UserStats " \
                           "   set WorkSinceLastReport = ?, " \
                           "       FactorsSinceLastReport = ?, " \
                           "       TimeSinceLastReport = ?, " \
                           "       TotalWorkDone = ?, " \
                           "       TotalFactorsFound = ?, " \
                           "       TotalTime = ? " \
                           " where UserID = ? ";
   bool     success;

   updateStatement = new SQLStatement(ip_Log, ip_DBInterface, updateSQL);
   updateStatement->BindInputParameter(id_WorkDone);
   updateStatement->BindInputParameter(ii_FactorCount);
   updateStatement->BindInputParameter(id_TimeSpent);
   updateStatement->BindInputParameter(id_TotalWorkDone);
   updateStatement->BindInputParameter(ii_TotalFactorCount);
   updateStatement->BindInputParameter(id_TotalTimeSpent);
   updateStatement->BindInputParameter(is_UserID, ID_LENGTH);

   success = updateStatement->Execute();

   delete updateStatement;

   return success;
}

bool     UserStats::Insert(void)
{
   SQLStatement *insertStatement;
   const char *insertSQL = "insert into UserStats " \
                           " ( UserID, WorkSinceLastReport, FactorsSinceLastReport, TimeSinceLastReport, " \
                           "   TotalWorkDone, TotalFactorsFound, TotalTime ) " \
                           "values ( ?,?,?,?,?,?,? ) ";
   bool     success;

   insertStatement = new SQLStatement(ip_Log, ip_DBInterface, insertSQL);
   insertStatement->BindInputParameter(is_UserID, ID_LENGTH);
   insertStatement->BindInputParameter(id_WorkDone);
   insertStatement->BindInputParameter(ii_FactorCount);
   insertStatement->BindInputParameter(id_TimeSpent);
   insertStatement->BindInputParameter(id_TotalWorkDone);
   insertStatement->BindInputParameter(ii_TotalFactorCount);
   insertStatement->BindInputParameter(id_TotalTimeSpent);

   success = insertStatement->Execute();

   delete insertStatement;

   if (success) ib_Found = true;

   return success;
}
