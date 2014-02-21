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

#ifndef NETSERV_H
#define NETSERV_H

#include "system.h"

typedef struct _BIND {
  ULONG			ulSocket;
  PSZ			pszAddr;
  struct in_addr	sInAddr;
  USHORT		usPort;
  BOOL			bReady;
} BIND, *PBIND;

typedef struct _BINDLIST {
  ULONG		ulBindCount;
  PBIND		pasBind;
} BINDLIST, *PBINDLIST;

typedef struct _NETSERV {
  BINDLIST	sBindList;
  ULONG		ulListenQueueLen;
  HMTX		hmtxBindList;
} NETSERV, *PNETSERV;

typedef BOOL ACCEPTSOCKETCALLBACK(ULONG ulSocket);

#define NetServBindListInit(pbl) do {\
  ((PBINDLIST)(pbl))->ulBindCount = 0;\
  ((PBINDLIST)(pbl))->pasBind = NULL;\
} while(0)

VOID NetServInit(PNETSERV psNetServ, ULONG ulListenQueueLen);
VOID NetServDone(PNETSERV psNetServ);
ULONG NetServBindListAdd(PBINDLIST psBindList, PSZ pszAddr, USHORT usPort);
VOID NetServBindListApply(PNETSERV psNetServ, PBINDLIST psBindList);
VOID BindListDestroy(PBINDLIST psBindList);
VOID NetServGetClientSockets(PNETSERV psNetServ,
                             PULONG apulList, ULONG ulMaxCount, 
                             ULONG ulTimeout, PULONG pulActualCount);
VOID NetServSetAcceptSocketCB( ACCEPTSOCKETCALLBACK cbNewAcceptSocket );

ULONG NetServQueryBindList(PNETSERV psNetServ);
VOID NetServReleaseBindList(PNETSERV psNetServ);
BOOL NetServGetBind(PNETSERV psNetServ, ULONG ulIdx, PSZ *ppszAddr,
                    PUSHORT pusPort);

#endif // NETSERV_H
