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
#include "shoutserv.h"
#include "httpserv/netserv.h"
#include "httpserv/reqfields.h"
#include "templates.h"

#define AUTHSOURCE_BUF_SIZE		720
#define STREAM_BUF_SIZE			4096
#define AUTHSOURCE_LEVEL_AUTH		0
#define AUTHSOURCE_LEVEL_DATA		1
#define AUTHSOURCE_LEVEL_NO_SOURCE	2
#define	CONNECTION_TIMEOUT		(8*1000)


typedef struct _AUTHSOURCE {
  struct _AUTHSOURCE	*psNext;
  struct _AUTHSOURCE	**ppsSelf;
  ULONG			ulSocket;
  ULONG			ulLevel;
  REQFIELDS		sFields;
  PTEMPLATE		psTemplate;
  union {
    struct _auth {
      ULONG		ulTimeStamp;
      PSZ		pszMountPoint;
      ULONG		ulBufPos;
      ULONG		ulLine;
      BOOL		bSkipNextLine;
      CONNINFO		sConnInfo;
      CHAR		acBuf[AUTHSOURCE_BUF_SIZE];
    };
    PSOURCE		psSource;
  };
} AUTHSOURCE, *PAUTHSOURCE;

static SHOUTMOUNTPOINTSLIST	sShoutMountPointsList = { 0, NULL };
static HMTX			hmtxShoutMountPointsList;
static BOOL			bHaveNewConnect = FALSE;
static PAUTHSOURCE		psAuthSourceList = NULL;
static HMTX			hmtxAuthSourceList;
static PCHAR			pcBufStream = NULL;

static BOOL _AcceptSocketCB(ULONG ulSocket)
{
  USHORT		usPort = SysSockPort( ulSocket );
  ULONG			ulIdx;
  PSHOUTMOUNTPOINT	psShoutMountPoint;
  BOOL			bFound = FALSE;

  SysMutexLock( &hmtxShoutMountPointsList );
  for( ulIdx = 0; ulIdx < sShoutMountPointsList.ulCount; ulIdx++ )
  {
    psShoutMountPoint = &sShoutMountPointsList.pasMountPoints[ulIdx];
    if ( psShoutMountPoint->usPort == usPort )
    {
      if ( psShoutMountPoint->ulSocket == (ULONG)(-1) )
      {
        psShoutMountPoint->ulSocket = ulSocket;
        bHaveNewConnect = TRUE;
      }
      bFound = TRUE;
      break;
    }
  }
  SysMutexUnlock( &hmtxShoutMountPointsList );

  return bFound ? FALSE : TRUE;
}

static PSHOUTMOUNTPOINT _BindListNewItem( PSHOUTMOUNTPOINTSLIST psShoutMountPointsList )
{
  PSHOUTMOUNTPOINT	psShoutMountPoint;

  if ( psShoutMountPointsList->ulCount == 0 ||
       (psShoutMountPointsList->ulCount & 0x07) == 0x07 )
  {
    psShoutMountPoint = realloc( psShoutMountPointsList->pasMountPoints,
                    (((psShoutMountPointsList->ulCount+1) & 0xFFFFFFF8) + 0x08)
                    * sizeof(SHOUTMOUNTPOINT) );
    if ( psShoutMountPoint == NULL )
      return NULL;
    psShoutMountPointsList->pasMountPoints = psShoutMountPoint;
  }

  psShoutMountPoint =
    &psShoutMountPointsList->pasMountPoints[psShoutMountPointsList->ulCount++];

  return psShoutMountPoint;
}

static PAUTHSOURCE _AuthSourceNew(ULONG ulSocket, PSZ pszMountPoint,
                                  ULONG ulTimeNow)
{
  PAUTHSOURCE	psAuthSource;

  psAuthSource = malloc( sizeof(AUTHSOURCE) );
  psAuthSource->ulSocket	= ulSocket;
  psAuthSource->ulLevel		= AUTHSOURCE_LEVEL_AUTH;
  psAuthSource->ulTimeStamp     = ulTimeNow;
  ReqFldInit( &psAuthSource->sFields );
  psAuthSource->pszMountPoint	= strdup( pszMountPoint );
  psAuthSource->ulLine		= 0;
  psAuthSource->ulBufPos	= 0;
  psAuthSource->bSkipNextLine	= FALSE;
  psAuthSource->psTemplate	= NULL;

  if ( psAuthSourceList != NULL )
    psAuthSourceList->ppsSelf = &psAuthSource->psNext;
  psAuthSource->psNext = psAuthSourceList;
  psAuthSourceList = psAuthSource;
  psAuthSource->ppsSelf = &psAuthSourceList;

  return psAuthSource;
}

static PAUTHSOURCE _AuthSourceDestroy(PAUTHSOURCE psAuthSource)
{
  PAUTHSOURCE	psAuthSourceNext = psAuthSource->psNext;

  *psAuthSource->ppsSelf = psAuthSource->psNext;
  if ( psAuthSourceNext != NULL )
    psAuthSourceNext->ppsSelf = psAuthSource->ppsSelf;

  SysSockClose( psAuthSource->ulSocket );

  switch ( psAuthSource->ulLevel )
  {
    case AUTHSOURCE_LEVEL_AUTH:
      if ( psAuthSource->pszMountPoint != NULL )
        free( psAuthSource->pszMountPoint );
      ReqFldDone( &psAuthSource->sFields );
      if ( psAuthSource->psTemplate != NULL )
        TemplRelease( psAuthSource->psTemplate );
      break;
    case AUTHSOURCE_LEVEL_DATA:
      SourceDestroy( psAuthSource->psSource );
      break;
//    case AUTHSOURCE_LEVEL_NO_SOURCE: // nothing to do any more
  }
  free( psAuthSource );

  return psAuthSourceNext;
}

static PAUTHSOURCE _AuthSourceReadAuth(PAUTHSOURCE psAuthSource)
{
  PAUTHSOURCE	psAuthSourceNext = psAuthSource->psNext;
  ULONG		ulCount;
  ULONG		ulScanPos;
  PCHAR		pcPtr, pcStr;
  BOOL		bSkipNextLine;
  BOOL		bBufFull;

  if ( !SysSockRead( psAuthSource->ulSocket,
          &psAuthSource->acBuf[ psAuthSource->ulBufPos ],
          AUTHSOURCE_BUF_SIZE - psAuthSource->ulBufPos, &ulCount ) )
  {
    _AuthSourceDestroy( psAuthSource );
    return psAuthSourceNext;
  }

  if ( ulCount == 0 )
    return psAuthSourceNext;
    

  psAuthSource->ulBufPos += ulCount;
  bBufFull = psAuthSource->ulBufPos == AUTHSOURCE_BUF_SIZE;

  ulScanPos = 0;
  while( 1 )
  {
    // get line
    pcStr = &psAuthSource->acBuf[ulScanPos];
    pcPtr = memchr( pcStr, '\n', psAuthSource->ulBufPos - ulScanPos );
    bSkipNextLine = FALSE;
    if ( pcPtr == NULL )
      if ( ulScanPos == 0 && psAuthSource->ulBufPos == AUTHSOURCE_BUF_SIZE )
      {
        pcPtr = &psAuthSource->acBuf[AUTHSOURCE_BUF_SIZE-1];
        bSkipNextLine = TRUE;
      }
      else
      {
        psAuthSource->ulBufPos -= ulScanPos;
        memcpy( &psAuthSource->acBuf, pcStr, psAuthSource->ulBufPos );
        break;
      }

    ulScanPos = (pcPtr - &psAuthSource->acBuf) + 1;

    if ( psAuthSource->bSkipNextLine )
    {
      psAuthSource->bSkipNextLine = bSkipNextLine;
      continue;
    }
    psAuthSource->bSkipNextLine = bSkipNextLine;

    while( pcPtr > pcStr && ( *(pcPtr-1) == '\r' || *(pcPtr-1) == '\n' ) )
      pcPtr--;
    *pcPtr = '\0';
    if ( psAuthSource->ulLine++ == 100 )
    {
      _AuthSourceDestroy( psAuthSource );
      return psAuthSourceNext;
    }

    // read line
    if ( psAuthSource->ulLine == 1 )
    {
      PSZ		pszMountPoint = psAuthSource->pszMountPoint;

      ReqFillConnInfo( psAuthSource->ulSocket, &psAuthSource->sConnInfo );
      psAuthSource->psTemplate = TemplQuery( pszMountPoint,
                                             &psAuthSource->sConnInfo, pcStr );

      if ( psAuthSource->psTemplate != NULL )
      {
        if ( !SysSockWrite( psAuthSource->ulSocket, "OK2\r\nicy-caps:11\r\n\r\n",
                            20, &ulCount ) )
          _AuthSourceDestroy( psAuthSource );
      }
      else
      {
        ulCount = strlen( pcStr );
        if ( (AUTHSOURCE_BUF_SIZE - ulCount) >= 4 )
        {
          *(PULONG)&pcStr[ulCount] = 0x00000A0D;
          ulCount += 2;
        }
        SysSockWrite( psAuthSource->ulSocket, pcStr, ulCount, &ulCount );
        _AuthSourceDestroy( psAuthSource );
      }

      return psAuthSourceNext;
    }
    else if ( *pcStr != '\0' )
    {
      if ( !ReqFldSetString( &psAuthSource->sFields, pcStr, pcPtr - pcStr )
           && psAuthSource->ulLine > 2 )
      {
        _AuthSourceDestroy( psAuthSource );
        return psAuthSourceNext;
      }
    }
    else
    {
      PSZ		pszMountPoint = psAuthSource->pszMountPoint;
      ULONG		ulRC;
      ULONG		ulDataLength = psAuthSource->ulBufPos - ulScanPos;

      memcpy( pcBufStream, &psAuthSource->acBuf[ulScanPos], ulDataLength );

      if ( ReqFldGet( &psAuthSource->sFields, "Content-Type" ) == NULL )
        ReqFldSet( &psAuthSource->sFields, "Content-Type", "audio/mpeg" );

      ulRC = SourceCreate( &psAuthSource->psSource, pszMountPoint,
                           &psAuthSource->sFields, &psAuthSource->sConnInfo,
                           psAuthSource->psTemplate );

      TemplRelease( psAuthSource->psTemplate );

      // end of AUTHSOURCE_LEVEL_AUTH phase
      psAuthSource->psTemplate = NULL;
      if ( pszMountPoint != NULL )
        free( pszMountPoint );
      ReqFldDone( &psAuthSource->sFields );
      // next phase - AUTHSOURCE_LEVEL_DATA

      if ( ulRC != SCRES_OK )
      {
        psAuthSource->ulLevel = AUTHSOURCE_LEVEL_NO_SOURCE;
        _AuthSourceDestroy( psAuthSource );
        return psAuthSourceNext;
      }

      psAuthSource->ulLevel = AUTHSOURCE_LEVEL_DATA;

      if ( ulDataLength > 0 )
      {
        ulRC = SourceWrite( psAuthSource->psSource, pcBufStream, ulDataLength );
        if ( ulRC != SCRES_OK )
        {
          _AuthSourceDestroy( psAuthSource );
          return psAuthSourceNext;
        }
      }

      break;
    }
  } // while

  return bBufFull ? psAuthSource : psAuthSourceNext;
}

static PAUTHSOURCE _AuthSourceReadData(PAUTHSOURCE psAuthSource)
{
  PAUTHSOURCE	psAuthSourceNext = psAuthSource->psNext;
  ULONG		ulCount;
  ULONG		ulRC;

  if ( SysSockRead( psAuthSource->ulSocket, pcBufStream, STREAM_BUF_SIZE,
                     &ulCount ) )
  {
    if ( ulCount == 0 )
      return psAuthSourceNext;

    ulRC = SourceWrite( psAuthSource->psSource, pcBufStream, ulCount );  
    if ( ulRC == SCRES_OK )
      return ulCount == STREAM_BUF_SIZE ? psAuthSource : psAuthSourceNext;
  }

  _AuthSourceDestroy( psAuthSource );
  return psAuthSourceNext;
}


VOID ShoutServInit()
{
  SysMutexCreate( &hmtxShoutMountPointsList );
  SysMutexCreate( &hmtxAuthSourceList );
  NetServSetAcceptSocketCB( _AcceptSocketCB );
}

VOID ShoutServDone()
{
  NetServSetAcceptSocketCB( NULL );

  SysMutexLock( &hmtxShoutMountPointsList );
  ShoutServMountPointsListCancel( &sShoutMountPointsList );
  SysMutexUnlock( &hmtxShoutMountPointsList );

  SysMutexLock( &hmtxAuthSourceList );
  while( psAuthSourceList != NULL )
    _AuthSourceDestroy( psAuthSourceList );
  SysMutexUnlock( &hmtxAuthSourceList );

  if ( pcBufStream != NULL )
    free( pcBufStream );

  SysMutexDestroy( &hmtxAuthSourceList );
  SysMutexDestroy( &hmtxShoutMountPointsList );
}

ULONG ShoutServMountPointsListAdd(PSHOUTMOUNTPOINTSLIST psShoutMountPointsList,
                                  USHORT usPort, PSZ pszMountPoint)
{
  PSHOUTMOUNTPOINT	psShoutMountPoint;

  if ( pszMountPoint == NULL || usPort == 0 )
    return SHRET_INVALID_PARAM;
  if ( pszMountPoint[0] == '/' )
    pszMountPoint++;
  if ( pszMountPoint[0] == '\0' )
    return SHRET_INVALID_PARAM;

  pszMountPoint = strdup( pszMountPoint );
  if ( pszMountPoint == NULL )
    return SHRET_NOT_ENOUGH_MEMORY;

  psShoutMountPoint = _BindListNewItem( psShoutMountPointsList );
  if ( psShoutMountPoint == NULL )
  {
    free( pszMountPoint );
    return SHRET_NOT_ENOUGH_MEMORY;
  }

  psShoutMountPoint->pszMountPoint = strdup( pszMountPoint );
  psShoutMountPoint->usPort = usPort;
  psShoutMountPoint->ulSocket = (ULONG)(-1);

  return SHRET_OK;
}

VOID ShoutServMountPointsListCancel(PSHOUTMOUNTPOINTSLIST psShoutMountPointsList)
{
  ULONG			ulIdx;
  PSHOUTMOUNTPOINT	psShoutMountPoint;

  for( ulIdx = 0; ulIdx < psShoutMountPointsList->ulCount; ulIdx++ )
  {
    psShoutMountPoint = &psShoutMountPointsList->pasMountPoints[ulIdx];
    if ( psShoutMountPoint->ulSocket != (ULONG)(-1) )
      SysSockClose( psShoutMountPoint->ulSocket );
    free( psShoutMountPoint->pszMountPoint );
  }
  free( psShoutMountPointsList->pasMountPoints );
  psShoutMountPointsList->ulCount = 0;
  psShoutMountPointsList->pasMountPoints = NULL;
}

VOID ShoutServMountPointsListApply(PSHOUTMOUNTPOINTSLIST psShoutMountPointsList)
{
  ULONG		ulRC;

  SysMutexLock( &hmtxShoutMountPointsList );

  if ( pcBufStream == NULL )
    pcBufStream = malloc( STREAM_BUF_SIZE );

  ShoutServMountPointsListCancel( &sShoutMountPointsList );
  sShoutMountPointsList = *psShoutMountPointsList;
  bHaveNewConnect = FALSE;

  SysMutexUnlock( &hmtxShoutMountPointsList );
}

PSOURCE ShoutServQuerySource(USHORT usPort, struct in_addr *psClientAddr)
{
  PAUTHSOURCE		psAuthSource = NULL;
  PSOURCE		psSource = NULL;
  struct in_addr	sInAddr;

  SysMutexLock( &hmtxAuthSourceList );
  psAuthSource = psAuthSourceList;
  while( psAuthSource != NULL )
  {
    if ( psAuthSource->ulLevel == AUTHSOURCE_LEVEL_DATA &&
         usPort == SysSockPort( psAuthSource->ulSocket ) )
    {
      if ( SysSockPeerAddr( &psAuthSource->ulSocket, &sInAddr) &&
           *(PULONG)&sInAddr == *(PULONG)psClientAddr &&
           SourceQueryPtr( psAuthSource->psSource ) )
      {
        psSource = psAuthSource->psSource;
      }
      break;
    }

    psAuthSource = psAuthSource->psNext;
  }
  SysMutexUnlock( &hmtxAuthSourceList );

  return psSource;
}


VOID ShoutLoop()
{
  PSHOUTMOUNTPOINT	psShoutMountPoint;
  ULONG			ulIdx;
  PAUTHSOURCE		psAuthSource;
  ULONG			ulTimeNow;

  if ( pcBufStream == NULL )
    return;

  SysTime( &ulTimeNow );

  SysMutexLock( &hmtxShoutMountPointsList );
  if ( bHaveNewConnect )
  {
    bHaveNewConnect = FALSE;
    for( ulIdx = 0; ulIdx < sShoutMountPointsList.ulCount; ulIdx++ )
    {
      psShoutMountPoint = &sShoutMountPointsList.pasMountPoints[ulIdx];
      if ( psShoutMountPoint->ulSocket != (ULONG)(-1) )
      {
        SysMutexLock( &hmtxAuthSourceList );
        _AuthSourceNew( psShoutMountPoint->ulSocket,
                        psShoutMountPoint->pszMountPoint, ulTimeNow );
        SysMutexUnlock( &hmtxAuthSourceList );
        psShoutMountPoint->ulSocket = (ULONG)(-1);
      }
    }
  }
  SysMutexUnlock( &hmtxShoutMountPointsList );

  SysMutexLock( &hmtxAuthSourceList );
  psAuthSource = psAuthSourceList;
  while( psAuthSource != NULL )
  {
    if ( psAuthSource->ulLevel == AUTHSOURCE_LEVEL_AUTH )
    {
      if ( (ulTimeNow - psAuthSource->ulTimeStamp) > CONNECTION_TIMEOUT )
      {
        psAuthSource = _AuthSourceDestroy( psAuthSource );
      }
      else
        psAuthSource = _AuthSourceReadAuth( psAuthSource );
    }
    else
      psAuthSource = _AuthSourceReadData( psAuthSource );
  }
  SysMutexUnlock( &hmtxAuthSourceList );
}

BOOL ShoutServGetMountPoint(USHORT usPort, PCHAR pcBuf, ULONG ulBufLen)
{
  PSHOUTMOUNTPOINT	psShoutMountPoint;
  BOOL			bFound = FALSE;
  ULONG			ulIdx;

  SysMutexLock( &hmtxShoutMountPointsList );
  for( ulIdx = 0; ulIdx < sShoutMountPointsList.ulCount; ulIdx++ )
  {
    psShoutMountPoint = &sShoutMountPointsList.pasMountPoints[ulIdx];
    if ( psShoutMountPoint->usPort == usPort )
    {
      if ( pcBuf != NULL )
        strncpy( pcBuf, psShoutMountPoint->pszMountPoint, ulBufLen );
      bFound = TRUE;
      break;
    }
  }
  SysMutexUnlock( &hmtxShoutMountPointsList );

  return bFound;
}
