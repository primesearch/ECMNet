/* CompositeFactorEvent Class header for ECMNet.

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

#ifndef _CompositeFactorEvent_

#define _CompositeFactorEvent_

#include "defs.h"
#include "server.h"
#include "DBInterface.h"
#include "Log.h"
#include "MasterComposite.h"

#define MAX_FACTORS  4

typedef struct
{
   string   factorValue;
   int32_t  decimalLength;
   int32_t  prpTestResult;
} cfeDetail_t;

class CompositeFactorEvent
{
public:
   CompositeFactorEvent(DBInterface *dbInterface, Log *theLog, string compositeName,
                        int32_t factorEvent, int64_t reportedDate);

   ~CompositeFactorEvent();

   void     SetSentToMaster(bool sentToMaster) { ib_SentToMaster = sentToMaster; };
   void     SetIDs(string emailID, string userID, string machineID, string instanceID, string serverID);
   void     SetCurveDetail(const char *factorMethod, double b1);
   void     SetCurveDetail(const char *factorMethod, int32_t factorStep,
                           double b1, double b1Time,
                           double b2, double b2Time,
                           int64_t sigma, int64_t x0);
   void     SetSigma(int64_t sigma) { il_Sigma = sigma; };

   bool     IsDuplicateEvent(void) { return ib_DuplicateEvent; };
   int32_t  GetFactorCount(void) { return ii_FactorEventDetailCount; };
   bool     FinalizeChanges(void);

   void     AddEventDetail(const char *factorValue);
   void     AddEventDetail(const char *factorValue, int32_t decimalLength, int32_t prpTestResult);

private:
   DBInterface   *ip_DBInterface;
   Log           *ip_Log;
   cfeDetail_t    ip_Factors[MAX_FACTORS];

   string         is_CompositeName;
   string         is_FactorValue;
   string         is_FactorMethod;
   string         is_EmailID;
   string         is_UserID;
   string         is_MachineID;
   string         is_InstanceID;
   string         is_ServerID;

   double         id_B1;
   double         id_B1Time;
   double         id_B2;
   double         id_B2Time;

   int64_t        il_ReportedDate;
   int64_t        il_Sigma;
   int64_t        il_X0;
   int32_t        ii_FactorStep;
   int32_t        ii_FactorEvent;
   int32_t        ii_FactorEventDetailCount;

   bool           ib_SentToMaster;
   bool           ib_DuplicateEvent;

   bool           InsertEvent(void);
   bool           InsertEventDetails(void);
};

#endif // #ifndef _CompositeFactorEvent_
