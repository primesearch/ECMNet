/* WorkReceiver Class header for ECMNet.

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

#ifndef _WorkReceiver_

#define _WorkReceiver_

#include "defs.h"
#include "DBInterface.h"
#include "SQLStatement.h"
#include "HelperSocket.h"
#include "Log.h"
#include "MasterComposite.h"
#include "Composite.h"
#include "CompositeFactorEvent.h"
#include "UserStats.h"

class WorkReceiver
{
public:
   WorkReceiver(DBInterface *dbInterface, HelperSocket *theSocket, Log *theLog, const char *serverID, bool recurse);

   ~WorkReceiver();

   void  ProcessMessageOld(const char *theMessage);
   void  ProcessMessage(void);

private:
   DBInterface   *ip_DBInterface;
   HelperSocket  *ip_Socket;
   Log           *ip_Log;
   Composite     *ip_Composite;
   MasterComposite *ip_MasterComposite;
   UserStats       *ip_UserStats;
   CompositeFactorEvent *ip_CompositeFactorEvent;

   int32_t  ii_WorkAccepted;
   int32_t  ii_FactorsAccepted;
   int64_t  il_ReportedDate;
   bool     ib_Recurse;

   void     StartWork(const char *compositeName);
   bool     EndWork(bool success);

   char     is_ClientVersion[10];
   string   is_ServerID;

   bool     ProcessWork(const char *buffer, char *compositeName);
   void     AbandonWork(const char *buffer);
   bool     ProcessFactorEvent(const char *buffer, const char *compositeName);
   bool     ProcessFactorEventDetail(const char *buffer, const char *compositeName);

   bool     ProcessWork(const char *compositeValue, const char *factorMethod,
                        int32_t factorIsNumber, double b1, int32_t curvesDone, double timeSpent);

   void     AddFactor(Log *factorLog, const char *factorValue, const char *factorType);
};

#endif // #ifndef _WorkReceiver_
