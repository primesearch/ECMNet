/* ServerSocket Class header for ECMNet.

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

#ifndef _ServerSocket_

#define _ServerSocket_

#include "defs.h"
#include "Socket.h"

class ServerSocket : public Socket
{
public:
   ServerSocket(Log *theLog, int32_t portID);

   bool     Open(void);

   // Listen for a message
   bool     AcceptMessage(int32_t &clientSocketID, char *clientAddress);

private:
   int32_t  ii_PortID;
};

#endif // #ifndef _ServerSocket_

