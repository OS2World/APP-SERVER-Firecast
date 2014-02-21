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
#include "httpserv\netserv.h"
#include "httpserv\netresolv.h"
#include "httpserv\netmessages.h"

#define DEFAUL_LISTEN_QUEUE_LEN	28

#define NET_MODULE	NET_MODULE_SERVER

static BOOL _AcceptSocketDefCB(ULONG ulSocket);
static ACCEPTSOCKETCALLBACK *cbAcceptSocket = _AcceptSocketDefCB;

VOID CBResolv(PVOID pObject, BOOL bFound, PSZ pszAddr, struct in_addr *psInAddr);

static PBIND _BindListNewItem(PBINDLIST psBindList)
{
  PBIND		psBind;

  if ( psBindList->ulBindCount == 0 ||
       (psBindList->ulBindCount & 0x07) == 0x07 )
  {
    psBind = realloc( psBindList->pasBind,
               (((psBindList->ulBindCount+1) & 0xFFFFFFF8) + 0x08) * sizeof(BIND) );
    if ( psBind == NULL )
      return NULL;
    psBindList->pasBind = psBind;
  }

  psBind = &psBindList->pasBind[ psBindList->ulBindCount++ ];

  return psBind;
}

static VOID _BindListDestroyItem(PBIND psBind)
{
  if ( psBind->bReady )
  {
    DBGINFO( NET_MESSAGE_REMOVEBIND, psBind, NULL, NULL );
    SysSockClose( psBind->ulSocket );
  }
  if ( psBind->pszAddr != NULL )
    free( psBind->pszAddr );
}

static VOID _BindListDestroy( PBINDLIST psBindList )
{
  ULONG		ulIdx;

  for( ulIdx = 0; ulIdx < psBindList->ulBindCount; ulIdx++ )
    _BindListDestroyItem( &psBindList->pasBind[ ulIdx ] );

  free( psBindList->pasBind );
}

static VOID _MakeServSock(PNETSERV psNetServ, PBIND psBind)
{
  ULONG			ulRC;
  struct in_addr	sInAddr;

  if ( psBind->bReady )
    return;

//  psInAddr = psBind->sInAddr;
  sInAddr.s_addr = INADDR_ANY;

  ulRC = SysCreateServSock( &psBind->ulSocket, &sInAddr, psBind->usPort,
                            psNetServ->ulListenQueueLen );
  if ( ulRC == RET_OK )
  {
    psBind->bReady = TRUE;
    DBGINFO( NET_MESSAGE_SETBIND, psNetServ, psBind, NULL );
  }
  else
  {
    ERROR( ulRC == RET_ERROR_BIND_SOCKET ?
           NET_MESSAGE_CANTBINDSOCK : NET_MESSAGE_CANTCREATESRVSOCK,
           psNetServ, psBind, NULL );
  }
}

static BOOL _AcceptSocketDefCB(ULONG ulSocket)
{
  return TRUE;
}


VOID NetServInit(PNETSERV psNetServ, ULONG ulListenQueueLen)
{
  ULONG		ulRC;

  memset( psNetServ, '\0', sizeof(NETSERV) );

  SysMutexCreate( &psNetServ->hmtxBindList );

  psNetServ->ulListenQueueLen = ulListenQueueLen == 0 ?
    DEFAUL_LISTEN_QUEUE_LEN : ulListenQueueLen;
}

VOID NetServDone(PNETSERV psNetServ)
{
  NetRemoveObject( psNetServ );
  _BindListDestroy( &psNetServ->sBindList );
  SysMutexDestroy( &psNetServ->hmtxBindList );
}

ULONG NetServBindListAdd(PBINDLIST psBindList, PSZ pszAddr, USHORT usPort)
{
  PBIND		psBind;

  if ( psBindList == NULL || usPort == 0 )
    return RET_INVALID_PARAM;

  psBind = _BindListNewItem( psBindList );
  if ( psBind == NULL )
  {
    ERROR_NOT_ENOUGHT_MEM();
    return RET_NOT_ENOUGH_MEMORY;
  }

  psBind->ulSocket	= (ULONG)(-1);
  psBind->usPort	= usPort;
  psBind->pszAddr	= pszAddr == NULL ? NULL : strdup( pszAddr );
  psBind->bReady	= FALSE;

  return RET_OK;
}

VOID NetServBindListApply(PNETSERV psNetServ, PBINDLIST psBindList)
{
  PBIND		psBind, psScanBind, psNewBind;
  ULONG		ulIdx, ulScanIdx;
  BINDLIST	sNewBindList = { 0 };
  BOOL		bFound;

  SysMutexLock( &psNetServ->hmtxBindList );

  for( ulIdx = 0; ulIdx < psBindList->ulBindCount; ulIdx++ )
  {
    psNewBind = _BindListNewItem( &sNewBindList );
    if ( psNewBind == NULL )
    {
      _BindListDestroy( &sNewBindList );
      ERROR_NOT_ENOUGHT_MEM();
      break;
    }

    psBind = &psBindList->pasBind[ ulIdx ];

    bFound = FALSE;
    for( ulScanIdx = 0; ulScanIdx < psNetServ->sBindList.ulBindCount; ulScanIdx++ )
    {
      psScanBind = &psNetServ->sBindList.pasBind[ ulScanIdx ];
      if ( ( stricmp(psScanBind->pszAddr == NULL ? "" : psScanBind->pszAddr,
                     psBind->pszAddr == NULL ? "" : psBind->pszAddr) == 0 )
         && ( psScanBind->usPort == psBind->usPort ) )
      {
        bFound = TRUE;
        break;
      }
    }

    if ( bFound )
    {
      *psNewBind = *psScanBind;
      psScanBind->ulSocket	= (ULONG)(-1);
      psScanBind->pszAddr 	= NULL;
      psScanBind->bReady	= FALSE;
    }
    else
    {
      psNewBind->ulSocket	= (ULONG)(-1);
      psNewBind->usPort		= psBind->usPort;
      psNewBind->pszAddr	= psBind->pszAddr == NULL ? NULL : strdup( psBind->pszAddr );
      psNewBind->bReady		= FALSE;

      if ( NetResolv( psNetServ, psNewBind->pszAddr, CBResolv,
                      &psNewBind->sInAddr ) )
      {
        _MakeServSock( psNetServ, psNewBind );
      }
    }
  }

  _BindListDestroy( &psNetServ->sBindList );
  psNetServ->sBindList = sNewBindList;
  _BindListDestroy( psBindList );

  SysMutexUnlock( &psNetServ->hmtxBindList );
}

VOID BindListDestroy(PBINDLIST psBindList)
{
  _BindListDestroy( psBindList );
}

VOID CBResolv(PVOID pObject, BOOL bFound, PSZ pszAddr, struct in_addr *psInAddr)
{
  ULONG		ulRC;
  ULONG		ulIdx;
  PBIND		psBind;
  PNETSERV	psNetServ = pObject;

  if ( !bFound )
  {
    ERROR( NET_MESSAGE_CANTRESOLV, psNetServ, pszAddr, NULL );
    return;
  }

  SysMutexLock( &psNetServ->hmtxBindList );

  for( ulIdx = 0; ulIdx < psNetServ->sBindList.ulBindCount; ulIdx++ )
  {
    psBind = &psNetServ->sBindList.pasBind[ulIdx];
    if ( psBind->pszAddr != NULL && !psBind->bReady &&
         strcmp(pszAddr, psBind->pszAddr) == 0 )
    {
      memcpy( &psBind->sInAddr, psInAddr, sizeof(struct in_addr) );
      _MakeServSock( psNetServ, psBind );
    }
  }

  SysMutexUnlock( &psNetServ->hmtxBindList );
}

VOID NetServGetClientSockets(PNETSERV psNetServ, PULONG apulList,
                             ULONG ulMaxCount, ULONG ulTimeout,
                             PULONG pulActualCount)
{
  ULONG			ulIdx;
  fd_set		sFDSet;
  ULONG			ulSocket;
  PBIND			psBind;
  struct in_addr	sInAddr;
  ULONG			ulCount;

  if ( ulMaxCount == 0 )
  {
    SysSleep( ulTimeout );
    *pulActualCount = 0;
    return;
  }

  SysMutexLock( &psNetServ->hmtxBindList );
  FD_ZERO( &sFDSet );
  for( ulIdx = 0; ulIdx < psNetServ->sBindList.ulBindCount; ulIdx++ )
  {
    psBind = &psNetServ->sBindList.pasBind[ulIdx];
    if ( psBind->bReady )
      FD_SET( psBind->ulSocket, &sFDSet );
  }
  SysMutexUnlock( &psNetServ->hmtxBindList );

  if ( !SysSelect( &sFDSet, NULL, NULL, ulTimeout ) )
  {
    *pulActualCount = 0;
    return;
  }

  ulCount = 0;
  SysMutexLock( &psNetServ->hmtxBindList );
  for( ulIdx = 0; ulIdx < psNetServ->sBindList.ulBindCount; ulIdx++ )
  {
    psBind = &psNetServ->sBindList.pasBind[ulIdx];
    ulSocket = psBind->ulSocket;

    if ( psBind->bReady && FD_ISSET( ulSocket, &sFDSet ) )
    {
      sInAddr  = psBind->sInAddr;

      if ( SysSockAccept( &ulSocket, &sInAddr ) )
      {
        DBGINFO( NET_MESSAGE_CONNECTION, &ulSocket, psNetServ, NULL );
        if ( cbAcceptSocket( ulSocket ) )
        {
          apulList[ ulCount ] = ulSocket;
          ulCount++;
          if ( ulCount == ulMaxCount )
            break;
        }
      }
    }
  }
  SysMutexUnlock( &psNetServ->hmtxBindList );

  *pulActualCount = ulCount;
}

VOID NetServSetAcceptSocketCB(ACCEPTSOCKETCALLBACK cbNewAcceptSocket)
{
  cbAcceptSocket = cbNewAcceptSocket == NULL ?
                   _AcceptSocketDefCB : cbNewAcceptSocket;
}

ULONG NetServQueryBindList(PNETSERV psNetServ)
{
  SysMutexLock( &psNetServ->hmtxBindList );
  return psNetServ->sBindList.ulBindCount;
}

VOID NetServReleaseBindList(PNETSERV psNetServ)
{
  SysMutexUnlock( &psNetServ->hmtxBindList );
}

BOOL NetServGetBind(PNETSERV psNetServ, ULONG ulIdx, PSZ *ppszAddr,
                    PUSHORT pusPort)
{
  PBIND		psBind;

  if ( ulIdx >= psNetServ->sBindList.ulBindCount )
    return FALSE;

  psBind = &psNetServ->sBindList.pasBind[ulIdx];
  *ppszAddr = psBind->pszAddr;
  *pusPort = psBind->usPort;
  return TRUE;
}
