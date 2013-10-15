/* MasterComposite Class header for ECMNet.

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

#ifndef _MasterComposite_

#define _MasterComposite_

#include "defs.h"
#include "server.h"
#include "DBInterface.h"
#include "Log.h"
#include "SharedMemoryItem.h"

class MasterComposite
{
public:
   // Only use this when we expect to find an existing composite
   MasterComposite(DBInterface *dbInterface, Log *theLog, string masterCompositeName);

   ~MasterComposite();

   // Prevent other threads from updating this master composite
   void     Lock(const char *serverID);

   bool     WasFound(void) { return ib_Found; };
   int32_t  GetLength(void) { return ii_DecimalLength; };
   int32_t  GetFactorCount(void) { return ii_FactorCount; };
   string   GetName(void) { return is_MasterCompositeName; };
   string   GetValue(void) { return is_MasterCompositeValue; };
   bool     IsActive(void) { return ib_Active; };

   void     SetActive(bool isActive) { ib_Active = isActive; };
   bool     SetValue(const char *masterCompositeValue);
   void     SetLastUpdateTime(void) { il_LastUpdateTime = (int64_t) time(NULL); };
   void     SetSendUpdatesToMaster(bool sendUpdates) { ib_SendUpdatesToMaster = sendUpdates; };
   void     IncrementFactorCount(void) { ii_FactorCount++; };
   void     ResetFactorCount(void) { ii_FactorCount = 0; };

   // Called by IAmASlave to set the work done by the master
   bool     InsertWorkDoneFromMaster(const char *factorMethod, double b1, int32_t curvesDone);

   // Called by UpgradeFromV2
   bool     AddWorkDone(const char *factorMethod, double b1,  int32_t toMasterCurvesDone, int32_t curvesDone);

   // Called elsewhere to set the work done by clients attached to this server
   bool     AddWorkDone(const char *factorMethod, double b1, int32_t curvesDone);

   bool     SendUpdatesToAllSlaves();
   bool     SentUpdateToSlave(const char *slaveID);

   bool     UpdateIfFullyFactored(void);

   bool     FinalizeChanges(void);

private:
   DBInterface   *ip_DBInterface;
   Log           *ip_Log;
   SharedMemoryItem *ip_Locker;

   string         is_MasterCompositeName;
   string         is_MasterCompositeValue;
   int32_t        ii_FactorCount;
   int32_t        ii_RecurseIndex;
   int32_t        ii_DecimalLength;
   int64_t        il_LastUpdateTime;
   bool           ib_Found;
   bool           ib_FullyFactored;
   bool           ib_Active;
   bool           ib_SendUpdatesToMaster;
   double         id_TotalWorkDone;

   void     ComputeDecimalLength(void);

   bool     Insert(void);
   bool     Update(void);
};

#endif // #ifndef _MasterComposite_
