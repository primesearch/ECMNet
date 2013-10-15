/* Composite Class for ECMNet.

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

#include "Composite.h"
#include "SQLStatement.h"

Composite::Composite(DBInterface *dbInterface, Log *theLog, const char *compositeName, bool hasBeenVerifiedAgainstMaster)
{
   SQLStatement *selectStatement;
   const char *selectSQL = "select CompositeValue, MasterCompositeName, " \
                           "        FactorEventCount, DecimalLength, LastSendTime " \
                           "  from Composite " \
                           " where CompositeName = ? ";
   char  masterCompositeName[NAME_LENGTH+1];
   char  compositeValue[VALUE_LENGTH+1];

   ip_DBInterface = dbInterface;
   ip_Log = theLog;
   is_CompositeName = compositeName;
   ib_VerifiedMaster = hasBeenVerifiedAgainstMaster;

   selectStatement = new SQLStatement(ip_Log, ip_DBInterface, selectSQL);
   selectStatement->BindInputParameter(compositeName, NAME_LENGTH);
   selectStatement->BindSelectedColumn(compositeValue, VALUE_LENGTH);
   selectStatement->BindSelectedColumn(masterCompositeName, NAME_LENGTH);
   selectStatement->BindSelectedColumn(&ii_FactorEventCount);
   selectStatement->BindSelectedColumn(&ii_DecimalLength);
   selectStatement->BindSelectedColumn(&il_LastSendTime);

   ib_Found = selectStatement->FetchRow(true);

   if (ib_Found)
   {
      is_CompositeValue = compositeValue;
      is_MasterCompositeName = masterCompositeName;
   }
   else
   {
      il_LastSendTime = 0;
      ii_FactorEventCount = 0;
      is_MasterCompositeName = "";
      ii_DecimalLength = 0;
   }

   delete selectStatement;
}

Composite::~Composite()
{
}

bool     Composite::SetValue(const char *compositeValue)
{
   if (ib_Found)
   {
      if (is_CompositeValue == (string) compositeValue)
         return true;

      ip_Log->LogMessage("Error: Trying to override composite value for %s", is_CompositeName.c_str());
      ip_Log->LogMessage("old <%s>", is_CompositeValue.c_str());
      ip_Log->LogMessage("new <%s>", compositeValue);

      return false;
   }

   is_CompositeValue = compositeValue;

   if (!ib_VerifiedMaster || !ii_DecimalLength)
      ComputeDecimalLengthAndPRPTest();

   return true;
}

bool     Composite::SetMasterCompositeName(string masterCompositeName)
{
   if (!ib_Found)
   {
      is_MasterCompositeName = masterCompositeName;
      ib_VerifiedMaster = true;
      return true;
   }

   if (masterCompositeName != is_MasterCompositeName)
   {
      ip_Log->LogMessage("Error: The master composite for %s does not match the expect master (%s != %s)",
                         is_CompositeName.c_str(), is_MasterCompositeName.c_str(), masterCompositeName.c_str());

      return false;
   }

   ib_VerifiedMaster = true;

   return true;
}

bool     Composite::FinalizeChanges(void)
{
   SQLStatement  *selectStatement;
   const char    *selectSQL = "select sum(LocalCurves * B1) " \
                              "  from MasterCompositeWorkDone " \
                              " where MasterCompositeName = ? ";
   double   totalWorkDone = 0.0;
   bool     success;

   if (ii_DecimalLength == 0)
   {
      ip_Log->LogMessage("Error: Composite has no length %s", is_CompositeName.c_str());
      return false;
   }

   if (!ib_VerifiedMaster)
   {
      ip_Log->LogMessage("Error: The master composite for %s has not been verified", is_CompositeName.c_str());
      return false;
   }

   selectStatement = new SQLStatement(ip_Log, ip_DBInterface, selectSQL);
   selectStatement->BindInputParameter(is_MasterCompositeName, NAME_LENGTH);
   selectStatement->BindSelectedColumn(&totalWorkDone);
   success = selectStatement->FetchRow(true);
   delete selectStatement;

   if (!success) return false;

   id_Difficulty = (double) ii_DecimalLength;
   id_Difficulty = id_Difficulty * id_Difficulty * totalWorkDone;

   if (ib_Found)
      return Update();
   else
      return Insert();
}

bool     Composite::Update(void)
{
   SQLStatement *updateStatement;
   const char   *updateSQL = "update Composite " \
                             "   set FactorEventCount = ?, " \
                             "       DecimalLength = ?, " \
                             "       Difficulty = ?, " \
                             "       LastSendTime = ? " \
                             " where CompositeName = ? ";
   bool     success;

   updateStatement = new SQLStatement(ip_Log, ip_DBInterface, updateSQL);
   updateStatement->BindInputParameter(ii_FactorEventCount);
   updateStatement->BindInputParameter(ii_DecimalLength);
   updateStatement->BindInputParameter(id_Difficulty);
   updateStatement->BindInputParameter(il_LastSendTime);
   updateStatement->BindInputParameter(is_CompositeName, NAME_LENGTH);

   success = updateStatement->Execute();

   delete updateStatement;

   return success;
}

bool     Composite::Insert(void)
{
   SQLStatement *insertStatement;
   const char   *insertSQL = "insert into Composite " \
                             "( CompositeName, CompositeValue, " \
                             "  DecimalLength, Difficulty, FactorEventCount, " \
                             "  LastSendTime, MasterCompositeName ) " \
                             "values ( ?,?,?,?,?,?,? ) ";
   bool     success;

   insertStatement = new SQLStatement(ip_Log, ip_DBInterface, insertSQL);
   insertStatement->BindInputParameter(is_CompositeName, NAME_LENGTH);
   insertStatement->BindInputParameter(is_CompositeValue, VALUE_LENGTH);
   insertStatement->BindInputParameter(ii_DecimalLength);
   insertStatement->BindInputParameter(id_Difficulty);
   insertStatement->BindInputParameter(ii_FactorEventCount);
   insertStatement->BindInputParameter(il_LastSendTime);
   insertStatement->BindInputParameter(is_MasterCompositeName, NAME_LENGTH);

   success = insertStatement->Execute();

   delete insertStatement;

   if (success) ib_Found = true;

   return success;
}

void     Composite::ComputeDecimalLengthAndPRPTest(void)
{
   ii_DecimalLength = eval_str(is_CompositeValue.c_str(), &ii_PRPTestResult);
}
