/* Log Class for ECMNet.

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

#include "Log.h"

Log::Log(int32_t logLimit, const char *logFileName, int32_t debugLevel, bool echoToConsole)
{
   ip_FileLock = new SharedMemoryItem(logFileName);

   ii_LogLimit = logLimit;

   is_LogFileName = new char [strlen(logFileName) + 1];
   strcpy(is_LogFileName, logFileName);
   SetDebugLevel(debugLevel);

   ib_EchoToConsole = echoToConsole;
   ib_EchoTest = true;
   ib_UseLocalTime = true;

   is_TempBuffer = new char[10000];

   ib_CROverLF = false;
   ib_NeedLF = false;
}

Log::~Log()
{
   delete ip_FileLock;
   delete [] is_LogFileName;
   delete [] is_TempBuffer;
}

void  Log::LogMessage(const char *fmt, ...)
{
   va_list  args;

   va_start(args, fmt);
   vsprintf(is_TempBuffer, fmt, args);
   va_end(args);

   ip_FileLock->Lock();

   if (ii_LogLimit != -1)
      Write(is_LogFileName, is_TempBuffer, ib_EchoToConsole, true);

   ip_FileLock->Release();

}

void  Log::TestMessage(const char *fmt, ...)
{
   va_list  args;

   va_start(args, fmt);
   vsprintf(is_TempBuffer, fmt, args);
   va_end(args);

   ip_FileLock->Lock();

   if (ii_LogLimit != -1)
      Write(is_LogFileName, is_TempBuffer, ib_EchoTest, true);

   ip_FileLock->Release();
}

void  Log::Debug(int32_t debugLevel, const char *fmt, ...)
{
   va_list  args;
   int32_t  internalDebugLevel = (int32_t) ip_FileLock->GetValueNoLock();

   // Check if we are debugging
   if (internalDebugLevel == 0)
      return;

   if (internalDebugLevel == DEBUG_ALL || (internalDebugLevel == debugLevel))
   {
      ip_FileLock->Lock();

      va_start(args, fmt);
      vsprintf(is_TempBuffer, fmt, args);
      va_end(args);

      Write(is_LogFileName, is_TempBuffer, true, true);

      ip_FileLock->Release();
   }
}

void  Log::Write(const char *fileName, const char *logMessage, int32_t toConsole, int32_t respectLimit)
{
   FILE       *fp;

   struct stat buf;
   char        oldFileName[400];

   if ((fp = fopen(fileName, "at")) == NULL)
   {
      // couldn't open for appending
      if (stat(fileName, &buf) == -1)
      {
         // File doesn't exist
         if ((fp = fopen(fileName, "wt")) == NULL)
         {
            fprintf(stderr, "Couldn't create file [%s].\n", fileName);
            return;
         }
      }
      else 
      {
         fprintf(stderr, "Couldn't append to file [%s].\n", fileName);
         return;
      }
   }

   if (toConsole)
   {
      // If the line begins with a '\n' then make sure the time
      // in inserted after the \n and not before
      if (ib_CROverLF)
         fprintf(stderr, "[%s] %s\r", Time(), logMessage);
      else
      {
         // If the last write had a carriage return and this output is to go onto
         // its own line, then add a line feed before outputting it.
         if (ib_NeedLF) fprintf(stderr, "\n");
         ib_NeedLF = false;

         if (*logMessage == '\n')
            fprintf(stderr, "\n[%s] %s\n", Time(), &logMessage[1]);
         else
            fprintf(stderr, "[%s] %s\n", Time(), logMessage);
      }
      fflush(stderr);
   }

   // If the line begins with a '\n' then make sure the time
   // in inserted after the \n and not before
   if (*logMessage == '\n')
      fprintf(fp, "\n[%s] %s\n", Time(), &logMessage[1]);
   else
      fprintf(fp, "[%s] %s\n", Time(), logMessage);

   fclose(fp);

   if (respectLimit && ii_LogLimit>0)
   {
      // See if the file has grown too big...
      if (stat(fileName, &buf) != -1)
      {
         if (buf.st_size > ii_LogLimit)
         {
            strcpy(oldFileName, fileName);
            strcat(oldFileName, ".old");
            // delete filename.old
            unlink(oldFileName);
            // rename filename to filename.old
            rename((const char *) fileName, (const char *) oldFileName);
         }
      }
   }
}

char *Log::Time()
{
   time_t timenow;
   struct tm *ltm;
   static char timeString[64];
   char   timeZone[100];
   char  *ptr1, *ptr2;

   while (true)
   {
     timenow = time(NULL);

     if (ib_UseLocalTime)
        ltm = localtime(&timenow);
     else
        ltm = gmtime(&timenow);

     if (!ltm)
        continue;

     if (ib_UseLocalTime)
     {
        // This could return a string like "Central Standard Time" or "CST"
        strftime(timeZone, sizeof(timeZone), "%Z", ltm);

        // If there are embedded spaces, this will convert
        // "Central Standard Time" to "CST"
        ptr1 = timeZone;
	     ptr1++;
        ptr2 = ptr1;
        while (true)
        {
           // Find the space
           while (*ptr2 && *ptr2 != ' ')
	           ptr2++;

           if (!*ptr2 || iscntrl(*ptr2)) 
              break;

           // Skip the space
           ptr2++;
           if (!*ptr2 || iscntrl(*ptr2)) 
              break;

           *ptr1 = *ptr2;
           ptr1++;
           *ptr1 = 0;
        }
     }
     else
        strcpy(timeZone, "GMT");

     strftime(timeString, sizeof(timeString), "%Y-%m-%d %X", ltm);
     if (ib_AppendTimeZone)
     {
        strcat(timeString, " ");
        strcat(timeString, timeZone);
     }

     break;
   }

   return timeString;
}
