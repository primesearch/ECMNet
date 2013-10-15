/* CompositeFactorEvent Class for ECMNet.

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

#include "CompositeFactorEvent.h"
#include "SQLStatement.h"

CompositeFactorEvent::CompositeFactorEvent(DBInterface *dbInterface, Log *theLog, string compositeName,
                                           int32_t factorEvent, int64_t reportedDate)
{
   ip_DBInterface = dbInterface;
   ip_Log = theLog;
   is_CompositeName = compositeName;
   ii_FactorEvent = factorEvent;

   is_EmailID = "";
   is_UserID = "";
   is_MachineID = "";
   is_InstanceID = "";
   is_ServerID = "";
   ib_SentToMaster = false;
   id_B1 = id_B1Time = id_B2 = id_B2Time = 0.0;
   il_Sigma = il_X0 = 0;
   ii_FactorStep = 0;
   il_ReportedDate = reportedDate;

   ii_FactorEventDetailCount = 0;
}

CompositeFactorEvent::~CompositeFactorEvent()
{
}

void  CompositeFactorEvent::SetIDs(string emailID, string userID, string machineID, string instanceID, string serverID)
{
   is_EmailID = emailID;
   is_UserID = userID;
   is_MachineID = machineID;
   is_InstanceID = instanceID;
   is_ServerID = serverID;
}

void  CompositeFactorEvent::SetCurveDetail(const char *factorMethod, double b1)
{
   is_FactorMethod = factorMethod;
   id_B1 = b1;
}

void  CompositeFactorEvent::SetCurveDetail(const char *factorMethod, int32_t factorStep,
                                           double b1, double b1Time,
                                           double b2, double b2Time,
                                           int64_t sigma, int64_t x0)
{
   is_FactorMethod = factorMethod;
   ii_FactorStep = factorStep;
   id_B1 = b1;
   id_B1Time = b1Time;
   id_B2 = b2;
   id_B2Time = b2Time;
   il_Sigma = sigma;
   il_X0 = x0;
}

bool  CompositeFactorEvent::FinalizeChanges(void)
{
   if (InsertEvent())
      return InsertEventDetails();
   else
      return false;
}

bool  CompositeFactorEvent::InsertEvent(void)
{
   SQLStatement *insertFactorStatement;
   const char *insertSQL = "insert into CompositeFactorEvent " \
                           " ( CompositeName, FactorEvent, ReportedDate, " \
                           "   EmailID, UserID, MachineID, InstanceID, ServerID, " \
                           "   SentToMaster, SentEmail, Duplicate, FactorMethod, FactorStep, " \
                           "   B1, B1Time, B2, B2Time, Sigma, X0 ) " \
                           "values ( ?,?,?,?,?,?,?,?,?,0,?,?,?,?,?,?,?,?,? ) ";
   bool  success = true;

   if (!ii_FactorEventDetailCount)
   {
      ip_Log->LogMessage("No factors were added for this event.");
      return false;
   }

   insertFactorStatement = new SQLStatement(ip_Log, ip_DBInterface, insertSQL);
   insertFactorStatement->BindInputParameter(is_CompositeName, NAME_LENGTH);
   insertFactorStatement->BindInputParameter(ii_FactorEvent);
   insertFactorStatement->BindInputParameter(il_ReportedDate);
   insertFactorStatement->BindInputParameter(is_EmailID, ID_LENGTH);
   insertFactorStatement->BindInputParameter(is_UserID, ID_LENGTH);
   insertFactorStatement->BindInputParameter(is_MachineID, ID_LENGTH);
   insertFactorStatement->BindInputParameter(is_InstanceID, ID_LENGTH);
   insertFactorStatement->BindInputParameter(is_ServerID, ID_LENGTH);
   insertFactorStatement->BindInputParameter((ib_SentToMaster ? 1 : 0));
   insertFactorStatement->BindInputParameter((ib_DuplicateEvent ? 1 : 0));
   insertFactorStatement->BindInputParameter(is_FactorMethod, METHOD_LENGTH);
   insertFactorStatement->BindInputParameter(ii_FactorStep);
   insertFactorStatement->BindInputParameter(id_B1);
   insertFactorStatement->BindInputParameter(id_B1Time);
   insertFactorStatement->BindInputParameter(id_B2);
   insertFactorStatement->BindInputParameter(id_B2Time);
   insertFactorStatement->BindInputParameter(il_Sigma);
   insertFactorStatement->BindInputParameter(il_X0);

   success = insertFactorStatement->Execute();

   delete insertFactorStatement;

   return success;
}

void  CompositeFactorEvent::AddEventDetail(const char *factorValue)
{
   int32_t  prpTestResult, decimalLength;

   decimalLength = eval_str(factorValue, &prpTestResult);
   
   AddEventDetail(factorValue, decimalLength, prpTestResult);
}

void  CompositeFactorEvent::AddEventDetail(const char *factorValue, int32_t decimalLength, int32_t prpTestResult)
{
   SQLStatement *selectStatement;
   const char *selectSQL = "select FactorEvent " \
                           "  from CompositeFactorEventDetail " \
                           " where CompositeName = ? " \
                           "   and FactorValue = ? ";
   int32_t  factorEvent;

   selectStatement = new SQLStatement(ip_Log, ip_DBInterface, selectSQL);
   selectStatement->BindSelectedColumn(&factorEvent);
   selectStatement->BindInputParameter(is_CompositeName, NAME_LENGTH);
   selectStatement->BindInputParameter(factorValue, VALUE_LENGTH);

   if (selectStatement->FetchRow(true) && factorEvent > 0)
      ib_DuplicateEvent = true;
   else
      ib_DuplicateEvent = false;

   delete selectStatement;

   ip_Factors[ii_FactorEventDetailCount].factorValue = factorValue;
   ip_Factors[ii_FactorEventDetailCount].decimalLength = decimalLength;
   ip_Factors[ii_FactorEventDetailCount].prpTestResult = prpTestResult;
   ii_FactorEventDetailCount++;
}

bool  CompositeFactorEvent::InsertEventDetails(void)
{
   SQLStatement *insertFactorStatement;
   const char *insertSQL = "insert into CompositeFactorEventDetail " \
                           " ( CompositeName, FactorEvent, FactorEventSequence, " \
                           "   FactorValue, DecimalLength, PRPTestResult, IsRecursed ) " \
                           "values ( ?,?,?,?,?,?,0 ) ";
   bool     success = true;
   int32_t  index;

   insertFactorStatement = new SQLStatement(ip_Log, ip_DBInterface, insertSQL);
   insertFactorStatement->BindInputParameter(is_CompositeName, NAME_LENGTH);
   insertFactorStatement->BindInputParameter(ii_FactorEvent);
   insertFactorStatement->BindInputParameter(ii_FactorEventDetailCount);
   insertFactorStatement->BindInputParameter(ip_Factors[0].factorValue, VALUE_LENGTH);
   insertFactorStatement->BindInputParameter(ip_Factors[0].decimalLength);
   insertFactorStatement->BindInputParameter(ip_Factors[0].prpTestResult);

   for (index=0; index<ii_FactorEventDetailCount; index++)
   {
      insertFactorStatement->SetInputParameterValue(is_CompositeName, true);
      insertFactorStatement->SetInputParameterValue(ii_FactorEvent);
      insertFactorStatement->SetInputParameterValue(index+1);
      insertFactorStatement->SetInputParameterValue(ip_Factors[index].factorValue);
      insertFactorStatement->SetInputParameterValue(ip_Factors[index].decimalLength);
      insertFactorStatement->SetInputParameterValue(ip_Factors[index].prpTestResult);

      success = insertFactorStatement->Execute();
   }

   delete insertFactorStatement;

   return success;
}
