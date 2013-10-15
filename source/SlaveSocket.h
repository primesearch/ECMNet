/* SlaveSocket Class header for ECMNet.

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

#ifndef _SlaveSocket_

#define _SlaveSocket_

#include "Socket.h"

class SlaveSocket : public Socket
{
public:
   SlaveSocket(Log *theLog, const char *serverName, int32_t portID);

   bool     Open(void);

   char    *GetServerName(void) { return is_ServerName; };

   int32_t  GetPortID(void) { return ii_PortID; };

   char    *GetServerVersion(void) { return is_ServerVersion; };

private:
   char     is_ServerName[200];

   char     is_ServerVersion[20];

   int32_t  ii_PortID;
};

#endif // #ifndef _SlaveSocket_

