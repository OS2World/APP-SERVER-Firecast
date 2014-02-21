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

#include <stdlib.h>
#include <string.h>
#include "sources.h"
#include "clients.h"
#include "ssmessages.h"

#define NET_MODULE	SS_MODULE_SOURCES

#define GARBAGE_SCAN_BY_ITERATION	10

static PSOURCE		psSourcesList = NULL;
static PSOURCE		psSourceGarbageScan = NULL;
static PSOURCE		*ppsLastSource = &psSourcesList;
static RWMTX		rwmtxSourcesList;

static VOID _SourceDestroy(PSOURCE psSource)
{
  PCLIENT	psClient;

  psClient = psSource->psClientsList;
  while( psClient != NULL )
  {
    ClientUnlinkSource( psClient, psSource->psFormat );
    psClient = psClient->psNext;
  }

  FormatDestroy( psSource->psFormat );
  ReqFldDone( &psSource->sClientFields );

  if ( psSource->psTemplate != NULL )
    TemplRelease( psSource->psTemplate );

  SysMutexDestroy( &psSource->hmtxQueryLocks );
  SysMutexDestroy( &psSource->hmtxClientsList );

  free( psSource );
}

static BOOL _SourceQuery(PSOURCE psSource)
{
  SysMutexLock( &psSource->hmtxQueryLocks );
  if ( psSource->ppsSelf == NULL )
  {
    SysMutexUnlock( &psSource->hmtxQueryLocks );
    return FALSE;
  }
  psSource->ulQueryLocks++;
  SysMutexUnlock( &psSource->hmtxQueryLocks );
  return TRUE;
}

static BOOL _TestFieldName(PREQFIELD pReqField, PSZ pszName)
{
  ULONG		ulLength = pszName == NULL ? 0 : strlen( pszName );

  return (pReqField->ulNameLen >= ulLength) &&
         (memicmp( &pReqField->acData, pszName, ulLength ) == 0);
}



VOID SourceInit()
{
  SysRWMutexCreate( &rwmtxSourcesList );
}

VOID SourceDone()
{
  SysRWMutexDestroy( &rwmtxSourcesList );
}

VOID SourceGarbageCollector()
{
  ULONG		ulNow;
  ULONG		ulCount;

  SysTime( &ulNow );

  SysRWMutexLockRead( &rwmtxSourcesList );

  if ( psSourceGarbageScan == NULL )
    psSourceGarbageScan = psSourcesList;

  for( ulCount = 0; ulCount < GARBAGE_SCAN_BY_ITERATION; ulCount++ )
  {
    if ( psSourceGarbageScan == NULL )
      break;

    if ( _SourceQuery( psSourceGarbageScan ) )
    {
      if ( psSourceGarbageScan->bDetached &&
           ((psSourceGarbageScan->ulDetachTime - ulNow) & 0x80000000) != 0 )
      {
        SysRWMutexUnlockRead( &rwmtxSourcesList );
        SourceDestroy( psSourceGarbageScan );
        SourceRelease( psSourceGarbageScan );
        return;
      }
      SourceRelease( psSourceGarbageScan );
    }

    psSourceGarbageScan = psSourceGarbageScan->psNext;
  }

  SysRWMutexUnlockRead( &rwmtxSourcesList );
}

ULONG SourceCreate(PSOURCE *ppsSource, PSZ pszMountPoint, PREQFIELDS psFields,
                PCONNINFO psConnInfo, PTEMPLATE psTemplate)
{
  PFORMAT	psFormat;
  PSOURCE	psSource;

  if ( pszMountPoint == NULL )
    return SCRES_INVALID_MPOINT;
  if ( pszMountPoint[0] == '/' )
    pszMountPoint++;
  if ( pszMountPoint[0] == '\0' )
    return SCRES_INVALID_MPOINT;

  psSource = SourceQuery( pszMountPoint );
  if ( psSource != NULL )
  {
    if ( psSource->bDetached )
    {
      psSource->bDetached = FALSE;
      if ( psConnInfo != NULL )
      {
        psSource->sSourceAddr = psConnInfo->sClientAddr;
      }
      else
        memset( &psSource->sSourceAddr, 0, sizeof(struct in_addr) );

      SourceRelease( psSource );
      *ppsSource = psSource;
      return SCRES_OK;
    }
    else
    {
      SourceRelease( psSource );
      WARNING( SS_MESSAGE_MPOINTEXIST, psConnInfo, pszMountPoint, NULL );
      return SCRES_MPOINT_EXIST;
    }
  }

  pszMountPoint = strdup( pszMountPoint );
  if ( pszMountPoint == NULL )
  {
    ERROR_NOT_ENOUGHT_MEM();
    return SCRES_NOT_ENOUGHT_MEM;
  }

  psSource = calloc( 1, sizeof(SOURCE) );
  if ( psSource == NULL )
  {
    free( pszMountPoint );
    ERROR_NOT_ENOUGHT_MEM();
    return SCRES_NOT_ENOUGHT_MEM;
  }

  psFormat = FormatNew( psFields );
  if ( psFormat == NULL )
  {
    free( pszMountPoint );
    free( psSource );
    ERROR( SS_MESSAGE_INVALIDDATAFORMAT, pszMountPoint, NULL, NULL );
    return SCRES_INVALID_STREAM;
  }

  psSource->pszMountPoint = pszMountPoint;
  psSource->psFormat = psFormat;
  SysMutexCreate( &psSource->hmtxQueryLocks );
  SysMutexCreate( &psSource->hmtxClientsList );
  psSource->ulId = TemplGetNextId();
  psSource->ppsLastClient = &psSource->psClientsList;
  if ( psConnInfo != NULL )
  {
    psSource->sSourceAddr = psConnInfo->sClientAddr;
  }

  ReqFldInit( &psSource->sClientFields );

  // make fields for clients of source
  {
    PCHAR	pcName;
    PCHAR	pcValue;
    CHAR	acBuf[32];
    ULONG	ulLength;
    PREQFIELD	pReqField;

    pReqField = psFields->psFieldList;
    while( pReqField != NULL )
    {
      pcValue = &pReqField->acData[pReqField->ulNameLen];
      pcName = NULL;

      if ( _TestFieldName( pReqField, "ice-audio-info" ) )
      {
        pcValue = strstr( pcValue, "bitrate=" );
        if ( pcValue != NULL )
        {
          pcValue += 8;
          ulLength = 0;
          while( (UCHAR)(pcValue[ulLength]-'0') <= 9 && ulLength < 5 )
          {
            acBuf[ulLength] = pcValue[ulLength];
            ulLength++;
          }
          acBuf[ulLength] = '\0';

          pcName = "icy-br";
          pcValue = &acBuf;
        }
      }
      else if ( _TestFieldName( pReqField, "ice-public" ) )
      {
        pcName = "icy-pub";
      }
      else if ( _TestFieldName( pReqField, "ice-bitrate" ) )
      {
        pcName = "icy-br";
      }
      else if ( (_TestFieldName( pReqField, "ice-" ) &&
                !_TestFieldName( pReqField, "ice-password" )) ||
                (_TestFieldName( pReqField, "icy-" ) &&
                !_TestFieldName( pReqField, "icy-metaint" )) )
      {
        ulLength = min( pReqField->ulNameLen, sizeof(acBuf)-1 );
        memcpy( &acBuf, &pReqField->acData, ulLength );
        acBuf[ulLength] = '\0';
        acBuf[2] = 'y';
        pcName = &acBuf;
      }

      if ( pcName )
      {
        while( *pcValue == ' ' )
          pcValue++;
        ReqFldSet( &psSource->sClientFields, pcName, pcValue );
      }

      pReqField = pReqField->psNext;
    }
  }

  TemplApplyFields( psTemplate, &psSource->sClientFields );

  psSource->psTemplate = psTemplate;
  if ( !TemplQueryPtr( psSource->psTemplate ) )
  {
    psSource->psTemplate = NULL;
    _SourceDestroy( psSource );
    return SCRES_INVALID_MPOINT;
  }

  INFO( SS_MESSAGE_NEWSOURCE, psConnInfo, pszMountPoint, NULL );

  SysRWMutexLockWrite( &rwmtxSourcesList );
  psSource->ppsSelf = ppsLastSource;
  *ppsLastSource = psSource;
  ppsLastSource = &psSource->psNext;
  SysRWMutexUnlockWrite( &rwmtxSourcesList );

  *ppsSource = psSource;
  return SCRES_OK;
}

VOID SourceDestroy(PSOURCE psSource)
{
  SysMutexLock( &psSource->hmtxQueryLocks );

  if ( psSource->bStop || psSource->bDetached ||
       psSource->psTemplate->ulKeepAlive == 0 )
  {
    SysRWMutexLockWrite( &rwmtxSourcesList );
    if ( psSource == psSourceGarbageScan )
      psSourceGarbageScan = psSource->psNext;

    if ( psSource->ppsSelf != NULL )
    {
      *psSource->ppsSelf = psSource->psNext;
      if ( psSource->psNext != NULL )
      {
        psSource->psNext->ppsSelf = psSource->ppsSelf;
        psSource->psNext = NULL;
      }
      else
        ppsLastSource = psSource->ppsSelf;
    }
    psSource->ppsSelf = NULL;
    SysRWMutexUnlockWrite( &rwmtxSourcesList );

    INFO( SS_MESSAGE_DESTROYSOURCE, psSource->pszMountPoint, NULL, NULL );

    if ( psSource->ulQueryLocks == 0 )
    {
      SysMutexUnlock( &psSource->hmtxQueryLocks );
      _SourceDestroy( psSource );
      return;
    }
  }
  else
  {
    ULONG	ulNow;

    FormatReset( psSource->psFormat );
    psSource->bDetached = TRUE;
    SysTime( &ulNow );
    psSource->ulDetachTime = ulNow + (psSource->psTemplate->ulKeepAlive * 1000);
  }

  SysMutexUnlock( &psSource->hmtxQueryLocks );
}

VOID SourceStop(PSOURCE psSource)
{
  SysMutexLock( &psSource->hmtxQueryLocks );

  psSource->bStop = TRUE;
  if ( psSource->bDetached )
    SourceDestroy( psSource );

  SysMutexUnlock( &psSource->hmtxQueryLocks );
}

PSOURCE SourceQuery(PSZ pszMountPoint)
{
  PSOURCE	psSource;

  if ( pszMountPoint == NULL )
    return NULL;
  if ( pszMountPoint[0] == '/' )
    pszMountPoint++;
  if ( pszMountPoint[0] == '\0' )
    return NULL;

  SysRWMutexLockRead( &rwmtxSourcesList );
  psSource = psSourcesList;
  while( psSource != NULL )
  {
    if ( stricmp( pszMountPoint, psSource->pszMountPoint ) == 0 )
    {
      if ( !_SourceQuery( psSource ) )
        psSource = NULL;
      break;
    }
    psSource = psSource->psNext;
  }
  SysRWMutexUnlockRead( &rwmtxSourcesList );

  return psSource;
}

BOOL SourceQueryPtr(PSOURCE psSource)
{
  return _SourceQuery( psSource );
}

PSOURCE SourceQueryById(ULONG ulId)
{
  PSOURCE	psSource;

  if ( psSourcesList == NULL )
    return NULL;

  SysRWMutexLockRead( &rwmtxSourcesList );
  psSource = psSourcesList;
  while( psSource != NULL )
  {
    if ( psSource->ulId == ulId )
    {
      if ( !_SourceQuery( psSource ) )
        psSource = NULL;
      break;
    }
    psSource = psSource->psNext;
  }
  SysRWMutexUnlockRead( &rwmtxSourcesList );

  return psSource;
}

VOID SourceRelease(PSOURCE psSource)
{
  if ( psSource == NULL )
    return;

  SysMutexLock( &psSource->hmtxQueryLocks );
  if ( psSource->ulQueryLocks > 0 )
  {
    psSource->ulQueryLocks--;
    if ( psSource->ulQueryLocks == 0 && psSource->ppsSelf == NULL )
    {
      SysMutexUnlock( &psSource->hmtxQueryLocks );
      _SourceDestroy( psSource ); // ppsSelf=NULL ==> don't need lock here
      return;
    }
  }
  SysMutexUnlock( &psSource->hmtxQueryLocks );
}

ULONG SourceWrite(PSOURCE psSource, PCHAR pcBuf, ULONG ulBufLen)
{
  if ( pcBuf == NULL )
    return SCRES_OK;

  if ( !_SourceQuery( psSource ) )
    return SCRES_SOURCE_NOT_EXIST;

  if ( psSource->bStop )
  {
    SourceRelease( psSource );
    return SCRES_SOURCE_NOT_EXIST;
  }

  if ( !FormatWrite( psSource->psFormat, pcBuf, ulBufLen ) )
  {
    SourceRelease( psSource );
    WARNING( SS_MESSAGE_INVALIDDATAFORMAT, psSource->pszMountPoint, NULL, NULL );
    return SCRES_INVALID_STREAM;
  }

  SourceRelease( psSource );

  return SCRES_OK;
}

ULONG SourceRead(PSOURCE psSource, PCHAR pcBuf, PULONG pulBufLen,
                 PFORMATCLIENT psFormatClient)
{
  BOOL	bRes;

  if ( psSource->bStop || !_SourceQuery( psSource ) )
    return SCRES_SOURCE_NOT_EXIST;

  bRes = FormatRead( psSource->psFormat, psFormatClient, pcBuf, pulBufLen );

  SourceRelease( psSource );

  return bRes ? SCRES_OK : SCRES_DATA_OUT;
}

ULONG SourceAttachClient(PSOURCE psSource, PCLIENT psClient,
                         PCONNINFO psConnInfo, PREQFIELDS psFields,
			 PFORMATCLIENT *ppsFormatClient)
{
  PCLIENT	psScanClient;
  ULONG		ulRC;

  if ( !_SourceQuery( psSource ) )
    return SCRES_SOURCE_NOT_EXIST;

  if ( !TemplTestAclClients( psSource->psTemplate, psConnInfo ) )
  {
    INFO( SS_MESSAGE_CLIENTACCESSDENIED, psConnInfo, psSource->pszMountPoint, NULL );
    SourceRelease( psSource );
    return SCRES_ACCESS_DENIED;
  }

  if ( psSource->psTemplate->ulMaxClients != 0 &&
       psSource->ulClientsCount >= psSource->psTemplate->ulMaxClients )
  {
    INFO( SS_MESSAGE_TOOMANYCLIENTS, psConnInfo, psSource->pszMountPoint,
          &psSource->psTemplate->ulMaxClients );
    SourceRelease( psSource );
    return SCRES_ACCESS_DENIED;
  }

  SysMutexLock( &psSource->hmtxClientsList );

  psScanClient = psSource->psClientsList;
  while( psScanClient != NULL )
  {
    if ( psScanClient == psClient )
    {
      SysMutexUnlock( &psSource->hmtxClientsList );
      SourceRelease( psSource );
      return SCRES_CLIENT_EXIST;
    }
    psScanClient = psScanClient->psNext;
  }

  *ppsFormatClient = FormatClientNew( psSource->psFormat, psFields );

  if ( *ppsFormatClient == NULL )
  {
    ulRC = SCRES_NOT_ENOUGHT_MEM;
  }
  else
  {
    psClient->psNext = NULL;
    psClient->ppsSelf = psSource->ppsLastClient;
    *psSource->ppsLastClient = psClient;
    psSource->ppsLastClient = &psClient->psNext;
    psSource->ulClientsCount++;
    ulRC = SCRES_OK;
  }

  SysMutexUnlock( &psSource->hmtxClientsList );
  SourceRelease( psSource );

  return ulRC;
}

ULONG SourceDetachClient(PSOURCE psSource, PCLIENT psClient)
{
  PCLIENT	psScanClient;

  if ( psClient->ppsSelf == NULL )
  {
    ClientUnlinkSource( psClient, psSource->psFormat );
    return SCRES_CLIENT_DETACHED;
  }

  if ( !_SourceQuery( psSource ) )
    return SCRES_SOURCE_NOT_EXIST;

  SysMutexLock( &psSource->hmtxClientsList );

  psScanClient = psSource->psClientsList;
  while( psScanClient != NULL )
  {
    if ( psScanClient == psClient )
    {
      *psScanClient->ppsSelf = psScanClient->psNext;
      if ( psScanClient->psNext != NULL )
        psScanClient->psNext->ppsSelf = psScanClient->ppsSelf;
      else
        psSource->ppsLastClient = psScanClient->ppsSelf;
      psClient->ppsSelf = NULL;

      ClientUnlinkSource( psScanClient, psSource->psFormat );
      psSource->ulClientsCount--;

      break;
    }
    psScanClient = psScanClient->psNext;
  }

  SysMutexUnlock( &psSource->hmtxClientsList );
  SourceRelease( psSource );

  return psClient->ppsSelf == NULL ? SCRES_OK : SCRES_CLIENT_NOT_EXIST;
}

BOOL SourceSetMeta(PSOURCE psSource, PSZ pszMetaData)
{
  BOOL		bRes;

  if ( !_SourceQuery( psSource ) )
    return FALSE;

  if ( psSource->psFormat != NULL )
  {
    bRes = FormatSetMeta( psSource->psFormat, pszMetaData );
  }
  else
    bRes = FALSE;

  SourceRelease( psSource );

  return bRes;
}

VOID SourceFillClientFields(PSOURCE psSource, PFORMATCLIENT psFormatClient,
                            PREQFIELDS psFields)
{
  if ( _SourceQuery( psSource ) )
  {
    ReqFldCopy( psFields, &psSource->sClientFields );
    if ( psFormatClient != NULL )
      FormatClientGetFields( psSource->psFormat, psFormatClient, psFields );
    SourceRelease( psSource );
  }
}

BOOL SourceTestPassword(PSOURCE psSource, PSZ pszPassword)
{
  return TemplTestPassword( psSource->psTemplate, pszPassword );
}

PSOURCE SourceQueryList()
{
  PSOURCE	psSource;

  if ( psSourcesList == NULL )
    return NULL;

  SysRWMutexLockRead( &rwmtxSourcesList );
//old/  _SourceQuery( psSourcesList );
//old/  SysRWMutexUnlockRead( &rwmtxSourcesList );
  return psSourcesList;
}

VOID SourceReleaseList()
{
//old/ Nothing to do:
//old/   We havn't lock rwmtxSourcesList, last psSource already unlocked in 
//old/   SourceGetNext()
  SysRWMutexUnlockRead( &rwmtxSourcesList );
}

PSOURCE SourceGetNext(PSOURCE psSource)
{
  return psSource->psNext;
/*old/
  PSOURCE	psSourceNext;

  SysRWMutexLockRead( &rwmtxSourcesList );
  if ( psSource->ppsSelf != NULL && psSource->psNext != NULL )
  {
    psSourceNext = psSource->psNext;
    _SourceQuery( psSourceNext );
  }
  else
    psSourceNext = NULL;
  SourceRelease( psSource );
  SysRWMutexUnlockRead( &rwmtxSourcesList );

  return psSourceNext;
*/
}

BOOL SourceGetMetaHistoryItem(PSOURCE psSource, ULONG ulIdx, PCHAR pcBufTime,
                              ULONG ulBufTimeLength, PSZ *ppszMetaData)
{
  BOOL	bRes;

  if ( psSource->psFormat == NULL )
    bRes = FALSE;
  else
    bRes = FormatGetMetaDataHistoryItem( psSource->psFormat, ulIdx, pcBufTime,
                                         ulBufTimeLength, ppszMetaData );

  return bRes;
}

PSZ SourceGetSourceAddr(PSOURCE psSource, PSZ pszBuf)
{
  SysStrAddr( pszBuf, 32, &psSource->sSourceAddr );
  return pszBuf;
}

PCLIENT SourceQueryClientsList(PSOURCE psSource)
{
  SysMutexLock( &psSource->hmtxClientsList );
  return psSource->psClientsList;
}

VOID SourceReleaseClientsList(PSOURCE psSource)
{
  SysMutexUnlock( &psSource->hmtxClientsList );
}

BOOL SourceDetachClientById(PSOURCE psSource, ULONG ulId)
{
  PCLIENT	psClient;
  BOOL		bFound = FALSE;

  SysMutexLock( &psSource->hmtxClientsList );

  psClient = psSource->psClientsList;
  while( psClient != NULL )
  {
    if ( ClientGetId( psClient ) == ulId )
    {
      bFound = TRUE;
      break;
    }
    psClient = psClient->psNext;
  }

  SysMutexUnlock( &psSource->hmtxClientsList );

  if ( !bFound )
    return FALSE;

  SourceDetachClient( psSource, psClient );
  return TRUE;
}
