/* UpgradeFromV2 Class header for ECMNet.

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

#ifndef _UpgradeFromV2_

#define _UpgradeFromV2_

#include "defs.h"
#include "server.h"
#include "DBInterface.h"
#include "Log.h"
#include "MasterComposite.h"
#include "Composite.h"
#include "CompositeFactorEvent.h"

class UpgradeFromV2
{
public:
   // Only use this when we expect to find an existing composite
   UpgradeFromV2(Log *theLog, const char *serverID);

   ~UpgradeFromV2();

   void  Start(const char *saveFileName);

private:
   DBInterface     *ip_DBInterface;
   Log             *ip_Log;
   Composite       *ip_Composite;
   MasterComposite *ip_MasterComposite;
   CompositeFactorEvent *ip_CompositeFactorEvent;

   char     ic_PreviousName[NAME_LENGTH+1];
   char     ic_ServerID[ID_LENGTH+1];
   bool     ib_Success;
   int32_t  ii_Successful;
   int32_t  ii_Failed;

   void     CommitPreviousComposite(void);
   void     NewComposite(const char *name, const char *value);
   bool     InsertFactor(const char *factorInfo, int32_t factorEventIndex);
   bool     InsertWorkDone(const char *b1Info);
   bool     ExtractParameters(char *paramInfo);
};

#endif // #ifndef _UpgradeFromV2_
