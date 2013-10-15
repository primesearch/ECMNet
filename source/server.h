/* ECMNet server main() function definition for ECMNet.

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

#ifndef _server_

#define _server_

#include "defs.h"
#include "Log.h"
#include "Socket.h"
#include "ServerSocket.h"
#include "SharedMemoryItem.h"

// In eval.c
extern "C" int eval_str (const char *cp, int *isPRP);

#define PRPTEST_AS_COMPOSITE 0
#define PRPTEST_AS_PRP       1
#define PRPTEST_AS_PRIME     2

#define MAX_B1 50

// This is populated from the database upon startup.
typedef struct
{
   double   b1;
   double   expectedWork;
   double   sumWork;
   long     curvesECM;
   long     curvesPP1;
   long     curvesPM1;
} b1_t;

typedef struct {
   char     sortSequence[200];
   double   selectStrategyPercent;
   double   selectCandidatePercent;
   double   hourBreak;
   double   percentAddIfOverHours;
   double   percentSubIfUnderHours;
   double   workSent;
   void    *next;
} strategy_t;

typedef struct
{
   int32_t  i_PortID;
   int32_t  i_MaxWorkUnits;
   int32_t  i_MaxClients;
   int32_t  i_PM1DepthOverECM;
   int32_t  i_PP1DepthOverECM;
   int32_t  i_B1Count;
   int32_t  i_ExpireHours;
   double   d_MinB1;
   double   d_MaxB1;
   char     s_ServerID[200];
   char     s_HTMLTitle[200];
   char     s_ProjectName[200];
   char     s_MasterPassword[200];
   char     s_AdminPassword[200];
   bool     b_IAmASlave;
   bool     b_Recurse;
   b1_t     p_B1[MAX_B1];
   Log     *p_Log;

   strategy_t       *p_Strategy;

   ServerSocket     *p_ServerSocket;

   SharedMemoryItem *p_Quitting;
   SharedMemoryItem *p_ClientCounter;
   SharedMemoryItem *p_TotalWorkSent;
   SharedMemoryItem *p_Locker;
} globals_t;

#endif // #ifndef _gfn_
