/* Mail Class for ECMNet.

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

#include "Mail.h"
#include "server.h"
#include "SQLStatement.h"

#define MAIL_BUFFER_SIZE   VALUE_LENGTH + 1000

Mail::Mail(Log *theLog, const char *serverName, uint32_t portID,
           const char *fromEmailID, const char *fromPassword,
           uint32_t destIDCount, char *destID[])
{
   uint32_t  i;

   ip_Log = theLog;
   strcpy(is_FromEmailID, fromEmailID);
   strcpy(is_FromPassword, fromPassword);
   strcpy(is_HostName, "localhost");
   ib_SMTPAuthentication = false;
   ib_SMTPVerification = false;

   ip_Socket = new MailSocket(ip_Log, serverName, portID, fromEmailID);

   ii_DestIDCount = 0;
   for (i=0; i<destIDCount; i++)
   {
      if (strcmp(destID[i], fromEmailID))
      {
         is_DestID[ii_DestIDCount] = new char [strlen(destID[i]) + 2];
         strcpy(is_DestID[ii_DestIDCount], destID[i]);
         ii_DestIDCount++;
      }
   }

   if (ip_Socket->Open())
   {
      ip_Socket->Close();
      ib_Enabled = true;
   }
   else
   {
      printf("Could not open socket with SMTP server at %s.  Mail has been disabled\n", serverName);
      ib_Enabled = false;
   }
}

Mail::~Mail()
{
   uint32_t  i;

   delete ip_Socket;

   for (i=0; i<ii_DestIDCount; i++)
      delete [] is_DestID[i];
}

bool     Mail::NewMessage(const char *toEmailID, const char *subject, ...)
{
   uint32_t rc;
   char     theSubject[MAIL_BUFFER_SIZE];
   va_list  args;

   if (!ib_Enabled)
      return false;

   va_start(args, subject);
   vsprintf(theSubject, subject, args);
   va_end(args);

   if (!ip_Socket->Open())
      return false;

   rc = GetLine();
   if (rc != 220)
   {
      ip_Log->LogMessage("SMTP server error.  Expected 220, got %ld", rc);
      ip_Socket->Close();
      return false;
   }

   if (!PrepareMessage(toEmailID))
   {
      ip_Socket->Close();
      return false;
   }

   if (!SendHeader(toEmailID, theSubject))
   {
      ip_Socket->Close();
      return false;
   }

   return true;
}

void    Mail::AppendLine(int32_t newLines, const char *line, ...)
{
   char     theLine[2000];
   va_list  args;

   va_start(args, line);
   vsprintf(theLine, line, args);
   va_end(args);

   while (newLines-- > 0)
      ip_Socket->SendNewLine();

   ip_Socket->Send("%s  ", theLine);
}

bool     Mail::SendMessage(void)
{
   if (!ip_Socket->GetIsOpen())
      return false;

   ip_Socket->Send(".");

   if (GetLine() != 250)
   {
      ip_Log->LogMessage("SMTP server error.  The message was not accepted by the server");
      ip_Socket->Close();
      return false;
   }

   ip_Socket->Send("QUIT");
   GetLine();

   ip_Socket->Close();
   return true;
}

uint32_t  Mail::GetLine(void)
{
   char   *data;

   data = 0;
   while (!data)
   {
      data = ip_Socket->Receive(2);

      if (!data)
         break;

      if (strstr(data, "ESMTP"))
         ib_SMTPAuthentication = true;
      if (strstr(data, "VRFY"))
         ib_SMTPVerification = true;

      if (strlen(data) < 3 || data[3] == '-')
         data = 0;
   }

   if (!data)
      return 0;

   return atol(data);
}

bool     Mail::SendHeader(const char *toEmailID, const char *theSubject)
{
   uint32_t  i;
   char    theDate[255];
   time_t  theTime;

   theTime = time(NULL);
   strftime(theDate, 80, "%a, %d %b %Y %X %Z", localtime(&theTime));

   ip_Socket->Send("Subject: %s", theSubject);
   ip_Socket->Send("From: %s", is_FromEmailID);
   ip_Socket->Send("To: %s", is_FromEmailID);
   if (strcmp(toEmailID, is_FromEmailID))
      ip_Socket->Send("To: %s", toEmailID);
   for (i=0; i<ii_DestIDCount; i++)
      ip_Socket->Send("To: %s", is_DestID[i]);
   ip_Socket->Send("Date: %s", theDate);

   return true;
}

bool     Mail::SendHeader(const char *header, const char *text, uint32_t expectedRC)
{
   int32_t   rc;
   uint32_t   gotRC;

   if (text)
      rc = ip_Socket->Send("%s %s", header, text);
   else
      rc = ip_Socket->Send("%s", header);

   if (rc < 0)
   {
      ip_Log->LogMessage("SMTP socket error sending '%s'", header);
      return false;
   }

   gotRC = GetLine();
   if (gotRC != expectedRC && gotRC != 252)
   {
      ip_Log->LogMessage("SMTP server error.  Sent '%s', expected %ld, got %ld", header, expectedRC, gotRC);
      return false;
   }

   return true;
}

bool     Mail::PrepareMessage(const char *toEmailID)
{
   uint32_t  i;
   char      encodedEmailID[100], encodedPassword[100];

   if (!ib_SMTPAuthentication)
   {
      if (!SendHeader("HELO", "home.com", 250))
         return false;
   }
   else
   {
      memset(encodedEmailID, 0x00, sizeof(encodedEmailID));
      memset(encodedPassword, 0x00, sizeof(encodedPassword));
      encodeBase64(is_FromEmailID, encodedEmailID);
      encodeBase64(is_FromPassword, encodedPassword);

      if (!SendHeader("EHLO", is_HostName, 250))
         return false;
      if (ib_SMTPVerification)
      {
         if (!SendHeader("VRFY", is_FromEmailID, 250))
            return false;
      }
      else
      {
         if (!SendHeader("AUTH LOGIN", 0, 334))
            return false;
         if (!SendHeader(encodedEmailID, 0, 334))
            return false;
         if (!SendHeader(encodedPassword, 0, 235))
            return false;
      }
   }

   if (!SendHeader("MAIL From:", is_FromEmailID, 250))
      return false;

   if (!SendHeader("RCPT To:", toEmailID, 250))
      return false;

   if (strcmp(is_FromEmailID, toEmailID))
      if (!SendHeader("RCPT To:", is_FromEmailID, 250))
         return false;

   for (i=0; i<ii_DestIDCount; i++)
      if (strcmp(is_DestID[i], toEmailID))
         if (!SendHeader("RCPT To:", is_DestID[i], 250))
            return false;

   if (!SendHeader("DATA", 0, 354))
      return false;

   return true;
}

// This presumes that outString is large enough to hold the encrypted string
// and has been initialized to all zeros
void     Mail::encodeBase64(const char *inString, char *outString)
{
   const char    *base64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
   char     array3[3], array4[4];
   int32_t  i, j, bytes, index;

   i = j = index = 0;
   bytes = strlen(inString);
   while (bytes--)
   {
       array3[i++] = *(inString++);
       if (i == 3)
       {
         array4[0] = (array3[0] & 0xfc) >> 2;
         array4[1] = ((array3[0] & 0x03) << 4) + ((array3[1] & 0xf0) >> 4);
         array4[2] = ((array3[1] & 0x0f) << 2) + ((array3[2] & 0xc0) >> 6);
         array4[3] = array3[2] & 0x3f;

         for (i=0; i<4; i++)
            outString[index++] = base64[(int32_t) array4[i]];
         i = 0;
      }
   }

   if (i)
   {
      for(j=i; j<3; j++)
         array3[j] = '\0';

      array4[0] = (array3[0] & 0xfc) >> 2;
      array4[1] = ((array3[0] & 0x03) << 4) + ((array3[1] & 0xf0) >> 4);
      array4[2] = ((array3[1] & 0x0f) << 2) + ((array3[2] & 0xc0) >> 6);
      array4[3] = array3[2] & 0x3f;

      for (j = 0; (j < i + 1); j++)
         outString[index++] = base64[(int32_t) array4[j]];

      while (i++ < 3)
         outString[index++] = '=';
   }
}
bool     Mail::NotifyUserAboutFactor(DBInterface *dbInterface, const char *compositeName, const char *compositeValue,
                                     int32_t factorEvent, const char *finderEmailID, bool duplicate)
{
   SQLStatement  *selectFactorStatement;
   const char    *selectFactor = "select FactorValue, FactorEventSequence, DecimalLength, " \
                                 "       EmailID, UserID, MachineID, InstanceID, ServerID, " \
                                 "       FactorMethod, FactorStep, PRPTestResult, " \
                                 "       B1, B1Time, B2, B2Time, Sigma " \
                                 "  from CompositeFactorEvent cfe, CompositeFactorEventDetail cfed " \
                                 " where cfe.CompositeName = ? " \
                                 "   and cfe.FactorEvent = ? " \
                                 "   and cfe.CompositeName = cfed.CompositeName " \
                                 "   and cfe.FactorEvent = cfed.FactorEvent " \
                                 " order by FactorEventSequence ";
   char           factorValue[VALUE_LENGTH+1];
   char           emailID[200];
   char           userID[50];
   char           machineID[50];
   char           instanceID[50];
   char           serverID[50];
   char           factorMethod[20];
   char           factorType[20];
   double         b1, b2;
   bool           success, first;
   int32_t        factorEventSequence, factorLength, b1Time, b2Time, factorStep, prpTestResult;
   int64_t        sigma;

   selectFactorStatement = new SQLStatement(ip_Log, dbInterface, selectFactor);
   selectFactorStatement->BindInputParameter(compositeName, NAME_LENGTH);
   selectFactorStatement->BindInputParameter(factorEvent);
   selectFactorStatement->BindSelectedColumn(factorValue, VALUE_LENGTH);
   selectFactorStatement->BindSelectedColumn(&factorEventSequence);
   selectFactorStatement->BindSelectedColumn(&factorLength);
   selectFactorStatement->BindSelectedColumn(emailID, sizeof(emailID));
   selectFactorStatement->BindSelectedColumn(userID, sizeof(userID));
   selectFactorStatement->BindSelectedColumn(machineID, sizeof(machineID));
   selectFactorStatement->BindSelectedColumn(instanceID, sizeof(instanceID));
   selectFactorStatement->BindSelectedColumn(serverID, sizeof(serverID));
   selectFactorStatement->BindSelectedColumn(factorMethod, sizeof(factorMethod));
   selectFactorStatement->BindSelectedColumn(&factorStep);
   selectFactorStatement->BindSelectedColumn(&prpTestResult);
   selectFactorStatement->BindSelectedColumn(&b1);
   selectFactorStatement->BindSelectedColumn(&b1Time);
   selectFactorStatement->BindSelectedColumn(&b2);
   selectFactorStatement->BindSelectedColumn(&b2Time);
   selectFactorStatement->BindSelectedColumn(&sigma);

   success = first = true;
   while (success && selectFactorStatement->FetchRow(false))
   {
      if (first)
         success = NewMessage(finderEmailID, "%c%d factor of %s found by ECMNet!",
                              ((prpTestResult == PRPTEST_AS_COMPOSITE) ? 'C' : 'P'),
                              factorLength, compositeName);

      if (!success)
         break;
      
      strcpy(factorType, "unknown");
      if (prpTestResult == PRPTEST_AS_COMPOSITE) strcpy(factorType, "composite");
      if (prpTestResult == PRPTEST_AS_PRP) strcpy(factorType, "prp");
      if (prpTestResult == PRPTEST_AS_PRIME) strcpy(factorType, "prime");

      if (first)
      {
         ip_Socket->SetAutoNewLine(false);

         AppendLine(0, "A factor was found for %s using GMP-ECM using factor method %s",
                     compositeName, factorMethod);

         if (duplicate)
            AppendLine(0, "The finder listed below has already reported this factor, so you will not get credit for it.");

         ip_Socket->SetAutoNewLine(true);
         AppendLine(2, "   Candidate number: %s", compositeValue);

         AppendLine(0, "   Factor: %s", factorValue);
         AppendLine(0, "   Factor Type: %s", factorType);
         AppendLine(0, "   Factor Length: %d", factorLength);
         first = false;
      }
      else
      {
         AppendLine(0, "   Co-Factor: %s", factorValue);
         AppendLine(0, "   Co-Factor Type: %s", factorType);
         AppendLine(0, "   Co-Factor Length: %d", factorLength);

         first = false;

         AppendLine(0, "   Sigma: %.0lf", sigma);

         if (b1Time > 0.0)
            AppendLine(0, "   B1: %.0lf (%lf seconds)", b1, b1Time);
         else
            AppendLine(0, "   B1: %.0lf", b1);

         if (factorStep == 2)
         {
            if (b2Time > 0.0)
               AppendLine(0, "   B2: %.0lf (%lf seconds)", b2, b2Time);
            else
               AppendLine(0, "   B2: %.0lf", b2);
         }
         
         AppendLine(1, "   Found via machine: %s", machineID);
         AppendLine(1, "   Found via instance: %s", instanceID);
         AppendLine(0, "   Found via server: %s", serverID);
      }
   }

   selectFactorStatement->CloseCursor();

   delete selectFactorStatement;

   if (success)
      SendMessage();

   return success;
}
