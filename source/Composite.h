/* Composite Class header for ECMNet.

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

#ifndef _Composite_

#define _Composite_

#include "defs.h"
#include "server.h"
#include "DBInterface.h"
#include "Log.h"

class Composite
{
public:
   // Only use this when we expect to find an existing composite
   Composite(DBInterface *dbInterface, Log *theLog, const char *compositeName, bool hasBeenVerifiedAgainstMaster);

   ~Composite();

   bool     WasFound(void) { return ib_Found; };
   int32_t  GetLength(void) { return ii_DecimalLength; };

   bool     SetMasterCompositeName(string masterCompositeName);
   string   GetCompositeName(void) { return is_CompositeName; };
   string   GetMasterCompositeName(void) { return is_MasterCompositeName; };
   bool     CompositeIsMasterComposite(void) { return is_MasterCompositeName == is_CompositeName; };
   int32_t  GetPRPTestResult(void) { return ii_PRPTestResult; };
   bool     SetValue(const char *compositeValue);
   void     SetLastSendTime(void) { il_LastSendTime = (int64_t) time(NULL); };
   int32_t  NextFactorEvent(void) { return ++ii_FactorEventCount; };

   bool     FinalizeChanges(void);

private:
   DBInterface   *ip_DBInterface;
   Log           *ip_Log;

   string         is_MasterCompositeName;
   string         is_CompositeName;
   string         is_CompositeValue;
   int32_t        ii_DecimalLength;
   int64_t        il_LastSendTime;
   bool           ib_Found;
   bool           ib_VerifiedMaster;
   int32_t        ii_PRPTestResult;
   int32_t        ii_FactorEventCount;
   double         id_Difficulty;

   void     ComputeDecimalLengthAndPRPTest(void);

   bool     Insert(void);
   bool     Update(void);
};

#endif // #ifndef _Composite_
