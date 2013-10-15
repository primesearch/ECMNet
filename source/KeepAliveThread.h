/* KeepAliveThread Class header for ECMNet.

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

#ifndef _KeepAliveThread_

#define _KeepAliveThread_

#include "defs.h"
#include "server.h"
#include "Thread.h"
#include "Socket.h"

#define KAT_WAITING     1
#define KAT_TERMINATE   5
#define KAT_TERMINATED  9

class KeepAliveThread : public Thread
{
public:
   void  StartThread(Socket *theSocket);

private:
#ifdef WIN32
   static DWORD WINAPI EntryPoint(LPVOID threadInfo);
#endif
};

#endif // #ifndef _KeepAliveThread_

