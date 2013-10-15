/* ClientSocket Class header for ECMNet.

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

#ifndef _ClientSocket_

#define _ClientSocket_

#include "Socket.h"

class ClientSocket : public Socket
{
public:
   ClientSocket(Log *theLog, const char *serverName, int32_t portID);

   bool     Open(const char *emailID, const char *userID, const char *machineID, const char *instanceID);

   char    *GetServerName(void) { return is_ServerName; };

   int32_t  GetPortID(void) { return ii_PortID; };

   char    *GetEmailID(void) { return is_EmailID; };
   char    *GetMachineID(void) { return is_MachineID; };
   char    *GetInstanceID(void) { return is_InstanceID; };
   char    *GetUserID(void) { return is_UserID; };
   char    *GetServerVersion(void) { return is_ServerVersion; };
   void     SetServerVersion(char *serverVersion) { strcpy(is_ServerVersion, serverVersion); };

private:
   char     is_ServerName[200];
   int32_t  ii_PortID;

   char     is_EmailID[200];
   char     is_MachineID[200];
   char     is_InstanceID[200];
   char     is_UserID[200];
   char     is_ServerVersion[20];
};

#endif // #ifndef _ClientSocket_

