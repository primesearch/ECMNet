/* SharedMemoryItem Class for ECMNet.

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


#ifdef WIN32
#include <windows.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include "SharedMemoryItem.h"

// Constructor
#ifdef WIN32
#include <windows.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include "SharedMemoryItem.h"

// Constructor
SharedMemoryItem::SharedMemoryItem(const char *itemName, bool withCondition)
{
   il_Value = 0;
   id_DoubleValue = 0.0;
   is_ItemName = itemName;
   ib_HasCondition = withCondition;

#ifdef WIN32
   ih_CriticalSection = &im_CriticalSection;
   ih_Condition = 0;

   InitializeCriticalSection(ih_CriticalSection);

   if (ib_HasCondition)
      ih_Condition = CreateSemaphore(NULL, 0, 2147483647, NULL);
#else
   pthread_mutexattr_init(&ih_PthreadMutexAttr);
   pthread_mutexattr_settype(&ih_PthreadMutexAttr, PTHREAD_MUTEX_ERRORCHECK);
   pthread_mutex_init(&ih_PthreadMutex, &ih_PthreadMutexAttr);

   if (ib_HasCondition)
      pthread_cond_init(&ih_Condition, NULL);
#endif
}

// Destructor
SharedMemoryItem::~SharedMemoryItem(void)
{
   Release();

#ifdef WIN32
   if (ib_HasCondition)
      CloseHandle(ih_Condition);

   DeleteCriticalSection(ih_CriticalSection);
#else
   pthread_mutex_destroy(&ih_PthreadMutex);

   if (ib_HasCondition)
      pthread_cond_destroy(&ih_Condition);
#endif
}

bool     SharedMemoryItem::TryLock(void)
{
   bool locked;

#ifdef WIN32
   locked = (TryEnterCriticalSection(ih_CriticalSection) == TRUE);
#else
   locked = !pthread_mutex_trylock(&ih_PthreadMutex);
#endif

   return locked;
}

void     SharedMemoryItem::Lock(void)
{

#ifdef WIN32
   EnterCriticalSection(ih_CriticalSection);
#else
   if (pthread_mutex_lock(&ih_PthreadMutex) != 0)
   {
      printf("Unable to lock mutex %s.  Exiting.\n", is_ItemName.c_str());
      exit(0);
   }
#endif
}

void     SharedMemoryItem::Release(void)
{
#ifdef WIN32
   LeaveCriticalSection(ih_CriticalSection);
#else
   pthread_mutex_unlock(&ih_PthreadMutex);
#endif
}

void     SharedMemoryItem::SetCondition(void)
{
#ifdef WIN32
   ii_CountWaiting++;

   Release();
   WaitForSingleObject(ih_Condition, INFINITE);
   Lock();
#else
   pthread_cond_wait(&ih_Condition, &ih_PthreadMutex);
#endif
}

void     SharedMemoryItem::ClearCondition(void)
{
#ifdef WIN32
   if (ii_CountWaiting > 0)
   {
      ReleaseSemaphore(ih_Condition, ii_CountWaiting, NULL);
      ii_CountWaiting = 0;
   }
#else
      pthread_cond_broadcast(&ih_Condition);
#endif
}

int64_t  SharedMemoryItem::GetValueNoLock(void)
{
   int64_t returnValue;

   Lock();
   returnValue = il_Value;
   Release();

   return returnValue;
}

void     SharedMemoryItem::SetValueNoLock(int64_t newValue)
{
   Lock();
   il_Value = newValue;
   Release();
}

void     SharedMemoryItem::IncrementValue(int64_t increment)
{
   Lock();
   il_Value += increment;
   Release();
}

void     SharedMemoryItem::DecrementValue(int64_t decrement)
{
   Lock();
   il_Value -= decrement;
   Release();
}

double   SharedMemoryItem::GetDoubleNoLock(void)
{
   double   doubleValue;

   Lock();
   doubleValue = id_DoubleValue;
   Release();

   return doubleValue;
}
