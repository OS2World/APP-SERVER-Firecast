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
#include <process.h> 
#include "httpserv\netresolv.h"
#include "httpserv\netmessages.h"

#define NET_MODULE	NET_MODULE_RESOLV

typedef struct _RESOLVITEM {
  struct _RESOLVITEM	*psNext;
  PVOID			pObject;
  PSZ			pszAddr;
  PRESOLVCALLBACK	cbResolv;
} RESOLVITEM, *PRESOLVITEM;

static PRESOLVITEM	pasResolvList = NULL;
static HMTX		hmtxResolvList;
static ULONG		ulInitTimes = 0;

THREADRET THREADCALL ResolvThread(PVOID pData);

VOID NetResolvInit()
{
  THREAD	hThread;

  if ( (ulInitTimes++) == 0 )
  {
    SysMutexCreate( &hmtxResolvList );
    SysThreadCreate( &hThread, ResolvThread, NULL );
  }
}

VOID NetResolvDone()
{
  PRESOLVITEM	psNext;

  if ( (--ulInitTimes) == 0 )
  {
    SysMutexLock( &hmtxResolvList );
    while( pasResolvList != NULL )
    {
      psNext = pasResolvList->psNext;
      free( pasResolvList->pszAddr );
      free( pasResolvList );
      pasResolvList = psNext;
    }
    SysMutexUnlock( &hmtxResolvList );

    // --
    SysMutexDestroy( &hmtxResolvList );
  }
}

BOOL NetResolv(PVOID pObject, PSZ pszAddr, PRESOLVCALLBACK cbResolv,
               struct in_addr *psInAddr)
{
  PRESOLVITEM		psResolvItem;

  if ( pszAddr == NULL || *pszAddr == '\0' )
  {
    psInAddr->s_addr = INADDR_ANY;
  }
  else if ( !SysInetAddr( pszAddr, psInAddr ) )
  {
    psResolvItem = malloc( sizeof(RESOLVITEM) );
    if ( psResolvItem == NULL )
    {
      ERROR_NOT_ENOUGHT_MEM();
      return FALSE;
    }

    psResolvItem->pObject	= pObject;
    psResolvItem->pszAddr	= strdup(pszAddr);
    psResolvItem->cbResolv	= cbResolv;

    SysMutexLock( &hmtxResolvList );
    psResolvItem->psNext = pasResolvList;
    pasResolvList	 = psResolvItem;
    SysMutexUnlock( &hmtxResolvList );
    return FALSE;
  }

  return TRUE;
}

VOID NetRemoveObject(PVOID pObject)
{
  PRESOLVITEM		psResolvItem;

  SysMutexLock( &hmtxResolvList );
  for( psResolvItem = pasResolvList; psResolvItem != NULL;
       psResolvItem = psResolvItem->psNext )
  {
    if ( psResolvItem->pObject == pObject )
      psResolvItem->pObject = NULL;
  }
  SysMutexUnlock( &hmtxResolvList );
}


THREADRET THREADCALL ResolvThread(PVOID pData)
{
  PRESOLVITEM		pasResolvItem;
  struct in_addr	sInAddr;
  BOOL			bFound;

  while( ulInitTimes > 0 )
  {
    if ( pasResolvList == NULL )
    {
      SysSleep(200);
      continue;
    }

    SysMutexLock( &hmtxResolvList );

    if ( pasResolvList == NULL || ulInitTimes == 0 )
      break;
    pasResolvItem = pasResolvList;
    pasResolvList = pasResolvItem->psNext;

    SysMutexUnlock( &hmtxResolvList );

    if ( pasResolvItem->pObject != NULL )
    {
      bFound = SysGetHostByName( pasResolvItem->pszAddr, &sInAddr );
      if ( pasResolvItem->pObject != NULL )
      {
        if ( !bFound )
          DETAIL( NET_MESSAGE_NAMENOTFOUND, pasResolvItem->pszAddr,
                  NULL, NULL );

        pasResolvItem->cbResolv( pasResolvItem->pObject, bFound,
                                 pasResolvItem->pszAddr, &sInAddr );
      }
    }

    free( pasResolvItem->pszAddr );
    free( pasResolvItem );
  }

  return THREADRC;
}
