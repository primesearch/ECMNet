/* Mail Class header for ECMNet.

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

#ifndef _Mail_

#define _Mail_

#include "defs.h"
#include "Log.h"
#include "MailSocket.h"
#include "DBInterface.h"

class Mail
{
public:
   Mail(Log *theLog, const char *serverName, uint32_t portID,
        const char *fromEmailID, const char *fromPassword,
        uint32_t destIDCount, char *destID[]);

   ~Mail();

   bool     NotifyUserAboutFactor(DBInterface *dbInterface, const char *compositeName, const char *compositeValue,
                                  int32_t factorEvent, const char *finderEmailID, bool duplicate);

private:
   uint32_t GetLine(void);

   bool     SendHeader(const char *toEmailID, const char *theSubject);

   bool     SendHeader(const char *header, const char *text, uint32_t expectedRC);

   bool     PrepareMessage(const char *toEmailID);

   void     encodeBase64(const char *inString, char *outString);

   bool     NewMessage(const char *toEmailID, const char *subject, ...);

   void     AppendLine(int32_t newLines, const char *fmt, ...);

   bool     SendMessage(void);

   Log     *ip_Log;

   MailSocket  *ip_Socket;

   char     is_HostName[255];

   char     is_FromEmailID[100];

   char     is_FromPassword[100];

   uint32_t ii_DestIDCount;

   char    *is_DestID[10];

   // This is set to 1 if a socket could be opened with the SMTP server
   bool     ib_Enabled;

   bool     ib_SMTPAuthentication;

   bool     ib_SMTPVerification;
};

#endif // #ifndef _Mail_

