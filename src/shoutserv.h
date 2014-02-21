/*
 * Copyright 2008 Vasilkin Andrey <digi@os2.snc.ru>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 *   3. The name of the author may not be used to endorse or promote products
 *      derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef SHOUTSERV_H
#define SHOUTSERV_H

#include "system.h"
#include "sources.h"

#define SHRET_OK			0
#define SHRET_INVALID_PARAM		1
#define SHRET_NOT_ENOUGH_MEMORY		2

typedef struct _SHOUTMOUNTPOINT {
  USHORT	usPort;
  PSZ		pszMountPoint;
  ULONG		ulSocket;
} SHOUTMOUNTPOINT, *PSHOUTMOUNTPOINT;

typedef struct _SHOUTMOUNTPOINTSLIST {
  ULONG			ulCount;
  PSHOUTMOUNTPOINT	pasMountPoints;
} SHOUTMOUNTPOINTSLIST, *PSHOUTMOUNTPOINTSLIST;

#define ShoutServMountPointsListInit(pmpl) do {\
  ((PSHOUTMOUNTPOINTSLIST)(pmpl))->ulCount = 0;\
  ((PSHOUTMOUNTPOINTSLIST)(pmpl))->pasMountPoints = NULL;\
} while(0)


VOID ShoutServInit();
VOID ShoutServDone();
ULONG ShoutServMountPointsListAdd(PSHOUTMOUNTPOINTSLIST psShoutMountPointsList,
                                  USHORT usPort, PSZ pszMountPoint);
VOID ShoutServMountPointsListCancel(PSHOUTMOUNTPOINTSLIST psShoutMountPointsList);
VOID ShoutServMountPointsListApply(PSHOUTMOUNTPOINTSLIST psShoutMountPointsList);
PSOURCE ShoutServQuerySource(USHORT usPort, struct in_addr *psClientAddr);
BOOL ShoutServGetMountPoint(USHORT usPort, PCHAR pcBuf, ULONG ulBufLen);
VOID ShoutLoop();

#endif // SHOUTSERV_H
