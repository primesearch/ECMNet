/* WorkUnit Class for ECMNet.

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

#include <ctype.h>
#include <signal.h>
#include "WorkUnit.h"

WorkUnit::WorkUnit(Log *theLog, const char *machineID, const char *instanceID, const char *suffix,
                   const char *gmpExe, int32_t kValue, int32_t maxmemValue)
{
   ip_Log = theLog;
   
   strcpy(is_MachineID, machineID);
   strcpy(is_Instance, instanceID);
   strcpy(is_GMPExe, gmpExe);
   strcpy(is_Suffix, suffix);

   im_Next = 0;
   ii_KValue = kValue;
   ii_MaxmemValue = maxmemValue;

   sprintf(is_InFileName, "wu_%s_%s_%s.in", is_Suffix, is_MachineID, is_Instance);
   sprintf(is_OutFileName, "wu_%s_%s_%s.out", is_Suffix, is_MachineID, is_Instance);

   is_CompositeName[0] = 0;
   id_B1 = id_B2 = 0.;
   id_B1Time = id_B2Time = 0;
   id_FactorB1Time = id_FactorB2Time = 0;
   ii_CurvesDone = 0;
   ib_FactorIsNumber = false;
   is_Command[0] = 0;
   ib_HaveFactor = false;
}

WorkUnit::~WorkUnit()
{
   unlink(is_InFileName);
   unlink(is_OutFileName);
}

double   WorkUnit::DoCurve(bool offline)
{
   int      curveCompleted;
   FILE    *fp;

   // Return non-zero to differentiate between doing no WorkUnit and an interrupt
   if (!CanDoAnotherCurve(offline))
      return 0.00001;

   if ((fp = fopen(is_InFileName, "wt")) == NULL)
   {
      ip_Log->LogMessage("%s: Could not file [%s] for writing.  Exiting program", is_Suffix, is_InFileName);
      exit(-1);
   }
   fprintf(fp, "%s", is_CompositeValue);
   fclose(fp);

   SetUpCall();

   system(is_Command);

   curveCompleted = ParseCurveOutput();

   unlink(is_InFileName);
   unlink(is_OutFileName);

   if (!curveCompleted)
   {
      ip_Log->LogMessage("%s: The current curve was not completed.  Assuming user stopped with ^C", is_Suffix);
#ifdef WIN32
      SetQuitting(1);
#else
      raise(SIGINT);
#endif

      return 0.0;
   }
   else
      ii_CurvesDone++;

   return id_B1;
}

bool  WorkUnit::CanDoAnotherCurve(bool offline)
{
   if (ib_FactorIsNumber)
      return false;

   if (ib_HaveFactor)
      return false;

   if (id_B1 == 0.)
      return false;

   if (!offline && ii_CurvesDone >= ii_MaxCurves)
      return false;

   // Never do more than one curve for P-1
   if (!memcmp(is_FactorMethod, "P-1", 3) && ii_CurvesDone > 0)
      return false;

   return true;
}

bool  WorkUnit::GetWorkUnitFromServer(ClientSocket *theSocket)
{
   char    *readBuf;
   bool     endLoop;
   int32_t  countScanned;
   uint32_t length;

   ii_CurvesDone = 0;
   ii_MaxCurves = 0;
   id_B1 = 0.0;
   ii_DecimalLength = 0;

   endLoop = false;
   readBuf = theSocket->Receive();
   while (readBuf && !endLoop)
   {
      if (!memcmp(readBuf, "INACTIVE", 8) || !memcmp(readBuf, "INFO", 4))
      {
         ip_Log->LogMessage("%s: No active candidates found on server", is_Suffix);
         return false;
      }

      // 3.x and later servers send a single message
      if (memcmp(theSocket->GetServerVersion(), "3", 1) >= 0)
      {
         if (!memcmp(readBuf, "End of Message", 14))
            return false;

         countScanned = sscanf(readBuf, "Factoring Work for %s %s %d %s %lf %d %u",
                               is_CompositeName, is_CompositeValue, &ii_DecimalLength,
                               is_FactorMethod, &id_B1, &ii_MaxCurves, &length);
         if (countScanned != 7)
         {
            ip_Log->LogMessage("%s: Could not parse message <%s>", is_Suffix, readBuf);
            return false;
         }
         if (length != strlen(is_CompositeValue))
         {
            ip_Log->LogMessage("%s: Composite length is wrong in message <%s>", is_Suffix, readBuf);
            return false;
         }

         return true;
      }

      if (!memcmp(readBuf, "ECMname: ", 9))
         strcpy(is_CompositeName, readBuf+9);
      else if (!memcmp(readBuf, "ECMnumber: ", 11))
         strcpy(is_CompositeValue, readBuf+11);
      else if (!memcmp(readBuf, "ServerVersion: ", 15))
         ;  // ignored
      else if (!memcmp(readBuf, "ServerType: ", 12))
         ;  // ignored
      else if (!memcmp(readBuf, "ECMB1: ", 7))
         id_B1 = atof(readBuf+7);
      else if (!memcmp(readBuf, "FactorMethod: ", 14))
         strcpy(is_FactorMethod, readBuf+14);
      else if (!memcmp(readBuf, "ECMCurves: ", 11))
         ii_MaxCurves = atol(readBuf+11);
      else if (!memcmp(readBuf, "End of Message", 14))
         endLoop = true;
      else if (!memcmp(readBuf, "ERR", 3))
      {
         ip_Log->LogMessage("%s: %s", is_Suffix, readBuf);
         return false;
      }
      else
      {
         ip_Log->LogMessage("%s: GetWorkUnit error. Data [%s] cannot be parsed", is_Suffix, readBuf);
         return false;
      }

      if (endLoop)
         break;

      readBuf = theSocket->Receive();
   }

   if (ii_MaxCurves == 0)
      ii_MaxCurves = 999999999;

   if (!is_CompositeName[0] || !is_CompositeValue[0] || id_B1 < 1.0)
   {
      ip_Log->LogMessage("%s: GetWorkUnit error.  Name, Number, or B1 not returned by server", is_Suffix);
      return false;
   }

   if (!ii_DecimalLength)
      ii_DecimalLength = (int32_t) strlen(is_CompositeValue);

   return true;
}

bool  WorkUnit::ReturnWorkUnit(ClientSocket *theSocket)
{
   char  *theMessage, goodMessage[200];
   bool   socketDown = false, haveError = false;

   if (ii_CurvesDone == 0)
   {
      theSocket->Send("Abandon Work for %s %s %.0lf",
                      is_CompositeName, is_FactorMethod, id_B1);
      return true;
   }

   if (is_CompositeName[0] == 0)
      return true;

   // 3.x and later servers expect different message
   if (memcmp(theSocket->GetServerVersion(), "3", 1) >= 0)
   {
      sprintf(goodMessage, "Accepted Work for %s", is_CompositeName);

      theSocket->Send("Return Work for %s %s %s %d %.0lf %d %lf %d",
                      is_CompositeName, is_CompositeValue, is_FactorMethod,
                      ib_FactorIsNumber, id_B1, ii_CurvesDone,
                      id_B1Time + id_B2Time, strlen(is_CompositeValue));
      if (ib_HaveFactor)
      {
         theSocket->Send("Return Factor Event for %s %s %d %.0lf %lf %.0lf %lf %"PRId64" %"PRId64"",
                         is_CompositeName, is_FactorMethod, ii_FactorStep,
                         id_B1, id_FactorB1Time, id_B2, id_FactorB2Time, il_Sigma, il_X0);
         theSocket->Send("Return Factor Event Detail for %s %s %s",
                         is_CompositeName, is_FactorValue, is_FactorType);
         theSocket->Send("Return Factor Event Detail for %s %s %s",
                         is_CompositeName, is_CofactorValue, is_CofactorType);
      }
      theSocket->Send("End Return Work for %s", is_CompositeName);

      theMessage = theSocket->Receive();

      if (!theMessage)
      {
         ip_Log->LogMessage("Did not get confirmation for workunit %s", is_CompositeName);
         return false;
      }

      if (!strcmp(theMessage, goodMessage))
      {
         ip_Log->LogMessage("%s: %d %s curves for B1=%.0lf accepted for composite %s",
                            is_Suffix, ii_CurvesDone, is_FactorMethod, id_B1, is_CompositeName);

         if (ib_HaveFactor)
            ip_Log->LogMessage("%s: Factor %s (found with %s, B1=%.0lf) accepted for composite %s",
                               is_Suffix, is_FactorValue, is_FactorMethod, id_B1, is_CompositeName);

         return true;
      }
      else
      {
         ip_Log->LogMessage(theMessage);
         return false;
      }
   }

   ip_Log->Debug(DEBUG_WORK, "%s: Uploading %ld %s curve%s for %s (B1=%.0lf)", is_Suffix, ii_CurvesDone,
                 is_FactorMethod, ((ii_CurvesDone == 1) ? "" : "s"), is_CompositeName, id_B1);

   theSocket->Send("RETURNWORK %s", ECMNET_VERSION);
   theSocket->Send("return_ECMname: %s", is_CompositeName);
   theSocket->Send("return_ECMnumber: %s", is_CompositeValue);
   theSocket->Send("return_ClientVersion: %s", ECMNET_VERSION);
   theSocket->Send("return_FactorMethod: %s", is_FactorMethod);
   if (ib_FactorIsNumber)
      theSocket->Send("return_FactorIsNumber:");

   theSocket->Send("return_B1: %.0lf", id_B1);
   theSocket->Send("return_Curves: %ld", ii_CurvesDone);
   theSocket->Send("End of Message");

   if (ib_HaveFactor)
   {
      ip_Log->Debug(DEBUG_WORK, "%s: Uploading factor for %s: %s", is_Suffix, is_CompositeName, is_FactorValue);

      theSocket->Send("RETURNFACTOR %s", ECMNET_VERSION);
      theSocket->Send("factor_ECMname: %s", is_CompositeName);
      theSocket->Send("factor_ECMnumber: %s", is_CompositeValue);
      theSocket->Send("factor_FACTOR: %s:%s:%s:%s", is_FactorValue, is_FactorType, is_CofactorValue, is_CofactorType);
      theSocket->Send("factor_FINDER: %s", theSocket->GetEmailID());
      theSocket->Send("factor_B1: %.0lf", id_B1);
      theSocket->Send("factor_sigma: %"PRId64"", il_Sigma);
      theSocket->Send("End of Message");

      theMessage = theSocket->Receive();
      if (!theMessage || strlen(theMessage) == 0)
         socketDown = 1;
      else if (!memcmp(theMessage, "OK.", 3) || !memcmp(theMessage, "INFO", 4))
         ip_Log->LogMessage("%s: Factor of %s was accepted: %s", is_Suffix, is_CompositeName, is_FactorValue);
      else if (!memcmp(theMessage, "ERR", 3))
      {
         ip_Log->LogMessage("%s: %s", is_Suffix, theMessage);
         haveError = 1;
      }
      else 
         ip_Log->LogMessage("%s: %s", is_Suffix, theMessage);
   }

   if (socketDown || haveError)
      return false;

   return true;
}

void  WorkUnit::SetUpCall(void)
{
   char     tempCommand[200], tempK[20], tempM[20];
   FILE    *fp;

   if ((fp = fopen(is_InFileName, "wt")) == NULL)
   {
      printf("Could not open file [%s] for writing.  Exiting program", is_InFileName);
      exit(-1);
   }

   fprintf(fp, "%s", is_CompositeValue);
   fclose(fp);

   sprintf(tempCommand, "%s", is_GMPExe);
   tempK[0] = 0;
   tempM[0] = 0;

   if (!memcmp(is_FactorMethod, FM_PM1, 3))
      sprintf(tempCommand, "%s -pm1", is_GMPExe);
   else if (!memcmp(is_FactorMethod, FM_PP1, 3))
      sprintf(tempCommand, "%s -pp1", is_GMPExe);

   if (ii_KValue) sprintf(tempK, "-k %d", ii_KValue);
   if (ii_MaxmemValue) sprintf(tempM, "-maxmem %d", ii_MaxmemValue);

   sprintf(is_Command, "%s %s %s %.0lf < %s > %s", tempCommand, tempK, tempM, id_B1, is_InFileName, is_OutFileName);

   ip_Log->UseCRInsteadOfLF(true);
   ip_Log->LogMessage("%s: Starting %s curve %d of %d for B1=%.0lf on %s", is_Suffix, is_FactorMethod, ii_CurvesDone+1, ii_MaxCurves, id_B1, is_CompositeName);
   ip_Log->UseCRInsteadOfLF(false);

   ip_Log->Debug(DEBUG_WORK, "%s: %s: %s", is_Suffix, is_CompositeName, is_Command);
}

// Parse the output from GMP-ECM and determine if a curve was
// completed.  If we detect that a factor was found, extract it.
bool  WorkUnit::ParseCurveOutput(void)
{
   char     line[241];
   FILE    *fp;
   double   seconds, b1Time = 0.0, b2Time = 0.0;
   bool     curveDone = false, factorFound = false;
   int32_t  step;

   if ((fp = fopen(is_OutFileName, "r")) == NULL)
      return false;

   // count no of lines in file
   while (fgets(line, 240, fp) != NULL)
   {
      if (!memcmp(line, "Found input number", 18))
      {
         ip_Log->LogMessage("%s: The input number was found.  Either the number is prime or B1 is too large", is_CompositeName);
         curveDone = 1;
         factorFound = 0;
         ib_FactorIsNumber = true;
         break;
      }

      if (strstr(line, "Step 2 took") != NULL)
         curveDone = true;
      if (strstr(line, "Factor found") != NULL)
         curveDone = factorFound = true;

      if (sscanf(line, "Step %d took %lf", &step, &seconds) == 2)
      {
         // The time is in milliseconds
         seconds /= 1000;

         if (step == 1) b1Time = seconds;
         if (step == 2) b2Time = seconds;
      }
   }

   fclose(fp);

   id_B1Time += b1Time;
   id_B2Time += b2Time;

   if (factorFound)
   {
      id_FactorB1Time = b1Time;
      id_FactorB2Time = b2Time;
      ii_FactorStep = step;
   }

   if (factorFound)
      ExtractFactor();

   if (curveDone)
      ib_JustDidCurve = true;

   return curveDone;
}

// This will only be called if we detected that a factor was found
void  WorkUnit::ExtractFactor(void)
{
   FILE    *fp;
   char    *ptr, *line, *temp;
   Log     *factorLog;

   if ((fp = fopen(is_OutFileName, "r")) == NULL)
      return;

   il_Sigma = 0;
   il_X0 = 0;
   id_B2 = 0.0;
   line = new char[BUFFER_SIZE];
   is_FactorValue[0] = 0;
   is_CofactorValue[0] = 0;
   is_FactorType[0] = 0;
   is_CofactorType[0] = 0;

   // The WorkUnit.out file has the output of ONE run of ecm.
   // Look for: "Found " ...next word is type of factor (prime, composite, probable)
   //                    ...get the factor after the ":" on this row.
   //                    ...First word of next row is type of cofactor (prime, composite, probable)
   //         : "Using B1="...get the b1/b2/sigma/x0 values from this ro
   //         : cofactor ...next word is the base10 cofactor
   //         : "Step " ...next character is the step that factor was found in.
   while (fgets(line, BUFFER_SIZE, fp) != NULL)
   {
      StripCRLF(line);

      // Check if we have found a factor, but have no information on the cofactor.
      // If so, then the first word of this row is the type of cofactor...
      if (is_FactorValue[0] && !is_CofactorType[0])
      {
         temp = NextWord(line);
         strcpy(is_CofactorType, temp);
         delete [] temp;
      }

      if ((ptr = strstr(line, "Found ")) != NULL)
      {
         // Get factor type
         temp = NextWord(ptr+6);
         strcpy(is_FactorType, temp);
         delete [] temp;

         // Get factor base10 representation...
         ptr = strstr(ptr, ": ");
         if (ptr != NULL)
         {
            ptr += 2;

            if (strcmp(ptr, is_CompositeValue))
               strcpy(is_FactorValue, ptr);
         }
      }
      else if ((ptr = strstr(line, "Using B1=")) != NULL)
      {
         // parse "Using B1=11000 ... sigma=151754956"
         id_B1 = atof(ptr+9);
         if (!strcmp(is_FactorMethod, FM_ECM))
         {
            // For ECM, get sigma
            ptr = strstr(line, "sigma=");
            if (ptr)
            {
               char *ptr2 = strchr(ptr, ':');
               if (ptr2)
                  il_Sigma = (int64_t) atof(ptr2+1);
               else
                  il_Sigma = (int64_t) atof(ptr+6);
            }
         }
         else
         {
            // For P-1, get x0
            ptr = strstr(line, "x0=");
            if (ptr) il_X0 = (int64_t) atof(ptr+3);
         }
         ptr = strstr(line, ", B2=");
         id_B2 = atof(ptr+5);
      }
      // Support older versions of GMP-ECM
      else if ((ptr = strstr(line, "Using B2=")) != NULL)
      {
         // parse "Using B2=11000"
         id_B2 = atof(ptr+9);
      }
      else if ((ptr = strstr(line, "ofactor ")) != NULL)
      {
         // next word is the cofactor.
         temp = NextWord(ptr+8);
         strcpy(is_CofactorValue, temp);
         delete [] temp;
      }
   }
   fclose(fp);

   if (strlen(is_FactorValue))
   {
      factorLog = new Log(0, "Factors.log", 0, true);

      if (!strcmp(is_FactorMethod, FM_ECM))
         factorLog->LogMessage("\n%s: %s factored using %s B1=%.0lf B2=%.0lf sigma=%"PRId64" (found in step %d)",
                               is_Suffix, is_CompositeName, is_FactorMethod, id_B1, id_B2, il_Sigma, ii_FactorStep);
      else
         factorLog->LogMessage("\n%s: %s factored using %s B1=%.0lf B2=%.0lf x0=%"PRId64" (found in step %d)",
                               is_Suffix, is_CompositeName, is_FactorMethod, id_B1, id_B2, il_X0, ii_FactorStep);
      factorLog->LogMessage("%s: Factor 1: %s %s", is_Suffix, is_FactorType, is_FactorValue);
      factorLog->LogMessage("%s: Factor 2: %s %s", is_Suffix, is_CofactorType, is_CofactorValue);

      delete factorLog;
      ib_HaveFactor = true;
   }

   delete [] line;
}

char *WorkUnit::NextWord(char *string)
{
   // Return the word pointed to by ptr (delimited by space or colon or EOL)
   // The calling function is responsible for deleting the string returned.
   char *ptr2, *temp;
   int   i;

   i = 0;
   ptr2 = string;
   while ((*ptr2)!=' ' && (*ptr2)!=':' && (*ptr2)!=0)
   {
      i++;
      ptr2++;
   }

   temp = new char[i+2];
   memcpy(temp, string, i);
   temp[i] = 0;

   return temp;
}

// Save the current status
void  WorkUnit::Save(FILE *saveFile)
{
   if (ib_HaveFactor)
   {
      fprintf(saveFile, "%s %s %s %d %.0lf %lf %lf %.0lf %lf %lf %"PRId64" %"PRId64" %d %d %s %s %s %s\n",
              is_CompositeName, is_CompositeValue, is_FactorMethod, ii_FactorStep,
              id_B1, id_B1Time, id_FactorB1Time, id_B2, id_B2Time, id_FactorB2Time,
              il_Sigma, il_X0, ii_MaxCurves, ii_CurvesDone,
              is_FactorValue, is_FactorType, is_CofactorValue, is_CofactorType);
   }
   else
   {
      fprintf(saveFile, "%s %s %s %.0lf %d %d %d %lf %lf\n",
              is_CompositeName, is_CompositeValue, is_FactorMethod,
              id_B1, ii_MaxCurves, ii_CurvesDone, ib_FactorIsNumber, id_B1Time, id_B2Time);
   }
}

// Load the previous status
void  WorkUnit::Load(char *inputLine)
{
   int32_t  countScanned, factorIsNumber;

   ib_HaveFactor = false;
   countScanned = sscanf(inputLine, "%s %s %s %d %lf %lf %lf %lf %lf %lf %"PRId64" %"PRId64" %d %d %s %s %s %s",
                         is_CompositeName, is_CompositeValue, is_FactorMethod, &ii_FactorStep,
                         &id_B1, &id_B1Time, &id_FactorB1Time, &id_B2, &id_FactorB2Time, &id_B2Time,
                         &il_Sigma, &il_X0, &ii_MaxCurves, &ii_CurvesDone,
                         is_FactorValue, is_FactorType, is_CofactorValue, is_CofactorType);

   if (countScanned != 18)
   {
      countScanned = sscanf(inputLine, "%s %s %s %lf %d %d %d %lf %lf",
              is_CompositeName, is_CompositeValue, is_FactorMethod,
              &id_B1, &ii_MaxCurves, &ii_CurvesDone,
              &factorIsNumber, &id_B1Time, &id_B2Time);

      if (countScanned != 9)
      {
         ip_Log->LogMessage("Unable to parse input line <%s> from savefile", inputLine);
         exit(0);
      }
      ib_FactorIsNumber = (factorIsNumber == 1) ? true : false;
   }
   else
      ib_HaveFactor = true;
}
