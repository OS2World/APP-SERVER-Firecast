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
#include <stdlib.h>
#include <stdio.h>
#include <time.h> 
#include "logmsg.h"
#include "ssmessages.h"
#include "httpserv/netserv.h"
#include "httpserv/requests.h"


static HMTX	hmtxMessage;
static PCHAR	pcLogBuf	= NULL;
static ULONG	ulBufPos	= 0;
static ULONG	ulBufSize	= 0;


static VOID _NetMessageCB(ULONG ulModule, ULONG ulType, ULONG ulCode,
                          PVOID pParam1, PVOID pParam2, PVOID pParam3)
{
  PCHAR			pcModule;
  PCHAR			pcType;
  ULONG			ulPtr;
  PCHAR			pcPtr;
  CHAR			acBuf[256];
  CHAR			acAddr[32];
  CHAR			acAddr2[32];
  struct in_addr	sInAddr;
  time_t		sTime; 
  ULONG			ulSpace;

  if ( ulBufSize == 0 )
    return;

  switch ( ulModule )
  {
    case NET_MODULE_SERVER:
      pcModule = "SERVER";
      break;
    case NET_MODULE_REQUESTS:
      pcModule = "REQUESTS";
      break;
    case NET_MODULE_HTTPSERV:
      pcModule = "HTTPSERV";
      break;
    case NET_MODULE_RESOLV:
      pcModule = "RESOLV";
      break;
    case NET_MODULE_HTTPCLNT:
      pcModule = "HTTPCLNT";
      break;
    case SS_MODULE_TEMPLATES:
      pcModule = "TEMPLATES";
      break;
    case SS_MODULE_SOURCES:
      pcModule = "SOURCES";
      break;
    case SS_MODULE_CONTROL:
      pcModule = "CONTROL";
      break;
    case SS_MODULE_FILERES:
      pcModule = "FILERES";
      break;
    case SS_MODULE_RELAYS:
      pcModule = "RELAYS";
      break;
    default:
      pcModule = "UNKNOWN";
      break;
  }

  switch ( ulType )
  {
    case NET_MSGTYPE_ERROR:
      pcType = "ERROR";
      break;
    case NET_MSGTYPE_WARNING:
      pcType = "WARNING";
      break;
    case NET_MSGTYPE_INFO:
      pcType = "INFO";
      break;
    case NET_MSGTYPE_DETAIL:
      pcType = "DETAIL";
      break;
    case NET_MSGTYPE_DEBUG:
      pcType = "DEBUG";
      break;
    default:
      pcType = "UNKNOWN";
      break;
  }

  sTime = time( NULL );
  ulPtr = strftime( &acBuf, 64, "%F %T ", localtime( &sTime ) );
  ulPtr += sprintf( &acBuf[ulPtr], "%s [%s] ", pcType, pcModule );
  pcPtr = &acBuf[ulPtr];
  ulSpace = (sizeof( acBuf ) - ulPtr) - 3;

  switch ( ulCode )
  {
    case NET_MESSAGE_NOTENOUGHTMEM:
      strcpy( pcPtr, "not enought memory" );
      break;
    case NET_MESSAGE_CANTRESOLV:
      snprintf( pcPtr, ulSpace, "unknown host %s", pParam2 );
      break;
    case NET_MESSAGE_CANTBINDSOCK:
      snprintf( pcPtr, ulSpace, "cannot bind the server socket to %s:%u",
               ((PBIND)(pParam2))->pszAddr, ((PBIND)(pParam2))->usPort );
      break;
    case NET_MESSAGE_CANTCREATESRVSOCK:
      snprintf( pcPtr, ulSpace, "cannot create the server socket for %s:%u",
               ((PBIND)(pParam2))->pszAddr, ((PBIND)(pParam2))->usPort );
      break;
    case NET_MESSAGE_SETBIND:
      snprintf( pcPtr, ulSpace, "listening on %s:%u",
               ((PBIND)(pParam2))->pszAddr, ((PBIND)(pParam2))->usPort );
      break;
    case NET_MESSAGE_REMOVEBIND:
      snprintf( pcPtr, ulSpace, "remove binding to on %s:%u",
               ((PBIND)(pParam1))->pszAddr, ((PBIND)(pParam1))->usPort );
      break;
    case NET_MESSAGE_CONNECTION:
      SysSockPeerAddr( (PULONG)(pParam1), &sInAddr );
      SysStrAddr( acAddr, 32, &sInAddr );
      snprintf( pcPtr, ulSpace, "connection from %s", acAddr );
      break;
    case NET_MESSAGE_CONNECTIONCLOSE:
      if ( SysSockAddr( (PULONG)(pParam1), &sInAddr ) )
      {
        SysStrAddr( acAddr, 32, &sInAddr );
        SysSockPeerAddr( (PULONG)(pParam1), &sInAddr );
        SysStrAddr( acAddr2, 32, &sInAddr );
        snprintf( pcPtr, ulSpace, "close connection on %s:%u (from %s)", acAddr,
                 SysSockPort(*(PULONG)(pParam1)), acAddr2 );
      }
      else
        strcpy( pcPtr, "close connection" );
      break;
    case NET_MESSAGE_REQUEST:
      SysStrAddr( acAddr, 32, &((PCONNINFO)pParam1)->sClientAddr );
      SysStrAddr( acAddr2, 32, &((PCONNINFO)pParam1)->sServAddr );
      snprintf( pcPtr, ulSpace, "request (%s to %s:%u) %s %s", acAddr, acAddr2,
               ((PCONNINFO)pParam1)->usServPort, (PCHAR)pParam2,
               (PCHAR)pParam3 );
      break;
    case NET_MESSAGE_REQUESTRESULT:
      snprintf( pcPtr, ulSpace, "%s %s -- %s", (PCHAR)pParam1, (PCHAR)pParam2,
               (PCHAR)pParam3 );
      break;
    case NET_MESSAGE_UNKNOWNHOST:
      SysStrAddr( acAddr, 32, &((PCONNINFO)pParam1)->sClientAddr );
      SysStrAddr( acAddr2, 32, &((PCONNINFO)pParam1)->sServAddr );
      snprintf( pcPtr, ulSpace, "%s to %s:%u -- unknown host [%s]", acAddr, acAddr2,
               &((PCONNINFO)pParam1)->usServPort, 
               pParam2 == NULL ? "" : (PCHAR)pParam2 );
    case NET_MESSAGE_HOSTACCESSDENIED:
      SysStrAddr( acAddr, 32, &((PCONNINFO)pParam1)->sClientAddr );
      SysStrAddr( acAddr2, 32, &((PCONNINFO)pParam1)->sServAddr );
      snprintf( pcPtr, ulSpace, "%s to %s:%u -- access denied [%s]", acAddr, acAddr2,
               &((PCONNINFO)pParam1)->usServPort, 
               pParam2 == NULL ? "" : (PCHAR)pParam2 );
      break;
    case NET_MESSAGE_CANTOPENFILE:
      snprintf( pcPtr, ulSpace, "cannot open file %s", (PCHAR)pParam1 );
      break;
    case SS_MESSAGE_TEMPLDENIED:
      if ( pParam1 == NULL )
        snprintf( pcPtr, ulSpace, "template denied [%s]",
                  pParam2 == NULL ? "" : (PCHAR)pParam2 );
      else
      {
        SysStrAddr( acAddr, 32, &((PCONNINFO)pParam1)->sClientAddr );
        SysStrAddr( acAddr2, 32, &((PCONNINFO)pParam1)->sServAddr );
        snprintf( pcPtr, ulSpace, "%s to %s:%u -- access denied [%s]", acAddr, acAddr2,
                 &((PCONNINFO)pParam1)->usServPort, 
                 pParam2 == NULL ? "" : (PCHAR)pParam2 );
      }
      break;
    case SS_MESSAGE_TEMPLINVALIDPSWD:
      SysStrAddr( acAddr, 32, &((PCONNINFO)pParam1)->sClientAddr );
      SysStrAddr( acAddr2, 32, &((PCONNINFO)pParam1)->sServAddr );
      snprintf( pcPtr, ulSpace, "%s to %s:%u -- invalid password [%s]", acAddr, acAddr2,
               &((PCONNINFO)pParam1)->usServPort, 
               pParam2 == NULL ? "" : (PCHAR)pParam2 );
      break;
    case SS_MESSAGE_MPOINTEXIST:
      if ( pParam1 == NULL )
        snprintf( pcPtr, ulSpace, "mount point exists [%s]",
                  pParam2 == NULL ? "" : (PCHAR)pParam2 );
      else
      {
        SysStrAddr( acAddr, 32, &((PCONNINFO)pParam1)->sClientAddr );
        SysStrAddr( acAddr2, 32, &((PCONNINFO)pParam1)->sServAddr );
        snprintf( pcPtr, ulSpace, "%s to %s:%u -- mount point exists [%s]", acAddr, acAddr2,
                 &((PCONNINFO)pParam1)->usServPort, 
                 pParam2 == NULL ? "" : (PCHAR)pParam2 );
      }
      break;
    case SS_MESSAGE_CLIENTACCESSDENIED:
      SysStrAddr( acAddr, 32, &((PCONNINFO)pParam1)->sClientAddr );
      SysStrAddr( acAddr2, 32, &((PCONNINFO)pParam1)->sServAddr );
      snprintf( pcPtr, ulSpace, "%s to %s:%u -- access denied [%s]", acAddr, acAddr2,
               &((PCONNINFO)pParam1)->usServPort, 
               pParam2 == NULL ? "" : (PCHAR)pParam2 );
      break;
    case SS_MESSAGE_TOOMANYCLIENTS:
      SysStrAddr( acAddr, 32, &((PCONNINFO)pParam1)->sClientAddr );
      SysStrAddr( acAddr2, 32, &((PCONNINFO)pParam1)->sServAddr );
      snprintf( pcPtr, ulSpace, "%s to %s:%u -- too many clients (limit: %u) [%s]",
               acAddr, acAddr2, &((PCONNINFO)pParam1)->usServPort, 
               *(PULONG)pParam3, pParam2 == NULL ? "" : (PCHAR)pParam2 );
      break;
    case SS_MESSAGE_NEWSOURCE:
      if ( pParam1 == NULL )
        snprintf( pcPtr, ulSpace, "new source [%s]", 
                  pParam2 == NULL ? "" : (PCHAR)pParam2 );
      else
      {
        SysStrAddr( acAddr, 32, &((PCONNINFO)pParam1)->sClientAddr );
        SysStrAddr( acAddr2, 32, &((PCONNINFO)pParam1)->sServAddr );
        snprintf( pcPtr, ulSpace, "%s to %s:%u -- new source [%s]", acAddr, acAddr2,
                  ((PCONNINFO)pParam1)->usServPort, 
                  pParam2 == NULL ? "" : (PCHAR)pParam2 );
      }
      break;
    case SS_MESSAGE_DESTROYSOURCE:
      snprintf( pcPtr, ulSpace, "destroy source [%s]", (PCHAR)pParam1 );
      break;
    case SS_MESSAGE_CANTLOADCFG:
      snprintf( pcPtr, ulSpace, "cannot load file \"%s\". A default configuration will be used.",
              (PCHAR)pParam1 );
      break;
    case SS_MESSAGE_CANTMAKEDEFCFG:
      strcpy( pcPtr, "cannot create a default configuration" );
      break;
    case SS_MESSAGE_CANTSAVECFG:
      snprintf( pcPtr, ulSpace, "cannot write a configuration file \"%s\"",
               (PCHAR)pParam1 );
      break;
    case SS_MESSAGE_INVALIDMETAREQUEST:
      SysStrAddr( acAddr, 32, &((PCONNINFO)pParam1)->sClientAddr );
      SysStrAddr( acAddr2, 32, &((PCONNINFO)pParam1)->sServAddr );
      snprintf( pcPtr, ulSpace, "%s to %s:%u -- invalid metadata-request [%s]", acAddr, acAddr2,
               &((PCONNINFO)pParam1)->usServPort, (PCHAR)pParam2 );
      break;
    case SS_MESSAGE_UNAUTHORIZED:
      SysStrAddr( acAddr, 32, &((PCONNINFO)pParam1)->sClientAddr );
      snprintf( pcPtr, ulSpace, "%s -- unauthorized request [%s]", acAddr,
                (PCHAR)pParam2 );
      break;
    case SS_MESSAGE_SOURCENOTFOUND:
      SysStrAddr( acAddr, 32, &((PCONNINFO)pParam1)->sClientAddr );
      snprintf( pcPtr, ulSpace, "%s -- source not found [%s]", acAddr,
               pParam2 == NULL ? "" : (PCHAR)pParam2 );
      break;
    case SS_MESSAGE_SETMETADATA:
      SysStrAddr( acAddr, 32, &((PCONNINFO)pParam1)->sClientAddr );
      snprintf( pcPtr, ulSpace, "%s -- metadata%s updated [%s]", acAddr,
               (BOOL)pParam3 ? "" : " was not",
               pParam2 == NULL ? "" : (PCHAR)pParam2 );
      break;
    case SS_MESSAGE_INVALIDDATAFORMAT:
      snprintf( pcPtr, ulSpace, "invalid a stream's format for source [%s]", (PCHAR)pParam1 );
      break;
    case SS_MESSAGE_RELAY_STATE:
      snprintf( pcPtr, ulSpace, "relay [%s] (source %s), state: %s",
                (PCHAR)pParam2, (PCHAR)pParam1, (PCHAR)pParam3 );
      break;
    case SS_MESSAGE_RELAY_HTTP_STATUS_CODE:
      snprintf( pcPtr, ulSpace, "relay [%s] (source %s), HTTP status code received: %u",
                (PCHAR)pParam2, (PCHAR)pParam1, (ULONG)pParam3 );
      break;
    case SS_MESSAGE_RELAY_HTTP_CONTENT_TYPE:
      snprintf( pcPtr, ulSpace, "relay [%s] (source %s), HTTP content type: %s",
                (PCHAR)pParam2, (PCHAR)pParam1, (PCHAR)pParam3 );
      break;
    default:
      strcpy( pcPtr, "unknown message" );
      break;
  }

  acBuf[sizeof(acBuf)-3] = '\0';

  pcPtr = strchr( pcPtr, '\0' );
  strcpy( pcPtr, "\r\n" );

  {
    ULONG	ulLength = strlen( &acBuf );

    SysMutexLock( &hmtxMessage );
    if ( ulBufPos > ulLength )
    {
      ulBufPos -= ulLength;
    }
    else
    {
      ulLength -= ulBufPos;
      memcpy( pcLogBuf, &acBuf[ulLength], ulBufPos );
      ulBufPos = ulBufSize - ulLength;
    }
    memcpy( &pcLogBuf[ulBufPos], &acBuf, ulLength );

    SysMutexUnlock( &hmtxMessage );
  }
}


VOID LogInit()
{
  SysMutexCreate( &hmtxMessage );
  LogSetLevel( NET_MSGTYPE_DEBUG );
}

VOID LogDone()
{
  NetMessageCBSet( NULL, 0 );
  SysMutexDestroy( &hmtxMessage );
}

VOID LogSetLevel(ULONG ulLevel)
{
  if ( ulLevel <= NET_MSGTYPE_DEBUG )
    NetMessageCBSet( _NetMessageCB, ulLevel );
}

ULONG LogGetLevel()
{
  return NetMessageGetLevel();
}

BOOL LogSetBuff(ULONG ulSize)
{
  PCHAR		pcNewBuf;

  if ( ulSize < LOG_MIN_BUFFER_SIZE )
    ulSize = LOG_MIN_BUFFER_SIZE;
  else if ( ulSize > LOG_MAX_BUFFER_SIZE )
    ulSize = LOG_MAX_BUFFER_SIZE;

  if ( (pcNewBuf = malloc( ulSize )) == NULL )
    return FALSE;
  pcNewBuf[0] = '\0';

  SysMutexLock( &hmtxMessage );
  free( pcLogBuf );
  pcLogBuf = pcNewBuf;
  pcLogBuf[0] = '\0';
  ulBufPos = 0;
  ulBufSize = ulSize;
  SysMutexUnlock( &hmtxMessage );

  return TRUE;
}

ULONG LogGetBuffSize()
{
  return ulBufSize;
}

BOOL LogOpen(PLOG psLog)
{
  if ( ulBufSize == 0 )
    return FALSE;

  psLog->ulPos = ulBufPos;
  psLog->ulLength = ulBufSize;
  return TRUE;
}

VOID LogClose(PLOG psLog)
{
}

ULONG LogRead(PLOG psLog, PCHAR pcBuf, ULONG ulBufLen)
{
  ULONG		ulLength;

  if ( ulBufSize == 0 )
    return 0;

  if ( ulBufLen > psLog->ulLength )
    ulBufLen = psLog->ulLength;

  ulLength = ulBufSize - psLog->ulPos;
  if ( ulLength < ulBufLen )
  {
    memcpy( pcBuf, &pcLogBuf[ psLog->ulPos ], ulLength );
    psLog->ulPos = 0;
    ulBufLen -= ulLength;
    pcBuf += ulLength;
  }
  else
    ulLength = 0;

  if ( psLog->ulPos == 0 && pcLogBuf[psLog->ulPos] == '\0' )
    return ulLength;

  memcpy( pcBuf, &pcLogBuf[ psLog->ulPos ], ulBufLen );
  psLog->ulPos += ulBufLen;
  if ( psLog->ulPos == ulBufSize )
    psLog->ulPos = 0;
  psLog->ulLength -= (ulLength + ulBufLen);

  return ulLength + ulBufLen;
}
