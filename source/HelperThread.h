/* HelperThread Class header for ECMNet.

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

#ifndef _HelperThread_

#define _HelperThread_

#include "defs.h"
#include "server.h"
#include "Thread.h"
#include "HelperSocket.h"
#include "Log.h"
#include "DBInterface.h"

class HelperThread : public Thread
{
public:
   void     StartThread(globals_t *theGlobals, HelperSocket *theSocket);

   void     HandleRequest(void);

   void     ReleaseResources(void);

private:
   Log              *ip_Log;
   HelperSocket     *ip_Socket;
   DBInterface      *ip_DBInterface;
   SharedMemoryItem *ip_ClientCounter;
   globals_t        *ip_Globals;

   char       *is_HTMLTitle;
   char       *is_ProjectName;
   char       *is_AdminPassword;

   void        SendHTML(const char *theMessage);
   bool        VerifyAdminPassword(const char *password);
   void        SendFile(const char *fileName);
   void        SendWork(const char *workString);
   void        ReceiveCompletedWork(const char *clientVersion);
   void        ChangeStatus(const char *theMessage, bool makeActive);
   void        AddComposites(const char *thePassword);
   void        AddCurves(const char *thePassword);
};

#endif // #ifndef _HelperThread_

