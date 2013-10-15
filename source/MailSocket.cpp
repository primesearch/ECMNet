/* MailSocket Class for ECMNet.

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

#include "MailSocket.h"

MailSocket::MailSocket(Log *theLog, const char *serverName, int32_t portID, const char *emailID) : Socket(theLog, "mail")
{
   is_ServerName = new char[strlen(serverName) + 1];
   is_EmailID = new char[strlen(emailID) + 1];

   strcpy(is_ServerName, serverName);
   strcpy(is_EmailID, emailID);

   ii_PortID = portID;
   ib_IsMail = true;
}

bool     MailSocket::Open()
{
   struct sockaddr_in   sin;
   uint32_t             addr;

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
      Close();
      ip_Log->LogMessage("%s:%ld connect to socket failed with error %d", is_ServerName, ii_PortID, GetSocketError());
      return false;
   }

   ib_IsOpen = true;

   return true;
}
