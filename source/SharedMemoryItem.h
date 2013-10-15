/* SharedMemoryItem Class header for ECMNet.

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

#ifndef  _SharedMemoryItem_
#define  _SharedMemoryItem_

#include "defs.h"

#ifdef WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

class SharedMemoryItem
{
public:
   SharedMemoryItem(const char *itemName, bool withCondition = false);

   ~SharedMemoryItem(void);

   // returns true if mutex is locked
   void        Lock(void);

   // returns true if mutex is locked
   bool        TryLock(void);

   void        Release(void);

   // These assume that the mutex is locked
   int64_t     GetValueHaveLock(void) { return il_Value; };
   void        SetValueHaveLock(int64_t newValue) { il_Value = newValue; };

   // These will lock/unlock the mutex while getting/setting the value
   int64_t     GetValueNoLock(void);
   void        SetValueNoLock(int64_t newValue);

   // These will lock/unlock the mutex while updating the value
   void        IncrementValue(int64_t increment = 1);
   void        DecrementValue(int64_t decrement = 1);

   void        SetCondition(void);
   void        ClearCondition(void);

   double      GetDoubleNoLock(void);
   void        IncrementDoubleHaveLock(double increment) { id_DoubleValue += increment; };

private:
   string      is_ItemName;
   bool        ib_HasCondition;
   int64_t     il_Value;
   double      id_DoubleValue;

#ifdef WIN32
   uint32_t    ii_CountWaiting;
   CRITICAL_SECTION    im_CriticalSection;
   LPCRITICAL_SECTION  ih_CriticalSection;
   HANDLE              ih_Condition;
#else
   pthread_mutex_t     ih_PthreadMutex;
   pthread_mutexattr_t ih_PthreadMutexAttr;
   pthread_cond_t      ih_Condition;
#endif
};

#endif
