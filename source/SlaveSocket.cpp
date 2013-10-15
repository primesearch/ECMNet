/* SlaveSocket Class for ECMNet.

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


#include "SlaveSocket.h"

SlaveSocket::SlaveSocket(Log *theLog, const char *serverName, int32_t portID) : Socket(theLog, "slave")
{
   strcpy(is_ServerName, serverName);
   ii_PortID = portID;
}

bool     SlaveSocket::Open(void)
{
   struct sockaddr_in   sin;
   uint32_t             addr;
   char                *readBuf;

   if (ib_IsOpen)
      return true;

   addr = GetAddress(is_ServerName);
   if (!addr)
      return false;

   // Using IPPROTO_IP is equivalent to IPPROTO_TCP since AF_INET and SOCK_STREAM are used
   ii_SocketID = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);

#ifdef WIN32
   if (ii_SocketID == INVALID_SOCKET)
      return false;
#else
   if (ii_SocketID < 0)
      return false;
#endif

   memset(&sin, 0x00, sizeof(sin));
   sin.sin_family       = AF_INET;
   sin.sin_addr.s_addr  = addr;
   sin.sin_port         = htons((unsigned short) ii_PortID);

   if (connect(ii_SocketID, (struct sockaddr *) &sin, sizeof(sin)) != 0)
   {
      ip_Log->LogMessage("%s:%d connect to socket failed with error %d", is_ServerName, ii_PortID, GetSocketError());
      Close();
      return false;
   }

   ib_IsOpen = true;

   Send("SLAVE %s", ECMNET_VERSION);

   // Verify that we have connected to the server
   readBuf = Receive();

   if (!readBuf || memcmp(readBuf, "Connected to server", 19))
   {
      if (readBuf && !memcmp(readBuf, "ERROR", 5))
         ip_Log->LogMessage(readBuf);
      else
         ip_Log->LogMessage("Could not verify connection to %s.  Will try again later.", is_ServerName);
      Close();
      return false;
   }

   if (sscanf(readBuf, "Connected to server version %s", is_ServerVersion) != 1)
      strcpy(is_ServerVersion, "2.x");

   return true;
}
