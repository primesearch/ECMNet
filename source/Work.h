/* Work Class header for ECMNet.

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

#ifndef _Work_

#define _Work_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>

#include "Log.h"
#include "ClientSocket.h"
#include "WorkUnit.h"

class Work
{
public:
   Work(Log *theLog, const char *emailID, const char *userID, const char *machineID,
        const char *instanceID, const char *gmpExe,
        const char *serverConfiguration, int32_t kValue, int32_t mValue);

   ~Work();

   // Return unique identifier for this class instance
   char    *GetWorkSuffix(void) { return is_WorkSuffix; };

   double   GetWorkPercent(void) { return id_WorkPercent; };

   int32_t  GetCurvesDone(void);
   int32_t  GetCurvesToDo(void);

   bool     WasFactorFound(void);

   // Do a single curve
   double   DoCurve(void);

   // Get work from the server
   bool     GetWork(void);

   // Return completed work to the server
   bool     ReturnWork(void);

   // Return a flag indicating if this work can do another curve
   bool     CanDoAnotherCurve(void);

   bool     IsDeadWork(void) { return ib_DeadWork; };

private:
   void     Load();
   void     Save();
   void     AddWorkUnit(WorkUnit *wu);
   void     GetGMPEXEVersion(void);

   char     is_WorkSuffix[50];
   char     is_SaveFileName[200];
   double   id_WorkPercent;

   // This is a pointer to the log
   Log     *ip_Log;

   // The name of the GMP-ECM executable
   char     is_GMPExe[50];
   char     is_GMPECMVersion[20];

   char     is_ServerName[50];
   int32_t  ii_PortID;

   char     is_EmailID[200];
   char     is_UserID[200];
   char     is_MachineID[200];
   char     is_Instance[200];

   // The server socket
   ClientSocket *ip_Socket;

   // The value to use to the -k and -maxmem options for GMP-ECM
   int32_t  ii_KValue;
   int32_t  ii_MaxmemValue;

   WorkUnit *im_FirstWorkUnit;

   // This is set to true if we were unable to connect to the server to return work
   bool     ib_DeadWork;
};

#endif // #ifndef _Work_

