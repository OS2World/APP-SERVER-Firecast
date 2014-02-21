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

#ifndef SOURCES_H
#define SOURCES_H

#include "system.h"
#include "httpserv/reqfields.h"
#include "httpserv/requests.h"
#include "format.h"
#include "templates.h"

#define SCRES_OK		0
#define SCRES_NOT_ENOUGHT_MEM	1
#define SCRES_MPOINT_EXIST	2
#define SCRES_SOURCE_NOT_EXIST	3
#define SCRES_DATA_OUT		5
#define SCRES_INVALID_STREAM	6
#define SCRES_CLIENT_EXIST	7
#define SCRES_CLIENT_DETACHED	8
#define SCRES_CLIENT_NOT_EXIST	9
#define SCRES_INVALID_MPOINT	10
#define SCRES_ACCESS_DENIED	11

typedef struct _CLIENT *PCLIENT;

typedef struct _SOURCE {
  struct _SOURCE	*psNext;
  struct _SOURCE	**ppsSelf;
  ULONG			ulId;
  PSZ			pszMountPoint;
  HMTX			hmtxQueryLocks;
  ULONG			ulQueryLocks;
  PFORMAT		psFormat;
  REQFIELDS		sClientFields;
  PTEMPLATE		psTemplate;
  BOOL			bStop;
  BOOL			bDetached;
  ULONG			ulDetachTime;
  struct in_addr	sSourceAddr;
  PCLIENT		psClientsList;
  PCLIENT		*ppsLastClient;
  ULONG			ulClientsCount;
  HMTX			hmtxClientsList;
} SOURCE, *PSOURCE;

VOID SourceInit();
VOID SourceDone();
VOID SourceGarbageCollector();
ULONG SourceCreate(PSOURCE *ppsSource, PSZ pszMountPoint, PREQFIELDS psFields,
                PCONNINFO psConnInfo, PTEMPLATE psTemplate);
VOID SourceDestroy(PSOURCE psSource);
VOID SourceStop(PSOURCE psSource);
PSOURCE SourceQuery(PSZ pszMountPoint);
BOOL SourceQueryPtr(PSOURCE psSource);
PSOURCE SourceQueryById(ULONG ulId);
VOID SourceRelease(PSOURCE psSource);
ULONG SourceWrite(PSOURCE psSource, PCHAR pcBuf, ULONG ulBufLen);
ULONG SourceRead(PSOURCE psSource, PCHAR pcBuf, PULONG pulBufLen,
                 PFORMATCLIENT psFormatClient);
ULONG SourceAttachClient(PSOURCE psSource, PCLIENT psClient,
                         PCONNINFO psConnInfo, PREQFIELDS psFields,
			 PFORMATCLIENT *ppsFormatClient);
ULONG SourceDetachClient(PSOURCE psSource, PCLIENT psClient);
BOOL SourceSetMeta(PSOURCE psSource, PSZ pszMetaData);
VOID SourceFillClientFields(PSOURCE psSource, PFORMATCLIENT psFormatClient,
                            PREQFIELDS psFields);
BOOL SourceTestPassword(PSOURCE psSource, PSZ pszPassword);

PSOURCE SourceQueryList();
VOID SourceReleaseList();
PSOURCE SourceGetNext(PSOURCE psSource);
BOOL SourceGetMetaHistoryItem(PSOURCE psSource, ULONG ulIdx, PCHAR pcBufTime,
                              ULONG ulBufTimeLength, PSZ *ppszMetaData);
PSZ SourceGetSourceAddr(PSOURCE psSource, PSZ pszBuf);
PCLIENT SourceQueryClientsList(PSOURCE psSource);
VOID SourceReleaseClientsList(PSOURCE psSource);
BOOL SourceDetachClientById(PSOURCE psSource, ULONG ulId);
#define SourceGetId(ps) (ps->ulId)
#define SourceGetMountPoint(ps) (ps->pszMountPoint)
#define SourceGetClientsCount(ps) (ps->ulClientsCount)
#define SourceGetFields(ps) (&ps->sClientFields)
#define SourceGetTemplateId(ps) (ps->psTemplate->ulId)
#define SourceGetHidden(ps) (TemplGetHidden(ps->psTemplate))

#endif // SOURCES_H
