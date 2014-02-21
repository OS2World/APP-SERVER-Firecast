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
#include "httpserv\httpclnt.h"
#include "httpserv\netresolv.h"
#include "httpserv\netmessages.h"

#define CLNT_FLAG_REQ_BODY		0x0001
#define CLNT_FLAG_EOF			0x0002
#define CLNT_FLAG_STATUS_LINE		0x0004
#define CLNT_FLAG_RESP_BODY_LENGTH	0x0008
#define CLNT_FLAG_RESP_TRANSFER_ENC	0x0010
#define CLNT_FLAG_RESP_CHUNKED		0x0020

#define NET_MODULE	NET_MODULE_HTTPCLNT

#define HTTPCLNT_SET_STATE(a) psHTTPClnt->ulState = a

static VOID _CloseConnection(PHTTPCLNT psHTTPClnt, ULONG ulState)
{
  SysSockClose( psHTTPClnt->ulSocket );
  psHTTPClnt->ulSocket = (ULONG)(-1);
  HTTPCLNT_SET_STATE( ulState );
}

static BOOL _ChunkedRead(PHTTPCLNT psHTTPClnt, PULONG pulActual)
{
  PCHAR		pcPtr, pcPtr2;
  PCHAR		pcChunkHdrEnd;
  ULONG		ulCount;

  *pulActual = 0;
  if ( (psHTTPClnt->ulFlags & CLNT_FLAG_EOF) != 0 )
    return FALSE;

  while( psHTTPClnt->ulTransmitPos < psHTTPClnt->ulBufPos )
  {
    pcPtr = &psHTTPClnt->acBuf[ psHTTPClnt->ulTransmitPos ];

    if ( psHTTPClnt->ulChunkBytes == 0 )
    {
      while( *pcPtr == '\r' || *pcPtr == '\n' )
      {
        pcPtr++;
        psHTTPClnt->ulTransmitPos++;

        if ( psHTTPClnt->ulTransmitPos == psHTTPClnt->ulBufPos )
          return TRUE;
      }

      pcChunkHdrEnd = memchr( pcPtr, '\n',
                        psHTTPClnt->ulBufPos - psHTTPClnt->ulTransmitPos );
      if ( pcChunkHdrEnd == NULL )
      { // need more data
        if ( psHTTPClnt->ulTransmitPos == 0 )
        {
          return psHTTPClnt->ulBufPos != CLNT_CONNECTION_BUF_SIZE;
        }

        memcpy( &psHTTPClnt->acBuf, pcPtr,
                psHTTPClnt->ulBufPos - psHTTPClnt->ulTransmitPos );
        psHTTPClnt->ulTransmitPos = 0;
        psHTTPClnt->ulBufPos -= psHTTPClnt->ulTransmitPos;
        return TRUE;
      }
      else
      {
        psHTTPClnt->ulChunkBytes = strtoul( pcPtr, &pcPtr2, 16 );
        if ( pcPtr == pcPtr2 || ( (*pcPtr2) != ';' && (*pcPtr2) != '\r' ) )
        { // error
          return FALSE;
        }
        psHTTPClnt->ulTransmitPos += ((pcChunkHdrEnd+1) - pcPtr);

        if ( psHTTPClnt->ulChunkBytes == 0 )
        {
          psHTTPClnt->ulTransmitPos = psHTTPClnt->ulBufPos;
          HTTPCLNT_SET_STATE( HTTPCLNT_STATE_DONE );
          psHTTPClnt->ulFlags |= CLNT_FLAG_EOF;
          return TRUE;
        }

        pcPtr = pcChunkHdrEnd + 1;
        if ( psHTTPClnt->ulTransmitPos == psHTTPClnt->ulBufPos )
          break;
      }
    }

    ulCount = min( psHTTPClnt->ulBufPos - psHTTPClnt->ulTransmitPos,
                   psHTTPClnt->ulChunkBytes );
    if ( !psHTTPClnt->cbWrite( psHTTPClnt->pObject, pcPtr, ulCount, &ulCount ) )
    {
      HTTPCLNT_SET_STATE( HTTPCLNT_STATE_DONE );
      return TRUE;
    }
    if ( ulCount == 0 )
      return TRUE;
    *pulActual += ulCount;

    psHTTPClnt->ulChunkBytes -= ulCount;
    psHTTPClnt->ulTransmitPos += ulCount;
  }

//  psHTTPClnt->ulTransmitPos = 0;
//  psHTTPClnt->ulBufPos = 0;
  return TRUE;
}



PHTTPCLNT HTTPClntCreate(PVOID pObject,
                         PHTTPCLNTREADCALLBACK cbRead,
                         PHTTPCLNTWRITECALLBACK cbWrite,
                         PHTTPCLNTHEADERCALLBACK cbHeader)
{
  PHTTPCLNT	psHTTPClnt;

  psHTTPClnt = calloc( 1, sizeof(HTTPCLNT) );
  if ( psHTTPClnt == NULL )
  {
    ERROR_NOT_ENOUGHT_MEM();
    return NULL;
  }
  psHTTPClnt->pObject = pObject;
  psHTTPClnt->cbRead  = cbRead;
  psHTTPClnt->cbWrite = cbWrite;
  psHTTPClnt->cbHeader = cbHeader;
  HTTPCLNT_SET_STATE( HTTPCLNT_STATE_DONE );
  psHTTPClnt->ulSocket = (ULONG)(-1);
  ReqFldInit( &psHTTPClnt->sFields );

  return psHTTPClnt;
}

VOID HTTPClntDestroy(PHTTPCLNT psHTTPClnt)
{
  if ( psHTTPClnt->ulSocket != (ULONG)(-1) )
    SysSockClose( psHTTPClnt->ulSocket );

  if ( psHTTPClnt->ulState == HTTPCLNT_STATE_SEARCH_NAME )
    NetRemoveObject( (PVOID)psHTTPClnt );

  if ( psHTTPClnt->pszAddr != NULL )
    free( psHTTPClnt->pszAddr );

  ReqFldDone( &psHTTPClnt->sFields );

  free( psHTTPClnt );
}

VOID HTTPClntCBResolv(PVOID pObject, BOOL bFound, PSZ pszAddr, struct in_addr *psInAddr)
{
  PHTTPCLNT	psHTTPClnt = (PHTTPCLNT)pObject;
  ULONG		ulRC;

  if ( !bFound )
  {
    HTTPCLNT_SET_STATE( HTTPCLNT_STATE_NAME_NOT_FOUND );
    return;
  }

  ulRC = SysCreateClientSock( &psHTTPClnt->ulSocket,
                              psInAddr, psHTTPClnt->usPort );

  switch( ulRC )
  {
    case RET_OK:
      HTTPCLNT_SET_STATE( HTTPCLNT_STATE_SEND_HEADER );
      break;
    case RET_WAIT_CONNECT:
      HTTPCLNT_SET_STATE( HTTPCLNT_STATE_CONNECTING );
      break;
    case RET_ERROR_CREATE_SOCKET:
      HTTPCLNT_SET_STATE( HTTPCLNT_STATE_ERROR_CREATE_SOCKET );
      break;
    default:
      SysSockClose( psHTTPClnt->ulSocket );
      switch( ulRC )
      {
        case RET_REFUSED:
          HTTPCLNT_SET_STATE( HTTPCLNT_STATE_CONNECTION_REFUSED );
          break;
        case RET_UNREACH:
          HTTPCLNT_SET_STATE( HTTPCLNT_STATE_ADDRESS_UNREACH );
          break;
        case RET_NO_BUFFERS:
          HTTPCLNT_SET_STATE( HTTPCLNT_STATE_NO_BUFFERS_LEFT );
          break;
        default: // RET_CANNOT_CONNECT
          HTTPCLNT_SET_STATE( HTTPCLNT_STATE_CANNOT_CONNECT );
      }
  }
}

ULONG HTTPClntRequest(PHTTPCLNT psHTTPClnt, PSZ pszMethod, PSZ pszURL,
                      PREQFIELDS psFields)
{
  PCHAR			pcPtr;
  PSZ			pszPath;
  PCHAR			pcLogin;
  ULONG			ulLoginLen;
  PCHAR			pcAddr;
  ULONG			ulAddrLen;
  PCHAR			pcPort;
  LONG			lPort;
  struct in_addr	sInAddr;
  LONG			lCount;
  LONG			lActual;
  CHAR			acAddr[128];

  if ( ( pszURL == NULL ) || ( strnicmp( pszURL, "http://", 7 ) != 0 ) )
    return HTTPCLNT_RES_INVALID_URL;

  pszURL += 7;
  pcPtr = strchr( pszURL, '/' );
  if ( pcPtr == NULL )
  {
    pcPtr = strchr( pszURL, '\0' );
    pszPath = pcPtr;
  }
  else
    pszPath = pcPtr + 1;

  pcAddr = memchr( pszURL, '@', (pcPtr - pszURL) );
  if ( pcAddr == NULL )
  {
    pcAddr = pszURL;
    pcLogin = NULL;
    ulLoginLen = 0;
  }
  else
  {
    ulLoginLen = pcAddr - pszURL;
    pcLogin = pcAddr == pszURL ? NULL : pszURL;
    pcAddr++;
  }

  pcPort = memchr( pcAddr, ':', (pcPtr - pcAddr) );
  if ( pcPort == NULL )
  {
    ulAddrLen = pcPtr - pcAddr;
    lPort = 80;
  }
  else
  {
    if ( pcPort == pcAddr )
      return HTTPCLNT_RES_INVALID_URL;

    ulAddrLen = pcPort - pcAddr;
    pcPort++;
    lPort = strtol( pcPort, &pcPtr, 10 );
    if ( lPort <= 0 || lPort > 0xFFFF || (*pcPtr != '\0' && *pcPtr != '/') )
      return HTTPCLNT_RES_INVALID_URL;
  }

  if ( ulAddrLen >= sizeof(acAddr) )
    return HTTPCLNT_RES_INVALID_URL;
  memcpy( &acAddr, pcAddr, ulAddrLen );
  acAddr[ulAddrLen] = '\0';

  if (
       (
         (
           ( strncmp( pszMethod, "POST", 4 ) == 0 )
         || 
           ( strncmp( pszMethod, "PUT", 3 ) == 0 )
         )
       &&
         (
           ( ReqFldGet( psFields, "Content-Length" ) != NULL )
         ||
           ( ReqFldGet( psFields, "Transfer-Encoding" ) != NULL )
         )
       )
     ||
       ( strncmp( pszMethod, "SOURCE", 6 ) == 0 )
     )
  {
    psHTTPClnt->ulFlags = CLNT_FLAG_REQ_BODY;
  }
  else
    psHTTPClnt->ulFlags = 0;

  lCount = snprintf( &psHTTPClnt->acBuf, CLNT_CONNECTION_BUF_SIZE - 2,
                     "%s /%s HTTP/1.1\r\n", pszMethod, pszPath );
  if ( lCount < 0 )
    return HTTPCLNT_RES_INVALID_URL;

  lActual = snprintf( &psHTTPClnt->acBuf[lCount],
                     ( CLNT_CONNECTION_BUF_SIZE - 2 ) - lCount,
                     "Host: %s\r\n", &acAddr );
  if ( lActual < 0 )
    return HTTPCLNT_RES_INVALID_URL;

  lCount += lActual;

  if ( psFields != NULL )
  {
    ReqFldToString( psFields, &psHTTPClnt->acBuf[lCount],
                    ( CLNT_CONNECTION_BUF_SIZE - 2 ) - lCount, (PULONG)&lActual );
    lCount += lActual;
  }

  strcat( &psHTTPClnt->acBuf[lCount], "\r\n" );
  psHTTPClnt->ulBufPos = lCount + 2;
  psHTTPClnt->ulTransmitPos = 0;
  psHTTPClnt->usPort = lPort;
  ReqFldClear( &psHTTPClnt->sFields );

  if ( psHTTPClnt->ulSocket != (ULONG)(-1) )
  {
    if ( ( psHTTPClnt->ulState == HTTPCLNT_STATE_DONE )
         && SysSockConnected( psHTTPClnt->ulSocket )
         && ( psHTTPClnt->pszAddr != NULL )
         && ( stricmp(psHTTPClnt->pszAddr, &acAddr) == 0 ) )
    {
      HTTPCLNT_SET_STATE( HTTPCLNT_STATE_SEND_HEADER );
      return HTTPCLNT_RES_OK;
    }

    SysSockClose( psHTTPClnt->ulSocket );
    psHTTPClnt->ulSocket = (ULONG)(-1);
  }

  if ( psHTTPClnt->pszAddr != NULL )
    free( psHTTPClnt->pszAddr );
  psHTTPClnt->pszAddr = strdup( &acAddr );

  if ( !NetResolv( (PVOID)psHTTPClnt, psHTTPClnt->pszAddr, HTTPClntCBResolv, &sInAddr ) )
  {
    HTTPCLNT_SET_STATE( HTTPCLNT_STATE_SEARCH_NAME );
    return HTTPCLNT_RES_OK;
  }

  HTTPClntCBResolv( (PVOID)psHTTPClnt, TRUE, NULL, &sInAddr);

  return (psHTTPClnt->ulState & HTTPCLNT_STATE_ERROR_MASK) == 0 ?
           HTTPCLNT_RES_OK : HTTPCLNT_RES_ERROR;
}

ULONG HTTPClntProcess(PHTTPCLNT psHTTPClnt)
{
  ULONG		ulRC;
  ULONG		ulCount;
  ULONG		ulLength;
  ULONG		ulScanPos;
  PCHAR         pcEOH, pcPtr;

  if ( (psHTTPClnt->ulState & HTTPCLNT_STATE_ERROR_MASK) != 0 )
    return HTTPCLNT_RES_ERROR;

  if ( psHTTPClnt->ulState == HTTPCLNT_STATE_CONNECTING )
  {
    ulRC = SysWaitClientSock( &psHTTPClnt->ulSocket );
    switch( ulRC )
    {
      case RET_WAIT_CONNECT:
        return HTTPCLNT_RES_OK;
      case RET_CANNOT_CONNECT:
        _CloseConnection( psHTTPClnt, HTTPCLNT_STATE_CANNOT_CONNECT );
        return HTTPCLNT_RES_ERROR;
      default:
        HTTPCLNT_SET_STATE( HTTPCLNT_STATE_SEND_HEADER );
    }
  }

  if ( psHTTPClnt->ulState == HTTPCLNT_STATE_SEND_HEADER )
  {
    if ( !SysSockWrite( psHTTPClnt->ulSocket,
                        &psHTTPClnt->acBuf[psHTTPClnt->ulTransmitPos],
                        psHTTPClnt->ulBufPos - psHTTPClnt->ulTransmitPos,
                        &ulCount ) )
    {
      _CloseConnection( psHTTPClnt, HTTPCLNT_STATE_CONNECTION_CLOSED );
      return HTTPCLNT_RES_ERROR;
    }

    psHTTPClnt->ulTransmitPos += ulCount;
    if ( psHTTPClnt->ulTransmitPos != psHTTPClnt->ulBufPos )
      return HTTPCLNT_RES_OK;

    psHTTPClnt->ulTransmitPos = 0;
    psHTTPClnt->ulBufPos = 0;

    HTTPCLNT_SET_STATE(
      psHTTPClnt->ulFlags & CLNT_FLAG_REQ_BODY != 0 ?
      HTTPCLNT_STATE_SEND_BODY : HTTPCLNT_STATE_READ_HEADER );
  }

  if ( psHTTPClnt->ulState == HTTPCLNT_STATE_SEND_BODY )
  {
    if ( (psHTTPClnt->ulFlags & CLNT_FLAG_EOF) == 0 )
    {
l01:
      ulLength = CLNT_CONNECTION_BUF_SIZE - psHTTPClnt->ulBufPos;
      if ( ulLength != 0 )
      {
        if ( !psHTTPClnt->cbRead( psHTTPClnt->pObject,
             &psHTTPClnt->acBuf[psHTTPClnt->ulBufPos], ulLength, &ulCount ) )
        {
          psHTTPClnt->ulFlags |= CLNT_FLAG_EOF;
        }
        else
          psHTTPClnt->ulBufPos += ulCount;
      }
    }

    ulLength = psHTTPClnt->ulBufPos - psHTTPClnt->ulTransmitPos;
    if ( ulLength != 0 )
    {
      if ( !SysSockWrite( psHTTPClnt->ulSocket,
              &psHTTPClnt->acBuf[psHTTPClnt->ulTransmitPos],
              ulLength, &ulCount) )
      {
        _CloseConnection( psHTTPClnt, HTTPCLNT_STATE_CONNECTION_CLOSED );
        return HTTPCLNT_RES_ERROR;
      }

      psHTTPClnt->ulTransmitPos += ulCount;

      if ( psHTTPClnt->ulTransmitPos == psHTTPClnt->ulBufPos )
      {
        if ( (psHTTPClnt->ulFlags & CLNT_FLAG_EOF) != 0 )
        {
          psHTTPClnt->ulTransmitPos = 0;
          psHTTPClnt->ulBufPos = 0;

          psHTTPClnt->ulFlags &= ~CLNT_FLAG_EOF;
          HTTPCLNT_SET_STATE( HTTPCLNT_STATE_READ_HEADER );
        }
        else if ( psHTTPClnt->ulBufPos != 0 )
        {
          psHTTPClnt->ulTransmitPos = 0;
          psHTTPClnt->ulBufPos = 0;
          goto l01;
        }
        else
          return HTTPCLNT_RES_OK;
      }
    }
  }

  if ( psHTTPClnt->ulState == HTTPCLNT_STATE_READ_HEADER )
  {
    if ( psHTTPClnt->ulBufPos < CLNT_CONNECTION_BUF_SIZE )
    {
      ulLength = CLNT_CONNECTION_BUF_SIZE - psHTTPClnt->ulBufPos;
      if ( !SysSockRead( psHTTPClnt->ulSocket,
              &psHTTPClnt->acBuf[psHTTPClnt->ulBufPos], ulLength, &ulCount) )
      {
        _CloseConnection( psHTTPClnt, HTTPCLNT_STATE_CONNECTION_CLOSED );
        return HTTPCLNT_RES_ERROR;
      }
      psHTTPClnt->ulBufPos += ulCount;
    }

    while( TRUE )
    {
      pcEOH = memchr( &psHTTPClnt->acBuf[psHTTPClnt->ulTransmitPos], '\n',
                      psHTTPClnt->ulBufPos - psHTTPClnt->ulTransmitPos );

      if ( pcEOH == NULL )
      {
        if ( psHTTPClnt->ulBufPos == CLNT_CONNECTION_BUF_SIZE )
        {
          if ( psHTTPClnt->ulTransmitPos == 0 )
          {
            _CloseConnection( psHTTPClnt, HTTPCLNT_STATE_INVALID_RESPONSE );
            return HTTPCLNT_RES_ERROR;
          }
          else
          {
            ulLength = CLNT_CONNECTION_BUF_SIZE - psHTTPClnt->ulTransmitPos;
            memcpy( &psHTTPClnt->acBuf,
                    &psHTTPClnt->acBuf[psHTTPClnt->ulTransmitPos], ulLength );
            psHTTPClnt->ulTransmitPos = 0;
            psHTTPClnt->ulBufPos = ulLength;
          }
        }

        return HTTPCLNT_RES_OK;
      }

      ulLength = pcEOH - &psHTTPClnt->acBuf[psHTTPClnt->ulTransmitPos];
      if ( (ulLength > 0) && (*(pcEOH-1) == '\r') )
      {
        ulLength--;
      }

      if ( ulLength == 0 )
      {
        if ( (psHTTPClnt->ulFlags & CLNT_FLAG_STATUS_LINE) == 0 )
        {
          _CloseConnection( psHTTPClnt, HTTPCLNT_STATE_INVALID_RESPONSE );
          return HTTPCLNT_RES_ERROR;
        }

        pcPtr = ReqFldGet( &psHTTPClnt->sFields, "Content-Length" );
        if ( pcPtr != NULL )
        {
          psHTTPClnt->ullLength = atoll( pcPtr );
          psHTTPClnt->ulFlags |= CLNT_FLAG_RESP_BODY_LENGTH;
        }

        pcPtr = ReqFldGet( &psHTTPClnt->sFields, "Transfer-Encoding" );
        if ( pcPtr != NULL )
        {
          if ( stricmp( pcPtr, "chunked" ) == 0 )
            psHTTPClnt->ulFlags |= (CLNT_FLAG_RESP_TRANSFER_ENC | CLNT_FLAG_RESP_CHUNKED);
          else
            psHTTPClnt->ulFlags |= CLNT_FLAG_RESP_TRANSFER_ENC;
        }

        if ( !psHTTPClnt->cbHeader( psHTTPClnt->pObject,
                                    psHTTPClnt->ulRespCode,
                                    &psHTTPClnt->sFields ) )
        {
          _CloseConnection( psHTTPClnt, HTTPCLNT_STATE_DONE );
          return HTTPCLNT_RES_OK;
        }

        psHTTPClnt->ulChunkBytes = 0;
        HTTPCLNT_SET_STATE( HTTPCLNT_STATE_READ_BODY );

        psHTTPClnt->ulTransmitPos = (pcEOH - &psHTTPClnt->acBuf) + 1;
        if ( psHTTPClnt->ulTransmitPos == psHTTPClnt->ulBufPos ) 
        {
          psHTTPClnt->ulTransmitPos = 0;
          psHTTPClnt->ulBufPos = 0;
          return HTTPCLNT_RES_OK;
        }
        break;
      }

      pcPtr = &psHTTPClnt->acBuf[psHTTPClnt->ulTransmitPos];

      if ( (psHTTPClnt->ulFlags & CLNT_FLAG_STATUS_LINE) == 0 )
      {
        pcPtr = memchr( pcPtr, ' ', ulLength );
        if ( pcPtr == NULL )
        {
          _CloseConnection( psHTTPClnt, HTTPCLNT_STATE_INVALID_RESPONSE );
          return HTTPCLNT_RES_ERROR;
        }

        psHTTPClnt->ulRespCode = atol( pcPtr+1 ); 
        if ( psHTTPClnt->ulRespCode < 100 || psHTTPClnt->ulRespCode > 500 )
        {
          _CloseConnection( psHTTPClnt, HTTPCLNT_STATE_INVALID_RESPONSE );
          return HTTPCLNT_RES_ERROR;
        }

        psHTTPClnt->ulFlags |= CLNT_FLAG_STATUS_LINE;
      }
      else
      {
        if ( !ReqFldSetString( &psHTTPClnt->sFields, pcPtr, ulLength ) )
        {
          _CloseConnection( psHTTPClnt, HTTPCLNT_STATE_INVALID_RESPONSE );
          return HTTPCLNT_RES_ERROR;
        }
      }

      psHTTPClnt->ulTransmitPos = (pcEOH - &psHTTPClnt->acBuf) + 1;
      if ( psHTTPClnt->ulTransmitPos == psHTTPClnt->ulBufPos ) 
      {
        psHTTPClnt->ulTransmitPos = 0;
        psHTTPClnt->ulBufPos = 0;
        return HTTPCLNT_RES_OK;
      }
    }
  }

  if ( psHTTPClnt->ulState == HTTPCLNT_STATE_READ_BODY )
  {
l02:
    if ( ( (psHTTPClnt->ulFlags & CLNT_FLAG_EOF) == 0 )
         && ( psHTTPClnt->ulBufPos < CLNT_CONNECTION_BUF_SIZE ) )
    {
      ulLength = CLNT_CONNECTION_BUF_SIZE - psHTTPClnt->ulBufPos;
      if ( !SysSockRead( psHTTPClnt->ulSocket,
              &psHTTPClnt->acBuf[psHTTPClnt->ulBufPos], ulLength, &ulCount) )
      {
        psHTTPClnt->ulFlags |= CLNT_FLAG_EOF;
      }
      else
        psHTTPClnt->ulBufPos += ulCount;
    }

    ulLength = psHTTPClnt->ulBufPos - psHTTPClnt->ulTransmitPos;
    if ( ulLength > 0 )
    {
      if ( ( psHTTPClnt->ulFlags &
             (CLNT_FLAG_RESP_BODY_LENGTH | CLNT_FLAG_RESP_TRANSFER_ENC) )
           == CLNT_FLAG_RESP_BODY_LENGTH )
      {
        if ( ulLength > psHTTPClnt->ullLength )
          ulLength = psHTTPClnt->ullLength;
      }

      if ( (psHTTPClnt->ulFlags & CLNT_FLAG_RESP_CHUNKED) != 0 )
      {
        if ( !_ChunkedRead( psHTTPClnt, &ulCount ) )
        {
          _CloseConnection( psHTTPClnt, HTTPCLNT_STATE_INVALID_RESPONSE );
          return HTTPCLNT_RES_ERROR;
        }
      }
      else
      {
        if ( !psHTTPClnt->cbWrite( psHTTPClnt->pObject,
             &psHTTPClnt->acBuf[psHTTPClnt->ulTransmitPos], ulLength, &ulCount ) )
        {
          HTTPCLNT_SET_STATE( HTTPCLNT_STATE_DONE );
          return HTTPCLNT_RES_OK;
        }
        psHTTPClnt->ulTransmitPos += ulCount;
      }

      if ( (psHTTPClnt->ulFlags & CLNT_FLAG_RESP_BODY_LENGTH) != 0 )
      {
        if ( ulCount >= psHTTPClnt->ullLength )
        {
          psHTTPClnt->ullLength = 0;
          if ( (psHTTPClnt->ulFlags & CLNT_FLAG_RESP_TRANSFER_ENC) == 0 )
          {
            HTTPCLNT_SET_STATE( HTTPCLNT_STATE_DONE );
            return HTTPCLNT_RES_OK;
          }
        }
        else
          psHTTPClnt->ullLength -= ulCount;
      }
    }

    if ( psHTTPClnt->ulTransmitPos == psHTTPClnt->ulBufPos )
    {
      if ( (psHTTPClnt->ulFlags & CLNT_FLAG_EOF) != 0 )
      {
        if ( !psHTTPClnt->cbWrite( psHTTPClnt->pObject, NULL, 0, &ulCount ) )
        {
          SysSockClose( psHTTPClnt->ulSocket );
          psHTTPClnt->ulSocket = (ULONG)(-1);
        }
        HTTPCLNT_SET_STATE( HTTPCLNT_STATE_DONE );
      }
      else if ( psHTTPClnt->ulBufPos != 0 )
      {
        psHTTPClnt->ulTransmitPos = 0;
        psHTTPClnt->ulBufPos = 0;
        goto l02;
      }
    }
  }

  return HTTPCLNT_RES_OK;
}
