/* UserStats Class header for ECMNet.

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

#ifndef _UserStats_

#define _UserStats_

#include "defs.h"
#include "server.h"
#include "DBInterface.h"
#include "Log.h"

class UserStats
{
public:
   // Only use this when we expect to find an existing composite
   UserStats(DBInterface *dbInterface, Log *theLog, string userID);

   ~UserStats();

   void     IncrementWorkDone(double b1, int32_t curvesDone) { id_WorkDone += (b1 * (double) curvesDone); id_TotalWorkDone += (b1 * (double) curvesDone); };
   void     IncrementWorkDone(double workDone) { id_WorkDone += workDone; id_TotalWorkDone += workDone; };
   void     IncrementTimeSpent(double timeSpent) { id_TimeSpent += timeSpent; id_TotalTimeSpent += timeSpent; };
   void     IncrementFactorCount(int32_t factorCount) { ii_FactorCount += factorCount; ii_TotalFactorCount += factorCount; };
   void     IncrementFactorCount(void) { ii_FactorCount++; ii_TotalFactorCount++; };

   void     SetTotalWorkDone(double workDone) { id_TotalWorkDone = workDone; id_WorkDone = 0.0; };
   void     SetTotalTimeSpent(double timeSpent) { id_TotalTimeSpent = timeSpent; id_TimeSpent = 0.0; };
   void     SetTotalFactorCount(int32_t factorCount) { ii_TotalFactorCount = factorCount; ii_FactorCount = 0; };

   bool     FinalizeChanges(void);

private:
   DBInterface   *ip_DBInterface;
   Log           *ip_Log;

   bool           ib_Found;
   string         is_UserID;
   int32_t        ii_FactorCount;
   int32_t        ii_TotalFactorCount;
   double         id_WorkDone;
   double         id_TotalWorkDone;
   double         id_TimeSpent;
   double         id_TotalTimeSpent;

   bool     Insert();
   bool     Update();
};

#endif // #ifndef _UserStats_
