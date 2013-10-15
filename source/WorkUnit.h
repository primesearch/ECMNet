/* WorkUnit Class header for ECMNet.

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

#ifndef _WorkUnit_

#define _WorkUnit_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>

#include "Log.h"
#include "ClientSocket.h"

class WorkUnit
{
public:
   WorkUnit(Log *theLog, const char *machineID, const char *instanceID, const char *suffix,
            const char *gmpExe, int32_t kValue, int32_t maxmemValue);

   ~WorkUnit();

   bool     GetWorkUnitFromServer(ClientSocket *theSocket);
   bool     ReturnWorkUnit(ClientSocket *theSocket);
   bool     GetJustDidCurve(void) { return ib_JustDidCurve; };
   void     SetJustDidCurve(bool justDidCurve) { ib_JustDidCurve = justDidCurve; };
   void    *GetNextWorkUnit(void) { return im_Next; };
   void     SetNextWorkUnit(void *nextWU) { im_Next = nextWU; };

   // Do a single curve
   double   DoCurve(bool offline);

   // Return a flag indicating if this WorkUnit can do another curve
   bool     CanDoAnotherCurve(bool offline);

   char    *GetWorkUnitName(void) { return is_CompositeName; };
   int32_t  GetCurvesDone(void) { return ii_CurvesDone; };
   int32_t  GetCurvesToDo(void) { return (ii_MaxCurves - ii_CurvesDone); };
   double   GetB1(void) { return id_B1; };
   char    *GetFactorMethod(void) { return is_FactorMethod; };

   // Return a flag indicating whether or not a factor was found
   bool     WasFactorFound(void) { return ib_HaveFactor; };

   void     GetGMPEXEVersion(void);

   void     Save(FILE *saveFile);
   void     Load(char *inputLine);

private:
   char     is_InFileName[200];
   char     is_OutFileName[200];
   void    *im_Next;

   // Set up the call to GMP-ECM
   void     SetUpCall(void);

   // Parse the curve output to determine if a factor was found
   bool     ParseCurveOutput(void);

   // Extract the values if k, the exponent, and the sign from the number
   void     ParseExpression(uint32_t *k, uint32_t *exp, const char *sign);

   // Extract the factor, cofactor, and their types from the GMP-ECM output file
   void     ExtractFactor(void);

   // Get the next word from the string
   char    *NextWord(char *string);
   void     Load();
   void     Save();

   // This is a pointer to the log
   Log     *ip_Log;

   bool     ib_JustDidCurve;

   // The name of the GMP-ECM executable
   char     is_GMPExe[200];

   char     is_MachineID[200];
   char     is_Instance[200];
   char     is_Suffix[200];

   // Unique name to identify the number
   char     is_CompositeName[200];
   char     is_CompositeValue[VALUE_LENGTH];
   int32_t  ii_DecimalLength;

   char     is_Command[200];

   char     is_FactorMethod[20];
   double   id_B1, id_B2;

   // The value to use to the -k and -maxmem options for GMP-ECM
   int32_t  ii_KValue;
   int32_t  ii_MaxmemValue;

   // The maximum number of curves to execute with the given B1 and factoring method
   int32_t  ii_MaxCurves;

   // The number of curves done with the given B1 and factoring method
   int32_t  ii_CurvesDone;

   //       This indicates if GMP-ECM found the input number as a factor.  This
   //       means that either the input number is prime or that B1 is too high.
   bool     ib_FactorIsNumber;

   bool     ib_HaveFactor;
   char     is_FactorValue[VALUE_LENGTH];
   char     is_FactorType[20];
   char     is_CofactorValue[VALUE_LENGTH];
   char     is_CofactorType[20];
   int32_t  ii_FactorStep;
   double   id_B1Time, id_B2Time;
   double   id_FactorB1Time, id_FactorB2Time;
   int64_t  il_Sigma;
   int64_t  il_X0;
};

#endif // #ifndef _WorkUnit_

