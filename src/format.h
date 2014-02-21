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

#ifndef FORMAT_H
#define FORMAT_H

#include "system.h"
#include "httpserv/reqfields.h"

#define METADATA_HISTORY_ITEMS	10

typedef struct _FORMAT *PFORMAT;
typedef PVOID PFORMATCLIENT;

VOID FormatInit();
VOID FormatDone();
PFORMAT FormatNew(PREQFIELDS psFields);
VOID FormatDestroy(PFORMAT psFormat);
PFORMATCLIENT FormatClientNew(PFORMAT psFormat, PREQFIELDS psFields);
VOID FormatClientDestroy(PFORMAT psFormat, PFORMATCLIENT psFormatClient);
BOOL FormatWrite(PFORMAT psFormat, PCHAR pcBuf, ULONG ulBufLen);
BOOL FormatRead(PFORMAT psFormat, PFORMATCLIENT psFormatClient, PCHAR pcBuf, PULONG pulBufLen);
VOID FormatClientGetFields(PFORMAT psFormat, PFORMATCLIENT psFormatClient, PREQFIELDS psFields);
BOOL FormatSetMeta(PFORMAT psFormat, PSZ pszMetaData);
BOOL FormatGetMetaDataHistoryItem(PFORMAT psFormat, ULONG ulIdx,
                                  PCHAR pcBufTime, ULONG ulBufTimeLength,
                                  PSZ *ppszMetaData);
VOID FormatReset(PFORMAT psFormat);

#endif // FORMAT_H
