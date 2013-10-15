/* HelperSocket Class header for ECMNet.

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

#ifndef _HelperSocket_

#define _HelperSocket_

#include "defs.h"
#include "Socket.h"

class HelperSocket : public Socket
{
public:
   HelperSocket(Log *theLog, int32_t socketID, const char *fromAddress, const char *description);

   bool     ParseFirstMessage(const char *theMessage);

   char    *GetFromAddress(void) { return is_FromAddress; };
   char    *GetEmailID(void) { return is_EmailID; };
   char    *GetMachineID(void) { return is_MachineID; };
   char    *GetInstanceID(void) { return is_InstanceID; };
   char    *GetUserID(void) { return is_UserID; };
   char    *GetClientVersion(void) { return is_ClientVersion; };

private:
   char     is_FromAddress[200];

   char     is_EmailID[ID_LENGTH+1];
   char     is_MachineID[ID_LENGTH+1];
   char     is_InstanceID[ID_LENGTH+1];
   char     is_UserID[ID_LENGTH+1];
   char     is_ClientVersion[20];
};

#endif // #ifndef _HelperSocket_

