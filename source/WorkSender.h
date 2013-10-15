/* WorkSender Class header for ECMNet.

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

#ifndef _WorkSender_

#define _WorkSender_

#include <string>
#include "defs.h"
#include "DBInterface.h"
#include "Log.h"
#include "HelperSocket.h"
#include "server.h"
#include "SharedMemoryItem.h"

class WorkSender
{
public:
   WorkSender(DBInterface *dbInterface, HelperSocket *theSocket, Log *theLog,
              double minB1, double maxB1, int32_t pm1OverECM, int32_t pp1OverECM,
              b1_t *b1, int32_t b1Count, SharedMemoryItem *totalWorkSent,
              strategy_t *pStrategy, SharedMemoryItem *locker);

   ~WorkSender();

   void     ProcessMessage(char *theMessage);

private:
   char     ic_SortSequence[200];
   double   id_MinB1;
   double   id_MaxB1;
   int32_t  ii_PM1OverECM;
   int32_t  ii_PP1OverECM;
   int32_t  ii_B1Count;
   b1_t    *ip_B1;

   DBInterface   *ip_DBInterface;
   HelperSocket  *ip_Socket;
   Log           *ip_Log;
   strategy_t    *ip_Strategy;
   SharedMemoryItem *ip_Locker;
   SharedMemoryItem *ip_TotalWorkSent;

   bool        SendUsingStrategy(strategy_t *pStrategy);
   double      SendWork(const char *masterCompositeName, const char *compositeName, const char *compositeValue,
                        int32_t decimalLength, double totalWorkDone);
   double      SendWorkUnit(const char *compositeName, const char *compositeValue,
                            int32_t decimalLength, const char *factorMethod, double b1, int32_t curvesToDo);
   bool        GetECMIndexAndCurves(double totalWorkDone, int32_t &ecmIndex, int32_t &ecmCurves);
   int32_t     GetCurvesDone(const char *masterCompositeName, const char *factorMethod, double b1);
};

#endif // #ifdef _WorkSender_
