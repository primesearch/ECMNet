/* Common defines/includes for ECMNet.

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

#ifndef _defs_

#define _defs_

#if defined(_MSC_VER) && defined(MEMLEAK)
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif

#define ECMNET_VERSION "3.0.5"

#if defined(__unix__) || defined(__unix) || defined(__APPLE__)
   #define __STDC_FORMAT_MACROS
   #include <inttypes.h>
   #define UNIX
#else
   #define PRId64 "I64d"
   #define PRIu64 "I64u"
#endif

#include <string>
#include <string.h>
using namespace std;

#define VALUE_LENGTH       50000
#define BUFFER_SIZE        VALUE_LENGTH + 5000
#define METHOD_LENGTH      20
#define NAME_LENGTH        100
#define ID_LENGTH          200

#define FM_PM1   "P-1"
#define FM_PP1   "P+1"
#define FM_ECM   "ECM"

#ifdef WIN32
   #define unlink _unlink
   #define stricmp _stricmp
   #define int32_t  signed int
   #define uint32_t unsigned int
   #define int64_t  signed long long
   #define uint64_t unsigned long long
   void SetQuitting(int a);
#if defined(_MSC_VER) && defined(MEMLEAK)
   #define DEBUG_NEW new(_NORMAL_BLOCK, __FILE__, __LINE__)
   #define new DEBUG_NEW
#endif
#endif

#ifdef UNIX
   #include <errno.h>
   #include <netdb.h>
   #include <unistd.h>

   #ifndef stricmp
      #define stricmp(x,y)  strcasecmp(x,y)
   #endif

   #ifndef RAND_MAX
      #define RAND_MAX (1 << 15) - 1
   #endif

   #define Sleep(x) usleep((x)*1000)
#endif

inline void  StripCRLF(char *string)
{
   long  x;

   if (!string)
      return;

   if (*string == 0)
      return;

   x = (long) strlen(string);
   while (x > 0 && (string[x-1] == 0x0a || string[x-1] == 0x0d))
   {
      string[x-1] = 0x00;
      x--;
   }
}

#endif // _defs_
