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
#include "system.h"
#include "httpserv\requests.h"
#include "httpserv\netresolv.h"
#include "httpserv\netmessages.h"

#define GET_SOCKETS_BY_ITERATION	16
#define CONNECTION_BUF_SIZE		4096
#define FREE_CONN_STORAGE_MAX_COUNT	8

#define	DEF_CONNECTION_TIMEOUT		(45*1000)
#define	DEF_KEEPALIVE_TIMEOUT		(10*1000)

#define NET_MODULE	NET_MODULE_REQUESTS

typedef struct _CONNECTION {
  struct _CONNECTION	*psNext;
  struct _CONNECTION	**ppsSelf;
  ULONG			ulSocket;
  ULONG			ulBufPos;
  CHAR			acBuf[CONNECTION_BUF_SIZE];

  PCHAR			pcURI;
  PCHAR			pcProto;
  PCHAR			pcVersion;
  REQFIELDS		sFields;

  PVOID			pRequest;
  RESPCODE		rcRespCode;
  ULONG			ulTransmitPos;
  BOOL			bSendEOF;
  BOOL			bKeepAlive;
  BOOL			bChunked;
  BOOL			bClient11;
  BOOL			bLength;
  BOOL			bDead;
  ULONG			ulChunkReadBytes;
  LLONG			llLength;
  ULONG			ulLockFlag;

  ULONG			ulTimeStamp;
  ULONG			ulTimeout;
} CONNECTION, *PCONNECTION;

static NETSERV			sNetHTTPServ;
static BOOL			bReqStop = FALSE;
static PCONNECTION		psConnFreeList = NULL;
static ULONG			ulFreeConnListCount = 0;
static PCONNECTION		psConnList = NULL;
static ULONG			ulConnListCount = 0;
static REQCB			sReq;
static ULONG			ulGetClientSocketsLock = 0;
static RWMTX			rwmtxConnList;
static ULONG			ulThreadsCount = 0;
static ULONG			ulConnectionTimeout = DEF_CONNECTION_TIMEOUT;
static ULONG			ulKeepAliveTimeout = DEF_KEEPALIVE_TIMEOUT;
static ULONG			ulTimeNow;
static PSZ			pszServerString;

#define REQUEST_HEADER_FIELDS_COUNT	(17-1)

static PSZ	pszRequestHeaderFields[REQUEST_HEADER_FIELDS_COUNT] =
{ "Accept", "Accept-Charset", "Accept-Encoding", "Accept-Language",
  "Authorization", "From", "Host", "If-Modified-Since", "If-Match",
  "If-None-Match", "If-Range", "If-Unmodified-Since", "Max-Forwards",
  "Proxy-Authorization", /*"Range",*/ "Referer", "User-Agent" };

//VOID ReqThread(PVOID pData);
THREADRET THREADCALL ReqThread(PVOID pData);

static VOID _ConnectionNew( ULONG ulSocket, ULONG ulTimeNow )
{
  PCONNECTION	psConnection;

  if ( psConnFreeList != NULL )
  {
    psConnection = psConnFreeList;
    psConnFreeList = psConnection->psNext;

    ulFreeConnListCount--;
  }
  else
  {
    psConnection = malloc( sizeof(CONNECTION) );
    if ( psConnection == NULL )
    {
      ERROR_NOT_ENOUGHT_MEM();
      SysSockClose( ulSocket );
      return;
    }
  }

  if ( psConnList != NULL )
    psConnList->ppsSelf = &psConnection->psNext;
  psConnection->psNext = psConnList;
  psConnList = psConnection;
  psConnection->ppsSelf = &psConnList;

  psConnection->ulSocket		= ulSocket;
  psConnection->ulBufPos		= 0;
  psConnection->pcURI			= NULL;
  psConnection->pRequest		= NULL;
  psConnection->rcRespCode		= NULL;
  psConnection->ulTransmitPos		= 0;
  psConnection->ulChunkReadBytes	= 0;
  psConnection->ulLockFlag		= 0;
  psConnection->bDead			= FALSE;
  ReqFldInit( &psConnection->sFields );
  psConnection->ulTimeout		= ulConnectionTimeout;
  psConnection->ulTimeStamp		= ulTimeNow;

  ulConnListCount++;

  DETAIL( NET_MESSAGE_CONNECTION, &ulSocket, NULL, NULL );
}

static VOID _ConnectionMarkDead( PCONNECTION psConnection )
{
  psConnection->bDead = TRUE;
}

static VOID _ConnectionDestroy( PCONNECTION psConnection )
{
  DETAIL( NET_MESSAGE_CONNECTIONCLOSE, &psConnection->ulSocket, NULL, NULL );

  if ( psConnection->pRequest != NULL )
    sReq.cbDestroy( psConnection->pRequest );

  SysSockClose( psConnection->ulSocket );
  ReqFldDone( &psConnection->sFields );

  *psConnection->ppsSelf = psConnection->psNext;
  if ( psConnection->psNext != NULL )
    psConnection->psNext->ppsSelf = psConnection->ppsSelf;
  ulConnListCount--;

  if ( ulFreeConnListCount < FREE_CONN_STORAGE_MAX_COUNT )
  {
    psConnection->psNext = psConnFreeList;
    psConnFreeList = psConnection;
    ulFreeConnListCount++;
  }
  else
  {
    free( psConnection );
  }
}

static BOOL _ChunkedRead(PCONNECTION psConnection)
{
  PCHAR		pcPtr, pcPtr2;
  PCHAR		pcChunkHdrEnd;
  ULONG		ulCount;

  while( psConnection->ulTransmitPos < psConnection->ulBufPos )
  {
    pcPtr = &psConnection->acBuf[ psConnection->ulTransmitPos ];

    if ( psConnection->ulChunkReadBytes == 0 )
    {
      while( *pcPtr == '\r' || *pcPtr == '\n' )
      {
        pcPtr++;
        psConnection->ulTransmitPos++;

        if ( psConnection->ulTransmitPos == psConnection->ulBufPos )
        {
          psConnection->ulTransmitPos = 0;
          psConnection->ulBufPos = 0;
          return TRUE;
        }
      }

      pcChunkHdrEnd = memchr( pcPtr, '\n',
                        psConnection->ulBufPos - psConnection->ulTransmitPos );
      if ( pcChunkHdrEnd == NULL )
      {
        if ( psConnection->ulTransmitPos == 0 &&
             psConnection->ulBufPos == CONNECTION_BUF_SIZE )
        { // error
          return FALSE;
        }

        // need more data
        memcpy( &psConnection->acBuf, pcPtr,
                psConnection->ulBufPos - psConnection->ulTransmitPos );
        psConnection->ulTransmitPos = 0;
        psConnection->ulBufPos -= psConnection->ulTransmitPos;
        return TRUE;
      }
      else
      {
        psConnection->ulChunkReadBytes = strtoul( pcPtr, &pcPtr2, 16 );
        if ( pcPtr == pcPtr2 || ( (*pcPtr2) != ';' && (*pcPtr2) != '\r' ) )
        { // error
          return FALSE;
        }
        psConnection->ulTransmitPos += ((pcChunkHdrEnd+1) - pcPtr);
        pcPtr = pcChunkHdrEnd + 1;
        if ( psConnection->ulTransmitPos == psConnection->ulBufPos )
          break;
      }
    }

    ulCount = min( psConnection->ulBufPos - psConnection->ulTransmitPos,
                   psConnection->ulChunkReadBytes );
    psConnection->rcRespCode = sReq.cbWrite( psConnection->pRequest,
                                             pcPtr, ulCount, &ulCount );
    if ( psConnection->rcRespCode && ulCount == 0 )
      return TRUE;

    psConnection->ulChunkReadBytes -= ulCount;
    psConnection->ulTransmitPos += ulCount;
  }

  psConnection->ulTransmitPos = 0;
  psConnection->ulBufPos = 0;
  return TRUE;
}

static PCONNECTION _ConnectionRead(PCONNECTION psConnection)
{
  ULONG			ulCount;
  PCONNECTION		psConnectionNext = psConnection->psNext;
  ULONG			ulScanPos;
  PCHAR			pcEOH, pcPos;
  BOOL			bDone;
  CONNINFO		sConnInfo;

  if ( !SysSockRead( psConnection->ulSocket,
          &psConnection->acBuf[ psConnection->ulBufPos ],
          CONNECTION_BUF_SIZE - psConnection->ulBufPos, &ulCount ) )
  {
    if ( psConnection->pRequest && 
         !(psConnection->bChunked || psConnection->bLength ) )
    {
      sReq.cbWrite( psConnection->pRequest, NULL, 0, NULL );
    }
    _ConnectionMarkDead( psConnection );

    return psConnectionNext;
  }

/*{
  CHAR	acBuf[4096+1];

  strncpy( acBuf, &psConnection->acBuf[ psConnection->ulBufPos ], ulCount );
  acBuf[ulCount] = '\0';
  printf( "<%s>\n", &acBuf );
}*/

//  if ( ulCount == 0 )
//    return psConnectionNext;

  if ( psConnection->pRequest )
  {
    psConnection->ulBufPos += ulCount;

    if ( psConnection->bChunked )
    {
      if ( !_ChunkedRead( psConnection ) )
      {
        _ConnectionMarkDead( psConnection );
        return psConnectionNext;
      }
    }
    else if ( psConnection->ulBufPos > psConnection->ulTransmitPos )
    {
      ulCount = psConnection->ulBufPos - psConnection->ulTransmitPos;
      if ( psConnection->bLength )
      {
        if ( psConnection->llLength < ulCount )
          ulCount = psConnection->llLength;
      }

      psConnection->rcRespCode = sReq.cbWrite( psConnection->pRequest,
                        &psConnection->acBuf[ psConnection->ulTransmitPos ],
                        ulCount, &ulCount );

      if ( psConnection->bLength && psConnection->llLength > 0 )
      {
        psConnection->llLength -= ulCount;
        if ( psConnection->llLength == 0 &&
             psConnection->rcRespCode == NULL )
        {
          // psConnection->rcRespCode must be returned here (not NULL)!
          psConnection->rcRespCode = sReq.cbWrite( psConnection->pRequest,
                                                   NULL, 0, NULL );
        }
      }

      psConnection->ulTransmitPos += ulCount;

      if ( psConnection->ulBufPos == psConnection->ulTransmitPos )
      {
        psConnection->ulBufPos = 0;
        psConnection->ulTransmitPos = 0;
      }
    }

    if ( psConnection->ulTransmitPos == 0 && psConnection->rcRespCode == NULL )
    {
      if ( SysSockReadReady( psConnection->ulSocket ) )
        psConnectionNext = psConnection; // force transmit
    }
  }
  else if ( ulCount != 0 )
  {
    // skip \r, \n before first request's line
    if ( psConnection->pcURI == NULL && psConnection->ulBufPos == 0 )
    {
      ulScanPos = 0;
      while( (psConnection->acBuf[ulScanPos] == '\r' 
             || psConnection->acBuf[ulScanPos] == '\n') 
             && (ulScanPos < ulCount) )
        ulScanPos++;

      ulCount -= ulScanPos;
      if ( ulCount == 0 )
        return psConnectionNext;
      memcpy( &psConnection->acBuf, &psConnection->acBuf[ulScanPos], ulCount );
    }

    ulScanPos = psConnection->ulBufPos > 6 ? psConnection->ulBufPos - 6 : 0;

    psConnection->ulBufPos += ulCount;
    do {
    l00:
      pcEOH = memchr( &psConnection->acBuf[ulScanPos], '\n',
                      psConnection->ulBufPos - ulScanPos );

      if ( pcEOH == NULL )
      {
        if ( psConnection->pcURI == NULL && 
             psConnection->ulBufPos == CONNECTION_BUF_SIZE )
        {
          psConnection->rcRespCode = RESP_CODE_414; // Request-URI Too Long
        }

        break;
      }

      bDone = FALSE;
      if ( psConnection->pcURI == NULL )
      {
        do
        {
          pcPos = memchr( &psConnection->acBuf, ' ',
                                        pcEOH - &psConnection->acBuf );
          if ( pcPos == NULL ) break;
          *(pcPos++) = '\0';
          psConnection->pcURI = pcPos;
          pcPos = memchr( pcPos, ' ', pcEOH - pcPos );
          if ( pcPos == NULL ) break;
          *(pcPos++) = '\0';
          psConnection->pcProto = pcPos;
          pcPos = memchr( pcPos, '/', pcEOH - pcPos );
          if ( pcPos == NULL ) break;
          *(pcPos++) = '\0';
          psConnection->pcVersion = pcPos;
          pcPos = pcEOH;
          while( *(pcPos-1) == '\r' )
            pcPos--;
          *pcPos = '\0';
          bDone = TRUE;
        }
        while( 0 );

        if ( !bDone )
        {
          _ConnectionMarkDead( psConnection );
          return psConnectionNext;
        }

        psConnection->ulTransmitPos = (pcEOH + 1) - &psConnection->acBuf;
      }
      else
      {
        pcPos = pcEOH;
        if ( *(pcPos-1) == '\r' )
          pcPos--;
        if ( pcPos > &psConnection->acBuf[psConnection->ulTransmitPos] )
        {
          ulCount = pcPos - &psConnection->acBuf[psConnection->ulTransmitPos];
          bDone = ReqFldSetString( &psConnection->sFields,
                             &psConnection->acBuf[psConnection->ulTransmitPos],
                             ulCount );
          if ( !bDone )
          {
            _ConnectionMarkDead( psConnection );
            return psConnectionNext;
          }
        }
        psConnection->ulTransmitPos = (pcEOH + 1) - &psConnection->acBuf;
      }

      pcEOH++;
      if ( *pcEOH == '\r' ) pcEOH++;
      if ( *pcEOH == '\r' ) pcEOH++;

      if ( pcEOH >= &psConnection->acBuf[psConnection->ulBufPos] )
        break;

      if ( *pcEOH == '\n' )
      {
        PCHAR	pcHost;

        *pcEOH = '\0';
        pcEOH++;

        psConnection->bClient11 = strcmp( psConnection->pcVersion, "1.1" ) == 0;
        pcHost = ReqFldGet( &psConnection->sFields, "Host" );

        if ( psConnection->bClient11 && pcHost == NULL ) // RFC2068 9.
        {
          psConnection->rcRespCode = RESP_CODE_400;
          break;
        }

        if ( strnicmp( psConnection->pcURI, "http://", 7 ) == 0 )
        {
          PCHAR		pcRelativeURI;

          pcRelativeURI = strchr( &psConnection->pcURI[7], '/' );
          if ( pcRelativeURI == NULL )
            pcRelativeURI = strchr( &psConnection->pcURI[7], '\0' );

          ulCount = (pcRelativeURI - psConnection->pcURI) - 7;
          memcpy( psConnection->pcURI, &psConnection->pcURI[7], ulCount );
          psConnection->pcURI[ulCount] = '\0';
          pcPos = strchr( psConnection->pcURI, ':' );
          if ( pcPos != NULL )
            *pcPos = '\0';
          ReqFldSet( &psConnection->sFields, "Host", psConnection->pcURI );
          pcHost = psConnection->pcURI;

          psConnection->pcURI = pcRelativeURI;
        }
        else if ( psConnection->pcURI[0] != '/' )
        {
          psConnection->rcRespCode = RESP_CODE_400;
          break;
        }

        ReqFillConnInfo( psConnection->ulSocket, &sConnInfo );
        DBGINFO( NET_MESSAGE_REQUEST, &sConnInfo, psConnection->acBuf,
                psConnection->pcURI );

        psConnection->rcRespCode =
          sReq.cbNew( &psConnection->pRequest, psConnection->pcProto,
                      psConnection->acBuf, psConnection->pcURI,
                      &psConnection->sFields, &sConnInfo );

        DETAIL( NET_MESSAGE_REQUESTRESULT, pcHost,
                psConnection->pcURI, psConnection->rcRespCode );

        if ( psConnection->rcRespCode == NULL )
        {
          if ( psConnection->pRequest == NULL )
          {
            _ConnectionMarkDead( psConnection );
            return psConnectionNext;
          }
          else
          {
            psConnection->ulTransmitPos = pcEOH - &psConnection->acBuf;
          }
        }

        pcPos = ReqFldGet( &psConnection->sFields, "Transfer-Encoding" );
        psConnection->bChunked = pcPos != NULL &&
                                 stricmp( pcPos, "chunked" ) == 0;
        psConnection->bLength = !psConnection->bChunked &&
                  (pcPos = ReqFldGet(&psConnection->sFields,"Content-Length"));
        if ( psConnection->bLength )
        {
          psConnection->llLength = atoll( pcPos ); 
          if ( psConnection->llLength < 0 )
            psConnection->rcRespCode = RESP_CODE_400;
        }

/*        if ( psConnection->bChunked || psConnection->bLength )
        {*/
          if ( pcPos = ReqFldGet( &psConnection->sFields, "Connection" ) )
            psConnection->bKeepAlive = stricmp( pcPos, "Keep-Alive" ) == 0;
          else
            psConnection->bKeepAlive = psConnection->bClient11;
/*        }
        else
          psConnection->bKeepAlive = FALSE;*/

        psConnection->ulTimeout = 0;
      }
      else
      {
        ulScanPos = pcEOH - &psConnection->acBuf;
        goto l00;
      }
    }
    while( 0 );
  }

  if ( psConnection->rcRespCode != NULL )
  {
    REQFIELDS	sRespFields;
    BOOL	bNoMessage;

    pcPos = &psConnection->acBuf;
    memcpy( pcPos, "HTTP/1.1 ", 9 );
    pcPos += 9;
    ulCount = strlen( psConnection->rcRespCode );
    memcpy( pcPos, psConnection->rcRespCode, ulCount );
    pcPos += ulCount;
    *(pcPos++) = '\r';
    *(pcPos++) = '\n';

    ReqFldInit( &sRespFields );
    ReqFldSetCurrentDate( &sRespFields, "Date" );
    ReqFldSet( &sRespFields, "Server", pszServerString );

    if ( strcmp(psConnection->rcRespCode, RESP_CODE_200_ICES) == 0 )
    {
      bNoMessage = TRUE;
      psConnection->rcRespCode = RESP_CODE_100;
    }
    else
      bNoMessage = 
        strcmp( &psConnection->acBuf, "HEAD" ) == 0 ||
        psConnection->rcRespCode[0] == '1' ||			//  1xx
        *(PULONG)(&psConnection->rcRespCode) == 0x20343032 ||	// "204 "
        *(PULONG)(&psConnection->rcRespCode) == 0x20353032 ||	// "205 "(?)
        *(PULONG)(&psConnection->rcRespCode) == 0x20343033;	// "304 "

    if ( psConnection->pRequest == NULL )
    {  // no object was retuned from call-back function - prepare debug message
      psConnection->bKeepAlive = FALSE;
      psConnection->bSendEOF = TRUE;
      ReqFldSet( &sRespFields, "Connection", "close" );
    }
    else
    {
      psConnection->bKeepAlive &= psConnection->rcRespCode[0] == '2';

      psConnection->bSendEOF = bNoMessage;

      ReqFldSet( &sRespFields, "Connection", 
                 psConnection->bKeepAlive ? "Keep-Alive" : "close" );

      sReq.cbGetFields( psConnection->pRequest, psConnection->rcRespCode,
                        &sRespFields );

      psConnection->bKeepAlive &=
        stricmp( ReqFldGet( &sRespFields, "Connection"), "Keep-Alive" ) == 0;

      pcEOH = ReqFldGet( &sRespFields, "Content-Length" );
      if ( pcEOH != NULL )
      {
        psConnection->llLength = atoll( pcEOH ); 
        psConnection->bLength = psConnection->llLength >= 0;
      }

      psConnection->bChunked = psConnection->bClient11;

      if ( psConnection->bChunked )
      {
        ReqFldSet( &sRespFields, "Transfer-Encoding", "chunked" );
      }
      else
        ReqFldDelete( &sRespFields, "Transfer-Encoding" );
    }

    ReqFldToString( &sRespFields, pcPos,
                    CONNECTION_BUF_SIZE - (pcPos - &psConnection->acBuf),
                    &ulScanPos );

    ReqFldClear( &sRespFields );

    pcPos += ulScanPos;
    *(pcPos++) = '\r';
    *(pcPos++) = '\n';

    if ( !bNoMessage && psConnection->pRequest == NULL )
    { // no object was retuned from call-back function - send debug message
      ulCount = strlen( psConnection->rcRespCode );
      memcpy( pcPos, psConnection->rcRespCode, ulCount );
      pcPos += ulCount;
    }

    psConnection->ulBufPos = pcPos - &psConnection->acBuf;
    psConnection->ulTransmitPos = 0;

    psConnectionNext = psConnection; // force _ConnectionWrite();
  }

  return psConnectionNext;
}

static PCONNECTION _ConnectionWrite(PCONNECTION psConnection)
{
  ULONG		ulCount;
  PCONNECTION	psConnectionNext = psConnection->psNext;
  ULONG		ulRC;
  PCHAR		pcPtr;
  ULONG		ulChunkSizeLen;
  ULONG		ulChunkSizeRealLen;

  if ( !psConnection->bSendEOF && psConnection->ulBufPos < CONNECTION_BUF_SIZE )
  do
  {
    pcPtr = &psConnection->acBuf[psConnection->ulBufPos];
    ulCount = CONNECTION_BUF_SIZE - psConnection->ulBufPos;

    if ( psConnection->bChunked )
    {
      if ( ulCount < 64+12 )
        break;

      ulChunkSizeLen = ulCount < 0x100 ? 2 : 3;
      pcPtr += ulChunkSizeLen + 2;
      ulCount -= (ulChunkSizeLen + 9);
    }

    if ( psConnection->bLength )
    {
      if ( ulCount > psConnection->llLength )
        ulCount = psConnection->llLength;
    }

    ulRC = sReq.cbRead( psConnection->pRequest, pcPtr, ulCount, &ulCount );

    if ( ulRC == REQ_READ_CB_ERROR )
    {
      _ConnectionMarkDead( psConnection );
      return psConnectionNext;
    }

    psConnection->bSendEOF = ulRC == REQ_READ_CB_EOF;

    if ( psConnection->bSendEOF && !(psConnection->bLength 
         || psConnection->bChunked) )
    {
      psConnection->bKeepAlive = FALSE;
    }

    if ( psConnection->bLength )
    {
      psConnection->llLength -= ulCount;
      if ( psConnection->llLength <= 0 )
        psConnection->bSendEOF = TRUE;
    }

    if ( psConnection->bChunked )
    {
      if ( ulCount > 0 )
      {
        if ( ulCount < 0x10 )
          ulChunkSizeRealLen = 1;		// N\r\n
        else if ( ulCount < 0x100 )
          ulChunkSizeRealLen = 2;		// NN\r\n
        else
          ulChunkSizeRealLen = 3;		// NNN\r\n

        ulChunkSizeLen -= ulChunkSizeRealLen;
        if ( ulChunkSizeLen != 0 )
        {
          memcpy( pcPtr-ulChunkSizeLen, pcPtr, ulCount );
          pcPtr -= ulChunkSizeLen;
        }

        ultoa( ulCount, pcPtr - (ulChunkSizeRealLen + 2), 16 );
        *(pcPtr-2) = '\r';
        *(pcPtr-1) = '\n';

        pcPtr += ulCount;
        *(pcPtr++) = '\r';
        *(pcPtr++) = '\n';

        ulCount += ulChunkSizeRealLen + 4;
      }

      if ( psConnection->bSendEOF )
      {
        if ( ulCount == 0 )
          pcPtr -= (ulChunkSizeLen + 2);
        memcpy( pcPtr, "0\r\n\r\n", 5 );
        ulCount += 5;
      }
    }

    psConnection->ulBufPos += ulCount;

  }
  while( 0 );

  if ( psConnection->ulBufPos > psConnection->ulTransmitPos )
  {
    if ( !SysSockWrite( psConnection->ulSocket,
            &psConnection->acBuf[ psConnection->ulTransmitPos ],
            psConnection->ulBufPos - psConnection->ulTransmitPos, &ulCount ) )
    {
      _ConnectionMarkDead( psConnection );
      return psConnectionNext;
    }
    psConnection->ulTransmitPos += ulCount;

    if ( psConnection->bSendEOF &&
         psConnection->ulBufPos == psConnection->ulTransmitPos )
    {
      if ( psConnection->rcRespCode[0] == '1' )
      { // 1xx - keep reading after our responce
        psConnection->rcRespCode	= NULL;
        psConnection->ulBufPos		= 0;
        psConnection->ulTransmitPos	= 0;
      }
      else if ( psConnection->bKeepAlive )
      {
        sReq.cbDestroy( psConnection->pRequest );
        psConnection->ulBufPos		= 0;
        psConnection->pcURI		= NULL;
        psConnection->pRequest		= NULL;
        psConnection->rcRespCode	= NULL;
        psConnection->ulTransmitPos	= 0;
//        psConnection->bChunked		= FALSE;
        ReqFldValid( &psConnection->sFields, &pszRequestHeaderFields,
                     REQUEST_HEADER_FIELDS_COUNT );
        psConnection->ulTimeStamp = ulTimeNow;
//        SysTime( &psConnection->ulTimeStamp );
        psConnection->ulTimeout		= ulKeepAliveTimeout;
      }
      else
        _ConnectionMarkDead( psConnection );
    }
//    else if ( SysSockWriteReady( psConnection->ulSocket ) )
    else
    {
      if ( psConnection->ulBufPos == psConnection->ulTransmitPos )
      {
        psConnection->ulBufPos = 0;
        psConnection->ulTransmitPos = 0;
      }
      if ( ulCount > 0 )
        psConnectionNext = psConnection;	// force transmit
    }
  }
  else
    if ( !SysSockConnected(psConnection->ulSocket) )
      _ConnectionMarkDead( psConnection );

  return psConnectionNext;
}


BOOL ReqInit(PREQCB psRequestCB, ULONG ulThreadsInit, PSZ pszServerName)
{
  ULONG		ulRC;
  THREAD	hThread;

  NetResolvInit();
  NetServInit( &sNetHTTPServ, 32 );
  SysRWMutexCreate( &rwmtxConnList );

  sReq = *psRequestCB;
  pszServerString = strdup( pszServerName );

  for( ulRC = 0; ulRC < ulThreadsInit; ulRC++ )
    SysThreadCreate( &hThread, ReqThread, NULL );

  return TRUE;
}

VOID ReqDone()
{
  PCONNECTION	psConnection;
  ULONG		ulIdx;

  bReqStop = TRUE;
  while( ulThreadsCount > 0 )
    SysSleep( 1 );

  while( ulConnListCount > 0 )
    _ConnectionDestroy( psConnList );

  while( psConnFreeList != NULL )
  {
    psConnection = psConnFreeList->psNext;
    free( psConnFreeList );
    psConnFreeList = psConnection;
  }

  SysRWMutexDestroy( &rwmtxConnList );
  NetServDone( &sNetHTTPServ );
  NetResolvDone();

  if ( pszServerString != NULL )
    free( pszServerString );
}

PNETSERV ReqGetNetServ()
{
  return &sNetHTTPServ;
}

BOOL ReqFillConnInfo( ULONG ulSocket, PCONNINFO psConnInfo )
{
  BOOL	bRes;

  bRes =
   SysSockPeerAddr( &ulSocket, &psConnInfo->sClientAddr ) &&
   SysSockAddr( &ulSocket, &psConnInfo->sServAddr );

  if ( bRes )
  {
    psConnInfo->usServPort = SysSockPort( ulSocket );
    return TRUE;
  }

  return FALSE;
}

THREADRET THREADCALL ReqThread(PVOID pData)
{
  ULONG		aulSockets[ GET_SOCKETS_BY_ITERATION ];
  ULONG		ulCount;
  ULONG		ulIdx;
  PCONNECTION	psConnection, psConnectionNext;

  SysRWMutexLockWrite( &rwmtxConnList );
  ulThreadsCount++;
  SysRWMutexUnlockWrite( &rwmtxConnList );

  while( !bReqStop )
  {
    if ( lockFlag( &ulGetClientSocketsLock ) != 0 )
    {
      SysTime( &ulTimeNow );

      SysRWMutexLockWrite( &rwmtxConnList );
      psConnection = psConnList;
      while( psConnection != NULL )
      {
        psConnectionNext = psConnection->psNext;
        if ( lockFlag( &psConnection->ulLockFlag ) != 0 )
        {
          if ( psConnection->bDead || ( psConnection->ulTimeout != 0 && 
               ulTimeNow - psConnection->ulTimeStamp >=
                 psConnection->ulTimeout ) )
          {
            _ConnectionDestroy( psConnection );
          }
          else
            clearFlag( &psConnection->ulLockFlag );
        }
        psConnection = psConnectionNext;
      }
      SysRWMutexUnlockWrite( &rwmtxConnList );

      // get new connections
      NetServGetClientSockets( &sNetHTTPServ, &aulSockets,
                               GET_SOCKETS_BY_ITERATION, 100, &ulCount );

      if ( ulCount != 0 )
      {
        SysRWMutexLockWrite( &rwmtxConnList );
        for( ulIdx = 0; ulIdx < ulCount; ulIdx++ )
          _ConnectionNew( aulSockets[ulIdx], ulTimeNow );
        SysRWMutexUnlockWrite( &rwmtxConnList );
      }

      clearFlag( &ulGetClientSocketsLock );
    }
    else
      SysSleep( 100 );

    SysRWMutexLockRead( &rwmtxConnList );

    // read data from connections
    psConnection = psConnList;
    while( psConnection != NULL )
    {
      if ( !psConnection->bDead &&
           lockFlag( &psConnection->ulLockFlag ) != 0 )
      {
        if ( psConnection->rcRespCode == NULL)
          psConnectionNext = _ConnectionRead( psConnection );
        else
          psConnectionNext = _ConnectionWrite( psConnection );

        clearFlag( &psConnection->ulLockFlag );
        psConnection = psConnectionNext;
      }
      else
        psConnection = psConnection->psNext;
    }

    SysRWMutexUnlockRead( &rwmtxConnList );
  }

  SysRWMutexLockWrite( &rwmtxConnList );
//  if ( lockFlag( &ulGetClientSocketsLock ) != 0 )
  if ( ulThreadsCount == 1 )
  {
    psConnection = psConnList;
    while( psConnection != NULL )
    {
      psConnectionNext = psConnection->psNext;
      _ConnectionDestroy( psConnection );
      psConnection = psConnectionNext;
    }
//    clearFlag( &ulGetClientSocketsLock );
  }
  ulThreadsCount--;
  SysRWMutexUnlockWrite( &rwmtxConnList );

  return THREADRC;
}

PSZ ReqGetServerName()
{
  return pszServerString;
}
