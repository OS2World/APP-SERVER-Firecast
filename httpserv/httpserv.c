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
#include <time.h>
#include "httpserv\requests.h"
#include "httpserv\reqfields.h"
#include "httpserv\fileres.h"
#include "httpserv\filehost.h"
#include "httpserv\netmessages.h"

#define RESP_TEMPL \
  "<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">" \
  "<HTML><HEAD><TITLE>%ERRORCODE%</TITLE></HEAD>" \
  "<BODY><H1>%ERROR%</H1>"  \
  "The requested URL %URI% %WHAT%<P>" \
  "<HR><ADDRESS>%SERVER% at %HOSTNAME%</ADDRESS>" \
  "</BODY></HTML>"

#define RESP_TEMPL_WHAT_401	"authentication is required"
#define RESP_TEMPL_WHAT_404	"was not found on this server"
#define RESP_TEMPL_WHAT_405	"is not allowed for the requested method"
#define RESP_TEMPL_WHAT_500	"encountered an internal error"
#define RESP_TEMPL_WHAT_414	"too long"

#define NET_MODULE	NET_MODULE_HTTPSERV

static PFILERES _CreateMessageRes(PCHAR pcTemplate, PCHAR pcURI,
                                  RESPCODE rcRespCode, PCHAR pcHostName,
                                  PCHAR pcWhat)
{
  ULONG		ulActual;
  ULONG		ulRC;
  PFILERES	psFileRes;
  PCHAR		pcKey, pcKeyEnd;
  ULONG		ulKeyLen;

  ulRC = FileResMemOpen( &psFileRes, 1024, "text/html" );
  if ( ulRC != FILERES_OK )
    return NULL;

  while( 1 )
  {
    pcKey = strchr( pcTemplate, '%' );
    if ( pcKey == NULL )
      break;
    ulRC = FileResWrite( psFileRes, pcTemplate, pcKey - pcTemplate, &ulActual );

    pcKey += 1;
    pcKeyEnd = strchr( pcKey, '%' );
    ulKeyLen = pcKeyEnd - pcKey;
    pcTemplate = pcKeyEnd + 1;
    if ( ulKeyLen == 0 )
      pcKeyEnd = "%";
    else if ( memcmp( pcKey, "URI", 3 ) == 0 )
      pcKeyEnd = pcURI;
    else if ( memcmp( pcKey, "ERRORCODE", 9 ) == 0 )
      pcKeyEnd = rcRespCode;
    else if ( memcmp( pcKey, "ERROR", 5 ) == 0 )
      pcKeyEnd = &rcRespCode[4];
    else if ( memcmp( pcKey, "SERVER", 6 ) == 0 )
      pcKeyEnd = ReqGetServerName();
    else if ( memcmp( pcKey, "HOSTNAME", 8 ) == 0 )
      pcKeyEnd = pcHostName;
    else if ( memcmp( pcKey, "WHAT", 4 ) == 0 )
      pcKeyEnd = pcWhat;
    else
      continue;

    if ( pcKeyEnd != NULL )
      ulRC = FileResWrite( psFileRes, pcKeyEnd, strlen(pcKeyEnd), &ulActual );
  }

  ulRC = FileResWrite( psFileRes, pcTemplate, strlen(pcTemplate), &ulActual );
  FileResSeek( psFileRes, 0 );
  return psFileRes;
}


RESPCODE CBReqNew(PVOID *ppRequest, PCHAR pcProto, PCHAR pcMethod, PCHAR pcURI,
                  PREQFIELDS psFields, PCONNINFO psConnInfo)
{
  ULONG		ulRC;
  RESPCODE	rcRespCode;
  PCHAR		pcWhat;
  PFILEHOST	psFileHost;
#define MAX_LOC_PATH_LEN	(FILE_HOST_MAX_BASE_PATH_LEN + 64)
  CHAR		acBuf[MAX_LOC_PATH_LEN + 1];
  ULONG		ulBufSpace;
  PCHAR		pcURIParam;
  ULONG		ulLength;
  ULONG		ulIdx;
  PCHAR		pcFileHost;

  pcFileHost = ReqFldGet(psFields, "Host");
  psFileHost = FileHostQuery( pcFileHost );
  if ( psFileHost == NULL)
  {
    INFO( NET_MESSAGE_UNKNOWNHOST, psConnInfo, pcFileHost, NULL );
    *ppRequest = NULL;
    return NULL;
  }

  if ( !FileHostTestAcl( psFileHost, psConnInfo ) )
  {
    INFO( NET_MESSAGE_HOSTACCESSDENIED, psConnInfo, pcFileHost, NULL );
    *ppRequest = NULL;
    FileHostRelease( psFileHost );
    return RESP_CODE_403;
  }

  pcURIParam = strchr( pcURI, '?' );
  if ( pcURIParam != NULL )
  {
    *pcURIParam = '\0';
    pcURIParam++;
  }

  ulRC = FileResOpen( (PFILERES *)ppRequest, psFileHost, pcURI, pcURIParam,
                      pcMethod, psFields, psConnInfo );

  if ( ulRC == FILERES_IS_DIRECTORY && /*psFileHost != NULL &&*/
       psFileHost->ulIndexFilesCount != 0 )
  {
    ulLength = strlen( pcURI );
    memcpy( &acBuf, pcURI, ulLength );
    if ( acBuf[ulLength-1] == '/' )
      ulLength--;
    acBuf[ulLength++] = '\\';
    ulBufSpace = MAX_LOC_PATH_LEN - ulLength;
    acBuf[MAX_LOC_PATH_LEN] = '\0';

    for( ulIdx = 0; ulIdx < psFileHost->ulIndexFilesCount; ulIdx++ )
      if ( strlen( psFileHost->paszIndexFiles[ulIdx] ) <= ulBufSpace )
      {
        strncpy( &acBuf[ulLength], psFileHost->paszIndexFiles[ulIdx],
                 ulBufSpace );
        ulRC = FileResOpen( (PFILERES *)ppRequest, psFileHost, &acBuf,
                            pcURIParam, pcMethod, psFields, psConnInfo );
        if ( ulRC != FILERES_NOT_FOUND )
          break;
      }
  }

  FileHostRelease( psFileHost );

  if ( ulRC == FILERES_IS_DIRECTORY )
    ulRC = FILERES_NOT_FOUND;

  switch ( ulRC )
  {
    case FILERES_WAIT_DATA:
      return NULL;
    case FILERES_OK:
      return RESP_CODE_200;
    case FILERES_OK_CONTINUE:
      return RESP_CODE_100;
    case FILERES_PARTIAL_CONTENT:
      return RESP_CODE_206;
    case FILERES_OK_ICES:
      return RESP_CODE_200_ICES;

    case FILERES_FORBIDDEN:
      *ppRequest = NULL;
      return RESP_CODE_403;
    case FILERES_NOT_IMPLEMENTED:
      *ppRequest = NULL;
      return RESP_CODE_501;
    case FILERES_NOT_MODIFED:
      *ppRequest = NULL;
      return RESP_CODE_304;
    case FILERES_PRECONDITION_FAILED:
      *ppRequest = NULL;
      return RESP_CODE_412;

    case FILERES_NOT_FOUND:
      rcRespCode = RESP_CODE_404;
      pcWhat = RESP_TEMPL_WHAT_404;
      break;
    case FILERES_NOT_ALLOWED:
      rcRespCode = RESP_CODE_405;
      pcWhat = RESP_TEMPL_WHAT_405;
      break;
    case FILERES_PATH_TOO_LONG:
      rcRespCode = RESP_CODE_414;
      pcWhat = RESP_TEMPL_WHAT_414;
      break;
    case FILERES_UNAUTHORIZED:
      rcRespCode = RESP_CODE_401;
      pcWhat = RESP_TEMPL_WHAT_401;
      break;
    default:
      rcRespCode = RESP_CODE_500;
      pcWhat = RESP_TEMPL_WHAT_500;
  }

  *ppRequest = _CreateMessageRes( RESP_TEMPL, pcURI, rcRespCode,
                                  ReqFldGet(psFields,"Host"), pcWhat );

  return rcRespCode;
}

VOID CBReqDestroy(PVOID pRequest)
{
  if ( pRequest != NULL )
    FileResClose( (PFILERES)pRequest );
}

RESPCODE CBReqWrite(PVOID pRequest, PCHAR pcBuf, ULONG ulBufLen,
                    PULONG pulActual)
{
  ULONG		ulRC;

  ulRC = FileResWrite( (PFILERES)pRequest, pcBuf, ulBufLen, pulActual);

  switch ( ulRC )
  {
    case FILERES_WAIT_DATA:
      return NULL;
    case FILERES_OK:
      return RESP_CODE_200;
    default: // FILERES_INTERNAL_ERROR
      return RESP_CODE_500;
  }
}

VOID CBReqGetFields(PVOID pRequest, RESPCODE ulRespCode,
                    PREQFIELDS psRespFields)
{
  FileResGetFields( (PFILERES)pRequest, ulRespCode, psRespFields );
}

ULONG CBReqRead(PVOID pRequest, PCHAR pcBuf, ULONG ulBufLen, PULONG pulActual)
{
  ULONG		ulRC;

  if ( pRequest == NULL )
  {
    *pulActual = 0;
    return REQ_READ_CB_EOF;
  }

  ulRC = FileResRead( (PFILERES)pRequest, pcBuf, ulBufLen, pulActual);

  switch ( ulRC )
  {
    case FILERES_OK:
      return REQ_READ_CB_OK;
    case FILERES_NOT_ALLOWED:
    case FILERES_INTERNAL_ERROR:
      return REQ_READ_CB_ERROR;
    default: // FILERES_NO_MORE_DATA:
      return REQ_READ_CB_EOF;
  }
}

VOID HTTPServInit(PSZ pszMIMETypesFileName, ULONG ulThreads, PSZ pszServerName)
{
  REQCB		sReqCB;

#if defined(__WATCOMC__) && defined(__OS2__)
  // bug in watcom's OS/2 run-time
  CHAR		acTZ[48];

  strncpy( &acTZ[3], getenv("TZ"), 47 );
  acTZ[47] = '\0';
  *(PULONG)(&acTZ) = 'A=ZT';
  *(PUSHORT)(&acTZ[4]) = 'AA';
  putenv( &acTZ );
  tzset();
#endif

  sReqCB.cbNew		= CBReqNew;
  sReqCB.cbDestroy	= CBReqDestroy;
  sReqCB.cbWrite	= CBReqWrite;
  sReqCB.cbGetFields	= CBReqGetFields;
  sReqCB.cbRead		= CBReqRead;

  FileResInit( pszMIMETypesFileName );
  FileHostInit();
  ReqInit( &sReqCB, ulThreads, pszServerName );
}

VOID HTTPServDone()
{
  ReqDone();
  FileHostDone();
  FileResDone();
}
