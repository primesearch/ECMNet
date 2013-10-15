/* IAmASlave Class header for ECMNet.

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

#ifndef  _IAmASlave_
#define  _IAmASlave_

#include "defs.h"
#include "DBInterface.h"
#include "SlaveSocket.h"
#include "Log.h"
#include "MasterComposite.h"
#include "Composite.h"
#include "CompositeFactorEvent.h"

class IAmASlave
{
public:
   IAmASlave(Log *theLog, const char *masterServerName, int32_t masterPortID,
             const char *SlaveID, const char *masterPassword);

   ~IAmASlave(void);

   void           SyncWithMaster(DBInterface *dbInterface);

private:
   DBInterface   *ip_DBInterface;
   SlaveSocket   *ip_Socket;
   Log           *ip_Log;

   string         is_ServerID;
   string         is_MasterPassword;
   MasterComposite      *ip_MasterComposite;
   Composite            *ip_Composite;
   CompositeFactorEvent *ip_CompositeFactorEvent;

   // Routines for slave to send updates to the master
   bool           SendUpdatesToMaster(void);
   bool           SendMasterCompositeToMaster(char *masterCompositeName, const char *masterCompositeValue, bool &fatalError);
   bool           SendWorkToMaster(const char *compositeName);
   bool           SendCompositesToMaster(const char *masterCompositeName);
   bool           SendCompositeFactorEventsToMaster(const char *masterCompositeName, const char *compositeName);
   void           SendCompositeFactorEventDetailsToMaster(const char *masterCompositeName, const char *compositeName, int32_t factorEvent);
   bool           SendUserStatsToMaster(void);

	bool				DeleteMasterCompositeDetails(void);

   // Routines for slave to get updates from the master
   bool           GetUpdatesFromMaster(void);
   bool           HandleMasterCompositeFromMaster(const char *startMessage, char *masterCompositeName, bool &fatalError);
   bool           HandleCompositeFromMaster(const char *theMessage, const char *slaveCompositeName, const char *slaveCompositeValue);
   bool           HandleWorkFromMaster(const char *theMessage);
   bool           HandleCompositeFromMaster(const char *theMessage);
   bool           HandleCompositeFactorEventFromMaster(const char *theMessage);
   bool           HandleCompositeFactorEventDetailFromMaster(const char *theMessage);
   bool           GetUserStatsFromMaster(void);
};

#endif
