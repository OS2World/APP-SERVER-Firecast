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

#include <string.h>
#include "clients.h"
#include "sources.h"

ULONG ClientCreate(PCLIENT psClient, PSOURCE psSource, PREQFIELDS psFields,
                   PCONNINFO psConnInfo)
{
  ULONG	ulRC;

  psClient->ppsSelf = NULL;
  psClient->ulId = TemplGetNextId();
  psClient->psFields = psFields;
  psClient->sAddr = psConnInfo->sClientAddr;
  SysMutexCreate( &psClient->hmtxClient );

  ulRC = SourceAttachClient( psSource, psClient, psConnInfo, psFields,
                             &psClient->psFormatClient );

  if ( ulRC != SCRES_OK )
  {
    psClient->psSource = NULL;
    SysMutexDestroy( &psClient->hmtxClient );
  }
  else
    psClient->psSource = psSource;
  
  return ulRC;
}

VOID ClientDestroy(PCLIENT psClient)
{
  if ( psClient->psSource != NULL )
    SourceDetachClient( psClient->psSource, psClient );

  SysMutexDestroy( &psClient->hmtxClient );
}

ULONG ClientGetFields(PCLIENT psClient, PREQFIELDS psFields)
{
  ULONG		ulRC;

  SysMutexLock( &psClient->hmtxClient );

  if ( psClient->psSource == NULL )
  {
    ulRC = SCRES_UNLINKED;
  }
  else
  {
    SourceFillClientFields( psClient->psSource, psClient->psFormatClient,
                            psFields );
    ulRC = SCRES_OK;
  }

  SysMutexUnlock( &psClient->hmtxClient );

  return ulRC;
}

ULONG ClientRead(PCLIENT psClient, PCHAR pcBuf, PULONG pulBufLen)
{
  ULONG		ulRC;

  SysMutexLock( &psClient->hmtxClient );
  if ( psClient->psSource == NULL )
  {
    ulRC = SCRES_UNLINKED;
  }
  else
  {
    ulRC = SourceRead( psClient->psSource, pcBuf, pulBufLen,
                       psClient->psFormatClient );
  }
  SysMutexUnlock( &psClient->hmtxClient );

  return ulRC;
}

VOID ClientUnlinkSource(PCLIENT psClient, PFORMAT psFormat)
{
  SysMutexLock( &psClient->hmtxClient );
  psClient->psSource = NULL;
  FormatClientDestroy( psFormat, psClient->psFormatClient );
  psClient->psFormatClient = NULL;
  SysMutexUnlock( &psClient->hmtxClient );
}
