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
#include <stdio.h>
#include <process.h>
#include <httpserv/netresolv.h>
#include "relays.h"
#include "templates.h"
#include "sources.h"
#include "ssmessages.h"

#define NET_MODULE	SS_MODULE_RELAYS

#define RELAYS_ON_TREAD			5
#define MAX_TREADS_NUMB			4
#define RELAY_RECONNECT_TIMEOUT		40000
#define RELAY_READ_TIMEOUT		30000
#define RELAY_CONNECT_TIMEOUT		40000

static PRELAY		psRelaysList = NULL;
static PRELAY		*ppsLastRelay = &psRelaysList;
static RWMTX		rwmtxRelaysList;
static ULONG		ulRelaysCount = 0;
static ULONG		ulThreadsCount = 0;
static ULONG		ulCtrlThreadLock = 0;
static ULONG		ulTimeNow;
static REQFIELDS	sClientFields;


THREADRET THREADCALL RelayThread(PVOID pData);
static ULONG _RelaySourceWrite(PRELAY psRelay, PCHAR pcBuf, ULONG ulBufLen);


static BOOL _HTTPRead(PVOID pObject, PCHAR pcBuf, ULONG ulBufLen, PULONG pulActual)
{
  return FALSE;
}

static BOOL _HTTPWrite(PVOID pObject, PCHAR pcBuf, ULONG ulBufLen, PULONG pulActual)
{
  PRELAY	psRelay = (PRELAY)pObject;
  ULONG		ulRC;
  ULONG		ulLength;
  PCHAR		pcPtr;

  psRelay->ulTimeStamp = ulTimeNow;

  if ( psRelay->psURLFile != NULL )
  {
    *pulActual = ulBufLen;
    ulLength = min( ulBufLen,
                    RELAY_MAX_M3U_LENGTH - psRelay->psURLFile->ulLength - 1 );
    memcpy( &psRelay->psURLFile->acContent[ psRelay->psURLFile->ulLength ],
            pcBuf, ulLength );
    psRelay->psURLFile->ulLength += ulLength;
    psRelay->psURLFile->acContent[ psRelay->psURLFile->ulLength ] = '\0';

    if ( ulLength == 0 )
    {
      pcPtr = strchr( &psRelay->psURLFile->acContent, '\n' );
      if ( pcPtr != NULL )
      {
        if ( (pcPtr != &psRelay->psURLFile->acContent) && (*(pcPtr-1) == '\r') )
          pcPtr--;
        *pcPtr = '\0';
      }
      psRelay->ulState = RELAY_STATE_URL_RECEIVED;
      DETAIL( SS_MESSAGE_RELAY_STATE, psRelay->pszLocation, psRelay->pszMountPoint, "URL RECEIVED (M3U)" );
    }
    return TRUE;
  }

  ulRC = _RelaySourceWrite( psRelay, pcBuf, ulBufLen );
  *pulActual = ulBufLen;

  if ( ulRC == SCRES_OK )
  {
    if ( !psRelay->bNailedUp )
    {
      if ( !SourceQueryPtr( psRelay->psSource ) )
        return FALSE;

      if ( SourceGetClientsCount( psRelay->psSource ) == 0 )
      {
        FormatReset( psRelay->psSource->psFormat );
        SourceRelease( psRelay->psSource );
        psRelay->ulState = RELAY_STATE_WAIT_CLIENTS;
        DETAIL( SS_MESSAGE_RELAY_STATE, psRelay->pszLocation, psRelay->pszMountPoint, "WAIT FOR CLIENTS" );
        return FALSE;
      }

      SourceRelease( psRelay->psSource );
    }
    return TRUE;
  }
  else
  {
    if ( ulRC == SCRES_SOURCE_NOT_EXIST )
    {
      psRelay->ulState = RELAY_STATE_IDLE;
      DBGINFO( SS_MESSAGE_RELAY_STATE, psRelay->pszLocation, psRelay->pszMountPoint, "IDLE" );
    }
    return FALSE;
  }
}

static BOOL _HTTPHeader(PVOID pObject, ULONG ulCode, PREQFIELDS psFields)
{
  PRELAY	psRelay = (PRELAY)pObject;
  ULONG		ulRC;
  PSZ		pszFieldCont;

  if ( ulCode == 200 )
  {
    pszFieldCont = ReqFldGet(psFields,"Content-Type");
    DBGINFO( SS_MESSAGE_RELAY_HTTP_CONTENT_TYPE, psRelay->pszLocation, psRelay->pszMountPoint, pszFieldCont );

    if ( stricmp( pszFieldCont, "audio/x-mpegurl" ) == 0 )
    {
      if ( psRelay->psURLFile == NULL )
        psRelay->psURLFile = malloc( sizeof(RELAYURLFILE) );
      psRelay->psURLFile->ulLength = 0;
      return TRUE;
    }

    pszFieldCont = ReqFldGet( psFields, "icy-metaint" );
    psRelay->ulMetaInt = pszFieldCont != NULL ? atol( pszFieldCont ) : 0;
    psRelay->ulMetaPos = psRelay->ulMetaInt;
    psRelay->ulMetaLen = 0;

    if ( psRelay->psSource != NULL )
//    if ( psRelay->ulState == RELAY_STATE_WAIT_CLIENTS )
    {
      psRelay->ulState = RELAY_STATE_TRANSMIT;
      INFO( SS_MESSAGE_RELAY_STATE, psRelay->pszLocation, psRelay->pszMountPoint, "TRANSMIT" );
      return TRUE;
    }

    ulRC = SourceCreate( &psRelay->psSource, psRelay->pszMountPoint,
                         psFields, NULL, psRelay->psTemplate );
    switch( ulRC )
    {
      case SCRES_OK:
        if ( psRelay->bNailedUp )
        {
          psRelay->ulState = RELAY_STATE_TRANSMIT;
          INFO( SS_MESSAGE_RELAY_STATE, psRelay->pszLocation, psRelay->pszMountPoint, "TRANSMIT (nailed-up)" );
          return TRUE;
        }
        psRelay->ulState = RELAY_STATE_WAIT_CLIENTS;
        INFO( SS_MESSAGE_RELAY_STATE, psRelay->pszLocation, psRelay->pszMountPoint, "WAIT FOR CLIENTS" );
        break;
      case SCRES_INVALID_MPOINT:
        psRelay->ulState = RELAY_STATE_INVALID_MPOINT;
        ERROR( SS_MESSAGE_RELAY_STATE, psRelay->pszLocation, psRelay->pszMountPoint, "INVALID MOUNT POINT" );
        break;
      case SCRES_MPOINT_EXIST:
        psRelay->ulState = RELAY_STATE_MPOINT_EXIST;
        break;
      case SCRES_INVALID_STREAM:
        psRelay->ulState = RELAY_STATE_INVALID_FORMAT;
        break;
      default:
        psRelay->ulState = RELAY_STATE_ERROR_CREATE_MPOINT;
        break;
    }
  }
  else
  {
    if ( ulCode == 404 )
    {
      psRelay->ulState = RELAY_STATE_HTTP_NOT_FOUND;
      WARNING( SS_MESSAGE_RELAY_STATE, psRelay->pszLocation, psRelay->pszMountPoint, "HTTP STATUS CODE 404 (Not Found)" );
    }
    else
    {
      psRelay->ulState = RELAY_STATE_LOCATION_FAILED;
      ERROR( SS_MESSAGE_RELAY_HTTP_STATUS_CODE, psRelay->pszLocation, psRelay->pszMountPoint, (PVOID)(ulCode) );
    }
  }

  return FALSE;
}


static VOID _RelayDestroy(PRELAY psRelay)
{
  if ( psRelay->psHTTPClnt != NULL )
    HTTPClntDestroy( psRelay->psHTTPClnt );

  if ( psRelay->psTemplate != NULL )
    TemplRelease( psRelay->psTemplate );

  if ( psRelay->psSource != NULL )
    SourceDestroy( psRelay->psSource );

  if ( psRelay->pszMountPoint != NULL )
    free( psRelay->pszMountPoint );

  if ( psRelay->psURLFile != NULL )
    free( psRelay->psURLFile );

  if ( psRelay->pszLocation != NULL )
    free( psRelay->pszLocation );

  if ( psRelay->pcMeta != NULL )
    free( psRelay->pcMeta );

  free( psRelay );
}

static VOID _StartThread()
{
  ULONG		ulOldThreadsCount = ulThreadsCount;
  ULONG		ulRC;
  THREAD	hThread;

  ulRC = SysThreadCreate( &hThread, RelayThread, NULL );

  if ( ulRC != RET_OK )
  {
    SysSleep( 0 );
    while ( ulOldThreadsCount == ulThreadsCount )
      SysSleep( 1 );
  }
}

static BOOL _RelayQuery(PRELAY psRelay)
{
  SysMutexLock( &psRelay->hmtxQueryLocks );
  if ( psRelay->ppsSelf == NULL )
  {
    SysMutexUnlock( &psRelay->hmtxQueryLocks );
    return FALSE;
  }
  psRelay->ulQueryLocks++;
  SysMutexUnlock( &psRelay->hmtxQueryLocks );
  return TRUE;
}

static ULONG _RelaySourceWrite(PRELAY psRelay, PCHAR pcBuf, ULONG ulBufLen)
{
  ULONG		ulRC;
  ULONG		ulLength;
  PCHAR		pcPtr;

  if ( psRelay->ulMetaInt == 0 )
  {
    ulRC = SourceWrite( psRelay->psSource, pcBuf, ulBufLen );
    return ulRC;
  }

  while( ulBufLen != 0 )
  {
    if ( psRelay->ulMetaLen != 0 )
    {
      ulLength = min( psRelay->ulMetaLen, ulBufLen );
      memcpy( &psRelay->pcMeta[psRelay->ulMetaPos], pcBuf, ulLength );
      ulBufLen -= ulLength;
      pcBuf += ulLength;
      psRelay->ulMetaLen -= ulLength;
      psRelay->ulMetaPos += ulLength;
      if ( psRelay->ulMetaLen == 0 )
      {
        pcPtr = memchr( &psRelay->pcMeta[14], 0x27, psRelay->ulMetaPos - 14 );
        if ( pcPtr != NULL )
        {
          *pcPtr = '\0';
          SourceSetMeta( psRelay->psSource, &psRelay->pcMeta[13] );
        }
        psRelay->ulMetaPos = psRelay->ulMetaInt;
      }
      continue;
    }

    if ( psRelay->ulMetaPos == 0 )
    {
      psRelay->ulMetaLen = ((ULONG)(pcBuf[0])) << 4;

      if ( (psRelay->ulMetaMaxLen < psRelay->ulMetaLen) ||
           ((psRelay->ulMetaMaxLen - psRelay->ulMetaLen) > 128) )
      {
        if ( psRelay->pcMeta != NULL )
          free( psRelay->pcMeta );
        psRelay->pcMeta = malloc( psRelay->ulMetaLen * 2 );
        psRelay->ulMetaMaxLen = psRelay->ulMetaLen;
      }

      ulBufLen--;
      pcBuf++;

      if ( psRelay->ulMetaLen == 0 )
      {
        psRelay->ulMetaPos = psRelay->ulMetaInt;
      }
      else
        continue;
    }

    ulLength = min( psRelay->ulMetaPos, ulBufLen );
    ulRC = SourceWrite( psRelay->psSource, pcBuf, ulLength );
    if ( ulRC != SCRES_OK )
      return ulRC;

    ulBufLen -= ulLength;
    pcBuf += ulLength;
    psRelay->ulMetaPos -= ulLength;
  }

  return SCRES_OK;
}

static PRELAY _RelayProcess(PRELAY psRelay)
{
  ULONG		ulRC;
  PSZ		pszLocation;

  if ( (psRelay->ulState & RELAY_STATE_ERROR_MASK) != 0 )
  {
    if (
         ( ( psRelay->ulState != RELAY_STATE_LOCATION_FAILED ) &&
           ( psRelay->ulState != RELAY_STATE_NO_TEMPLATE ) )
       &&
         ( (ulTimeNow - psRelay->ulTimeStamp) >= RELAY_RECONNECT_TIMEOUT )
       )
    {
      psRelay->ulState = RELAY_STATE_IDLE;
    }
    
    return psRelay->psNext;
  }

  if ( ( psRelay->ulState == RELAY_STATE_IDLE ) ||
       ( psRelay->ulState == RELAY_STATE_URL_RECEIVED ) ||
       (
         ( psRelay->ulState == RELAY_STATE_WAIT_CLIENTS ) &&
         ( SourceGetClientsCount( psRelay->psSource ) != 0 )
       )
     )
  {
    psRelay->psTemplate = TemplQuery( psRelay->pszMountPoint, NULL, NULL );
    if ( psRelay->psTemplate != NULL )
    {
      psRelay->psHTTPClnt = HTTPClntCreate( (PVOID)psRelay, _HTTPRead,
                                            _HTTPWrite, _HTTPHeader );
      if ( psRelay->psHTTPClnt != NULL )
      {
         if ( psRelay->ulState == RELAY_STATE_URL_RECEIVED )
         {
           pszLocation = &psRelay->psURLFile->acContent;
         }
         else
         {
           pszLocation = psRelay->pszLocation;
         }
         ulRC = HTTPClntRequest( psRelay->psHTTPClnt, "GET",
                                 pszLocation, &sClientFields );
         if ( ulRC == HTTPCLNT_RES_OK )
         {
           psRelay->ulState = RELAY_STATE_CONNECT;
           psRelay->ulTimeStamp = ulTimeNow;
           DBGINFO( SS_MESSAGE_RELAY_STATE, psRelay->pszLocation, psRelay->pszMountPoint, "CONNECTING..." );
         }
         else
         {
           psRelay->ulState = RELAY_STATE_LOCATION_FAILED;
           ERROR( SS_MESSAGE_RELAY_STATE, psRelay->pszLocation, psRelay->pszMountPoint, "INVALID LOCATION" );
         }

//         psRelay->ulState = ulRC == HTTPCLNT_RES_OK ?
//                              RELAY_STATE_CONNECT : RELAY_STATE_LOCATION_FAILED;

         if ( psRelay->psURLFile != NULL )
         {
           free( psRelay->psURLFile );
           psRelay->psURLFile = NULL;
         }
      }
      else
        psRelay->ulState = RELAY_STATE_LOCATION_FAILED;
    }
    else
    {
      psRelay->ulState = RELAY_STATE_NO_TEMPLATE;
      ERROR( SS_MESSAGE_RELAY_STATE, psRelay->pszLocation, psRelay->pszMountPoint, "TEMPLATE NOT FOUND" );
    }

    if ( ( psRelay->ulState != RELAY_STATE_CONNECT )
         && ( psRelay->psSource != NULL ) )
    {
      SourceDestroy( psRelay->psSource );
      psRelay->psSource = NULL;
    }
  }

  if ( ( (psRelay->ulState == RELAY_STATE_WAIT_CLIENTS) ||
       (psRelay->ulState == RELAY_STATE_TRANSMIT) )
     &&
       ( psRelay->psSource->bStop ) )
  {
    SourceDestroy( psRelay->psSource );
    psRelay->psSource = NULL;
    psRelay->ulState = RELAY_STATE_IDLE;
  }

  if ( (psRelay->ulState == RELAY_STATE_TRANSMIT)
     || (psRelay->ulState == RELAY_STATE_CONNECT) )
  {
    ulRC = HTTPClntProcess( psRelay->psHTTPClnt );

    if ( ulRC == HTTPCLNT_RES_OK )
    {
      ULONG	ulDelta = ulTimeNow - psRelay->ulTimeStamp;

      if ( psRelay->ulState == RELAY_STATE_TRANSMIT )
      {
        if ( ulDelta >= RELAY_READ_TIMEOUT )
        {
          psRelay->ulState = RELAY_STATE_TRANSMIT_ERROR;
          ERROR( SS_MESSAGE_RELAY_STATE, psRelay->pszLocation, psRelay->pszMountPoint, "TRANSMIT TIMEOUT" );
        }
      }
      else if ( psRelay->ulState == RELAY_STATE_CONNECT )
      {
        if ( ulDelta >= RELAY_CONNECT_TIMEOUT )
        {
          psRelay->ulState = RELAY_STATE_IDLE;
          ERROR( SS_MESSAGE_RELAY_STATE, psRelay->pszLocation, psRelay->pszMountPoint, "CONNECT TIMEOUT" );
        }
      }
    }

    if ( ( ulRC != HTTPCLNT_RES_OK ) &&
         ( (psRelay->ulState & RELAY_STATE_ERROR_MASK) == 0 ) )
    {
      PSZ	pszComment;

      switch( HTTPClntGetState( psRelay->psHTTPClnt ) )
      {
        case HTTPCLNT_STATE_NAME_NOT_FOUND:
          psRelay->ulState = RELAY_STATE_NAME_NOT_FOUND;
          pszComment = "HOST NAME NOT FOUND";
          break;
        case HTTPCLNT_STATE_CONNECTION_CLOSED:
          psRelay->ulState = RELAY_STATE_CONNECTION_CLOSED;
          pszComment = "CONNECTION CLOSED";
          break;
        case HTTPCLNT_STATE_CONNECTION_REFUSED:
          psRelay->ulState = RELAY_STATE_CONNECTION_REFUSED;
          pszComment = "CONNECTION REFUSED";
          break;
        case HTTPCLNT_STATE_ADDRESS_UNREACH:
          psRelay->ulState = RELAY_STATE_ADDRESS_UNREACH;
          pszComment = "ADDRESS UNREACH";
          break;
        case HTTPCLNT_STATE_ERROR_CREATE_SOCKET:
          pszComment = "CANNOT CREATE SOCKET";
          psRelay->ulState = RELAY_STATE_CANNOT_CONNECT;
          break;
        case HTTPCLNT_STATE_NO_BUFFERS_LEFT:
          pszComment = "NO BUFFERS LEFT";
          psRelay->ulState = RELAY_STATE_CANNOT_CONNECT;
          break;
        case HTTPCLNT_STATE_CANNOT_CONNECT:
          pszComment = "CANNOT CONNECT";
          psRelay->ulState = RELAY_STATE_CANNOT_CONNECT;
          break;
        case HTTPCLNT_STATE_INVALID_RESPONSE:
          pszComment = "INVALID HTTP-RESPONSE";
          psRelay->ulState = RELAY_STATE_INVALID_RESPONSE;
          break;
        default:
          psRelay->ulState = RELAY_STATE_TRANSMIT_ERROR;
          pszComment = "TRANSMIT ERROR";
      }

      ERROR( SS_MESSAGE_RELAY_STATE, psRelay->pszLocation, psRelay->pszMountPoint, pszComment );
    }
    else if ( ( psRelay->ulState == RELAY_STATE_TRANSMIT ) &&
              HTTPClntGetState( psRelay->psHTTPClnt ) == HTTPCLNT_STATE_DONE )
    {
      psRelay->ulState = RELAY_STATE_DISCONNECTED;
      INFO( SS_MESSAGE_RELAY_STATE, psRelay->pszLocation, psRelay->pszMountPoint, "DISCONNECTED" );
    }

    if ( (psRelay->ulState != RELAY_STATE_TRANSMIT) &&
         (psRelay->ulState != RELAY_STATE_CONNECT) )
//    if ( (psRelay->ulState & RELAY_STATE_ERROR_MASK) != 0
//         || psRelay->ulState == RELAY_STATE_IDLE
//         || psRelay->ulState == RELAY_STATE_WAIT_CLIENTS )
    {
      if ( (psRelay->ulState != RELAY_STATE_WAIT_CLIENTS)
           && (psRelay->psURLFile == NULL)
           && (psRelay->psSource != NULL) )
      {
        SourceDestroy( psRelay->psSource );
        psRelay->psSource = NULL;
      }

      if ( ( psRelay->ulState != RELAY_STATE_URL_RECEIVED ) &&
           ( psRelay->psURLFile != NULL ) )
      {
        free( psRelay->psURLFile );
        psRelay->psURLFile = NULL;
      }

      HTTPClntDestroy( psRelay->psHTTPClnt );
      psRelay->psHTTPClnt = NULL;

      psRelay->ulTimeStamp = ulTimeNow;
    }
  }

  return psRelay->psNext;
}


VOID RelayInit()
{
  SysRWMutexCreate( &rwmtxRelaysList );

  ReqFldInit( &sClientFields );
  ReqFldSet( &sClientFields, "User-Agent", SERVER_STRING );
  ReqFldSet( &sClientFields, "icy-metadata", "1" );
}

VOID RelayDone()
{
  RelayRemoveAll();

  while( ulThreadsCount > 0 )
    SysSleep( 1 );

  ReqFldDone( &sClientFields );
  SysRWMutexDestroy( &rwmtxRelaysList );
}

VOID RelayRemoveAll()
{
  SysRWMutexLockWrite( &rwmtxRelaysList );

  while( psRelaysList != NULL )
    RelayDestroy( psRelaysList );

  SysRWMutexUnlockWrite( &rwmtxRelaysList );
}

PRELAY RelayNew()
{
  PRELAY	psRelay;

  psRelay = calloc( 1, sizeof(RELAY) );
  if ( psRelay == NULL )
  {
    ERROR_NOT_ENOUGHT_MEM();
  }

  psRelay->ulId = TemplGetNextId();
  SysMutexCreate( &psRelay->hmtxQueryLocks );

  return psRelay;
}

VOID RelayReady(PRELAY psRelay)
{
  SysRWMutexLockWrite( &rwmtxRelaysList );
  psRelay->ppsSelf = ppsLastRelay;
  *ppsLastRelay = psRelay;
  ppsLastRelay = &psRelay->psNext;
  ulRelaysCount++;
  if ( ulThreadsCount == 0 )
  {
    _StartThread();
  }
  SysRWMutexUnlockWrite( &rwmtxRelaysList );
}

VOID RelaySetMountPoint(PRELAY psRelay, PSZ pszMountPoint)
{
  if ( psRelay->pszMountPoint != NULL )
    free( psRelay->pszMountPoint );

  if ( pszMountPoint != NULL && *pszMountPoint == '/' )
    pszMountPoint++;

  psRelay->pszMountPoint = 
    pszMountPoint == NULL || *pszMountPoint == '\0'
    ? NULL : strdup( pszMountPoint );
}

VOID RelaySetLocation(PRELAY psRelay, PSZ pszLocation)
{
  if ( psRelay->pszLocation != NULL )
    free( psRelay->pszLocation );

  psRelay->pszLocation = 
    pszLocation == NULL || *pszLocation == '\0'
    ? NULL : strdup( pszLocation );
}

VOID RelayDestroy(PRELAY psRelay)
{
  SysMutexLock( &psRelay->hmtxQueryLocks );

  SysRWMutexLockWrite( &rwmtxRelaysList );
  if ( psRelay->ppsSelf != NULL )
  {
    *psRelay->ppsSelf = psRelay->psNext;
    if ( psRelay->psNext != NULL )
    {
      psRelay->psNext->ppsSelf = psRelay->ppsSelf;
      psRelay->psNext = NULL;
    }
    else
      ppsLastRelay = psRelay->ppsSelf;
  }
  psRelay->ppsSelf = NULL;
  ulRelaysCount--;
  SysRWMutexUnlockWrite( &rwmtxRelaysList );

  if ( psRelay->ulQueryLocks == 0 )
  {
    SysMutexUnlock( &psRelay->hmtxQueryLocks );
    _RelayDestroy( psRelay );
    return;
  }

  SysMutexUnlock( &psRelay->hmtxQueryLocks );
}

BOOL RelayReset(PRELAY psRelay)
{
  if ( ( (psRelay->ulState & RELAY_STATE_ERROR_MASK) != 0 )
       && _RelayQuery( psRelay ) )
  {
    psRelay->ulState = RELAY_STATE_IDLE;
    RelayRelease( psRelay );
    return TRUE;
  }

  return FALSE;
}

/*PRELAY RelayQuery(PSZ pszMountPoint)
{
  PRELAY	psRelay;

  if ( pszMountPoint == NULL )
    return NULL;
  if ( pszMountPoint[0] == '/' )
    pszMountPoint++;
  if ( pszMountPoint[0] == '\0' )
    return NULL;

  SysRWMutexLockRead( &rwmtxRelaysList );
  psRelay = psRelaysList;
  while( psRelay != NULL )
  {
    if ( stricmp( pszMountPoint, psRelay->pszMountPoint ) == 0 )
    {
      if ( !_RelayQuery( psRelay ) )
        psRelay = NULL;
      break;
    }
    psRelay = psRelay->psNext;
  }
  SysRWMutexUnlockRead( &rwmtxRelaysList );

  return psRelay;
}*/

PRELAY RelayQueryById(ULONG ulId)
{
  PRELAY	psRelay;

  SysRWMutexLockRead( &rwmtxRelaysList );
  psRelay = psRelaysList;
  while( psRelay != NULL )
  {
    if ( psRelay->ulId == ulId )
    {
      if ( !_RelayQuery( psRelay ) )
        psRelay = NULL;
      break;
    }
    psRelay = psRelay->psNext;
  }
  SysRWMutexUnlockRead( &rwmtxRelaysList );

  return psRelay;
}

BOOL RelayQueryPtr(PRELAY psRelay)
{
  return _RelayQuery( psRelay );
}

VOID RelayRelease(PRELAY psRelay)
{
  if ( psRelay == NULL )
    return;

  SysMutexLock( &psRelay->hmtxQueryLocks );
  if ( psRelay->ulQueryLocks > 0 )
  {
    psRelay->ulQueryLocks--;
    if ( psRelay->ulQueryLocks == 0 && psRelay->ppsSelf == NULL )
    {
      SysMutexUnlock( &psRelay->hmtxQueryLocks );
      _RelayDestroy( psRelay ); // ppsSelf=NULL ==> don't need lock here
      return;
    }
  }
  SysMutexUnlock( &psRelay->hmtxQueryLocks );
}

PRELAY RelayQueryList()
{
  SysRWMutexLockRead( &rwmtxRelaysList );
  return psRelaysList;
}

VOID RelayReleaseList()
{
  SysRWMutexUnlockRead( &rwmtxRelaysList );
}



THREADRET THREADCALL RelayThread(PVOID pData)
{
  ULONG		ulNeedThreads;
  PRELAY	psRelay, psRelayNext;

  ulThreadsCount++;

  while( TRUE )
  {
    if ( lockFlag( &ulCtrlThreadLock ) != 0 )
    {
      SysTime( &ulTimeNow );

      ulNeedThreads = ulRelaysCount == 0 ?
                      0 : ( (ulRelaysCount - 1) / RELAYS_ON_TREAD ) + 1;
      if ( ulNeedThreads > ulThreadsCount )
      {
        if ( ulThreadsCount < MAX_TREADS_NUMB )
          _StartThread();
      }
      else if ( ulNeedThreads < ulThreadsCount )
        break;

      clearFlag( &ulCtrlThreadLock );
    }
//    else
      SysSleep( 100 );

    SysRWMutexLockRead( &rwmtxRelaysList );
    psRelay = psRelaysList;
    while( psRelay != NULL )
    {
      if ( lockFlag( &psRelay->ulLockFlag ) != 0 )
      {
        psRelayNext = _RelayProcess( psRelay );
        clearFlag( &psRelay->ulLockFlag );
        psRelay = psRelayNext;
      }
      else
        psRelay = psRelay->psNext;
    }
    SysRWMutexUnlockRead( &rwmtxRelaysList );
  }

  ulThreadsCount--;
  clearFlag( &ulCtrlThreadLock );

  return THREADRC;
}
