/* HelperSocket Class for ECMNet.

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

#include "HelperSocket.h"

HelperSocket::HelperSocket(Log *theLog, int32_t socketID, const char *fromAddress, const char *description) : Socket(theLog, description)
{
   ib_IsOpen = true;
   ii_SocketID = socketID;
   strcpy(is_FromAddress, fromAddress);
   is_ClientVersion[0] = 0;
}

bool  HelperSocket::ParseFirstMessage(const char *theMessage)
{
   int32_t countScanned;
   char   *pos;

   if (memcmp(theMessage, "FROM ", 5) && memcmp(theMessage, "SLAVE ", 6))
   {
      Send("Expected a FROM message, but received <%s>.  The connected was dropped", theMessage);
      ip_Log->LogMessage("%d: Expected a FROM message, but received <%s>.  The connected was dropped", ii_SocketID, theMessage);
      return false;
   }

   if (strlen(theMessage) > 200)
   {
      Send("The message is too long to be parsed.  The connected was dropped");
      ip_Log->LogMessage("%d: The message is too long to be parsed.  The connected was dropped", ii_SocketID);
      return false;
   }

   is_MachineID[0] = 0;
   if (!memcmp(theMessage, "FROM ", 5))
   {
      // In the future clients will send the version as the first parameter
      if (!memcmp(theMessage, "FROM 3.", 7))
      {
         countScanned = sscanf(theMessage, "FROM %s %s %s %s %s", is_ClientVersion, is_EmailID, is_MachineID, is_InstanceID, is_UserID);
         if (countScanned != 5)
            countScanned = sscanf(theMessage, "FROM %s %s %s %s", is_ClientVersion, is_EmailID, is_InstanceID, is_UserID);
      }
      else
         countScanned = sscanf(theMessage, "FROM %s %s %s %s", is_EmailID, is_InstanceID, is_UserID, is_ClientVersion);

      if (countScanned != 4 && countScanned != 5)
      {
         countScanned = sscanf(theMessage, "FROM %s", is_EmailID);
         if (countScanned == 1)
         {
            strcpy(is_ClientVersion, "2.x");

            pos = strchr(is_EmailID, ':');
            if (pos)
            {
               strcpy(is_InstanceID, pos+1);
               *pos = 0;
            }
            else
               strcpy(is_InstanceID, "n/a");

            strcpy(is_UserID, is_EmailID);
            pos = strchr(is_UserID, '@');
            if (pos) *pos = 0;
         }

         if (countScanned != 1)
         {
            Send("Could not parse the FROM message: <%s>.  The connected was dropped", theMessage);
            ip_Log->LogMessage("%d: Could not parse the FROM message: <%s>.  The connected was dropped", ii_SocketID, theMessage);
            return false;
         }
      }
   }
   else
   {
      if (sscanf(theMessage, "SLAVE %s", is_ClientVersion) != 1)
      {
         Send("Could not parse the SLAVE message: <%s>.  The connected was dropped", theMessage);
         ip_Log->LogMessage("%s: Could not parse the SLAVE message: <%s>.  The connected was dropped", ii_SocketID, theMessage);
         return false;
      }
   }

   if (*is_MachineID == 0)
      strcpy(is_MachineID, is_InstanceID);

   return true;
}
