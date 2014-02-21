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

#ifndef RELAYS_H
#define RELAYS_H

#include "system.h"
#include "sources.h"
#include "httpserv/httpclnt.h"
#include "templates.h"

#define RELAYRES_OK			0
#define RELAYRES_NOT_ENOUGHT_MEM	1
#define RELAYRES_RELAY_EXIST		2
#define RELAYRES_INVALID_MPOINT		3

#define RELAY_STATE_IDLE		0x0000
#define RELAY_STATE_WAIT_CLIENTS	0x0001
#define RELAY_STATE_TRANSMIT		0x0002
#define RELAY_STATE_URL_RECEIVED	0x0003
#define RELAY_STATE_CONNECT		0x0004
#define RELAY_STATE_ERROR_MASK		0xFF00
#define RELAY_STATE_NO_TEMPLATE		0x0100
#define RELAY_STATE_LOCATION_FAILED	0x0200
#define RELAY_STATE_TRANSMIT_ERROR	0x0300
#define RELAY_STATE_INVALID_MPOINT	0x0400
#define RELAY_STATE_MPOINT_EXIST	0x0500
#define RELAY_STATE_INVALID_FORMAT	0x0600
#define RELAY_STATE_ERROR_CREATE_MPOINT	0x0700
#define RELAY_STATE_DISCONNECTED	0x0800
#define RELAY_STATE_NAME_NOT_FOUND	0x0900
#define RELAY_STATE_CONNECTION_CLOSED	0x0A00
#define RELAY_STATE_CONNECTION_REFUSED	0x0B00
#define RELAY_STATE_ADDRESS_UNREACH	0x0C00
#define RELAY_STATE_CANNOT_CONNECT	0x0D00
#define RELAY_STATE_INVALID_RESPONSE	0x0E00
#define RELAY_STATE_HTTP_NOT_FOUND	0x0F00

#define RELAY_MAX_M3U_LENGTH		256

typedef struct _RELAYURLFILE {
  ULONG		ulLength;
  CHAR		acContent[RELAY_MAX_M3U_LENGTH];
} RELAYURLFILE, *PRELAYURLFILE;

typedef struct _RELAY {
  struct _RELAY		*psNext;
  struct _RELAY		**ppsSelf;
  ULONG			ulId;
  PSZ			pszLocation;
  PSZ			pszMountPoint;
  BOOL			bNailedUp;
  PTEMPLATE		psTemplate;
  PSOURCE		psSource;
  PHTTPCLNT		psHTTPClnt;
  PRELAYURLFILE		psURLFile;
  ULONG			ulState;
  ULONG			ulTimeStamp;
  ULONG			ulQueryLocks;
  ULONG			ulMetaInt;
  ULONG			ulMetaPos;
  ULONG			ulMetaLen;
  ULONG			ulMetaMaxLen;
  PCHAR			pcMeta;
  HMTX			hmtxQueryLocks;
  ULONG			ulLockFlag;
} RELAY, *PRELAY;

VOID RelayInit();
VOID RelayDone();
VOID RelayRemoveAll();

PRELAY RelayNew();
VOID RelayReady(PRELAY psRelay);
VOID RelayDestroy(PRELAY psRelay);
//PRELAY RelayQuery(PSZ pszMountPoint);
PRELAY RelayQueryById(ULONG ulId);
BOOL RelayQueryPtr(PRELAY psRelay);
VOID RelayRelease(PRELAY psRelay);
VOID RelaySetMountPoint(PRELAY psRelay, PSZ pszMountPoint);
VOID RelaySetLocation(PRELAY psRelay, PSZ pszLocation);
BOOL RelayReset(PRELAY psRelay);
#define RelaySetNailedUp(ps,a) (ps)->bNailedUp = a
#define RelayGetId(ps) ((ps)->ulId)
#define RelayGetState(ps) ((ps)->ulState)
#define RelayGetNailedUp(ps) ((ps)->bNailedUp)
#define RelayGetMountPoint(ps) ((ps)->pszMountPoint)
#define RelayGetLocation(ps) ((ps)->pszLocation)

PRELAY RelayQueryList();
VOID RelayReleaseList();
#define RelayGetNext(ps) (ps->psNext)

#endif // RELAYS_H
