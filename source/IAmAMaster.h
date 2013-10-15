/* IAmAMaster Class header for ECMNet.

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

#ifndef  _IAmAMaster_
#define  _IAmAMaster_

#include "defs.h"
#include "DBInterface.h"
#include "HelperSocket.h"
#include "Log.h"
#include "MasterComposite.h"
#include "Composite.h"
#include "CompositeFactorEvent.h"

class IAmAMaster
{
public:
   IAmAMaster(DBInterface *dbInterface, HelperSocket *theSocket, Log *theLog,
              const char *serverID, const char *expectedPassword, bool iAmASlave);

   ~IAmAMaster(void);

   void           SyncWithSlave(const char *theMessage);

private:
   DBInterface   *ip_DBInterface;
   HelperSocket  *ip_Socket;
   Log           *ip_Log;

   char           ic_ServerID[ID_LENGTH+1];
   char           ic_ExpectedPassword[100];
   bool           ib_IAmASlave;
   MasterComposite      *ip_MasterComposite;
   Composite            *ip_Composite;
   CompositeFactorEvent *ip_CompositeFactorEvent;

   bool           PopulateMasterSlaveSync(const char *slaveID);

   // Routines for master to get updates from a slave
   bool           GetUpdatesFromSlave(const char *slaveID);

   bool           StartMasterCompositeSync(const char *theMessage, char *masterCompositeName);
   bool           EndMasterCompositeSync(bool success, const char *slaveID);

   bool           SyncMasterCompositeFromSlave(const char *masterCompositeName, const char *slaveID, bool &fatalError);

   bool           HandleWorkFromSlave(const char *theMessage);
   bool           HandleCompositeFromSlave(const char *theMessage);
   bool           HandleCompositeFactorEventFromSlave(const char *theMessage, const char *slaveID);
   bool           HandleCompositeFactorEventDetailFromSlave(const char *theMessage);
   bool           GetUserStatsFromSlave(void);

   // Routines for master to send updates to a slave
   bool           SendUpdatesToSlave(const char *slaveID);
   bool           SendMasterCompositeToSlave(const char *masterCompositeName, const char *slaveID, bool &fatalError);
   void           SendWorkToSlave(const char *masterCompositeName);
   void           SendCompositesToSlave(const char *masterCompositeName, const char *slaveID);
   void           SendCompositeFactorEventsToSlave(const char *masterCompositeName, const char *compositeName);
   void           SendCompositeFactorEventDetailsToSlave(const char *masterCompositeName, const char *compositeName, int32_t factorEvent);
   bool           SendUserStatsToSlave(void);
};

#endif
