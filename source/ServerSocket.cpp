/* ServerSocket Class for ECMNet.

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


#include "ServerSocket.h"

ServerSocket::ServerSocket(Log *theLog, int32_t portID) : Socket(theLog, "server")
{
   ii_PortID = portID;
   ip_Log = theLog;
}

bool     ServerSocket::Open()
{
   struct sockaddr_in   sin;
   int                  mytrue = 1;

   // Using IPPROTO_IP is equivalent to IPPROTO_TCP since AF_INET and SOCK_STREAM are used
   ii_SocketID = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);

#ifdef WIN32
   if (ii_SocketID == INVALID_SOCKET)
      return false;
#else
   if (ii_SocketID < 0)
      return false;
#endif

   // Try to reuse the socket
   if (setsockopt(ii_SocketID, SOL_SOCKET, SO_REUSEADDR, (char*)&mytrue, sizeof(mytrue)) != 0)
   {
      Close();
      ip_Log->LogMessage("Port %d: setsockopt(SO_REUSEADDR) failed", ii_PortID);
      return false;
   }

   // bind the socket to address and port given
   memset(&sin, 0x00, sizeof(sin));
   sin.sin_family       = AF_INET;
   sin.sin_addr.s_addr  = inet_addr("0.0.0.0");
   sin.sin_port         = htons((unsigned short) ii_PortID);

   if (bind(ii_SocketID, (struct sockaddr *) &sin, sizeof(sin)) != 0)
   {
      ip_Log->LogMessage("Port %d: bind on socket failed", ii_PortID);
      return false;
   }

   // set up to receive connections on this socket
   if (listen(ii_SocketID, 999) != 0)
   {
      Close();
      ip_Log->LogMessage("Port %d: listen on socket failed", ii_PortID);
      return false;
   }

   ip_Log->Debug(DEBUG_SOCKET, "Port %d: Listening on socket %d", ii_PortID, ii_SocketID);
   ib_IsOpen = true;

   return true;
}

bool     ServerSocket::AcceptMessage(int32_t &clientSocketID, char *clientAddress)
{
   struct sockaddr_in   sin;
   int32_t              socketID;
#if defined(WIN32)
   int                  sinlen;
#else
   socklen_t            sinlen;
#endif

   sinlen = sizeof(struct sockaddr);
   socketID = accept(ii_SocketID, (struct sockaddr *) &sin, &sinlen);

#ifdef WIN32
   if (socketID == INVALID_SOCKET)
      return false;
#else
   if (socketID < 0)
      return false;
#endif

   ip_Log->Debug(DEBUG_SOCKET, "%d: client connecting from %s", socketID, inet_ntoa(sin.sin_addr));

   ib_IsOpen = true;
   ib_IsReadBuffering = false;
   ib_IsSendBuffering = false;

   strcpy(clientAddress, inet_ntoa(sin.sin_addr));
   clientSocketID = socketID;

   return true;
}
