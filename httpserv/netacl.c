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
#include "httpserv\netacl.h"


static VOID _AclItemAddrToStr(PNETACLITEM psNetAclItem, PCHAR pcBuf)
{
  ULONG		ulLength;

  SysStrAddr( pcBuf, 24, &psNetAclItem->sAddr );
  ulLength = strlen(pcBuf);
  pcBuf[ulLength] = '/';
  SysStrAddr( &pcBuf[ulLength+1], 24, &psNetAclItem->sMask );
}


VOID NetAclDone(PNETACL psNetAcl)
{
  free( psNetAcl->pasItems );
}

BOOL NetAclAdd(PNETACL psNetAcl, PSZ pszAddr, BOOL bAllow)
{
  CHAR			acBuf[24];
  PCHAR			pcMask;
  ULONG			ulLength;
  struct in_addr	sAddr;
  struct in_addr	sMask = {0};
  LONG			lMask;
  PCHAR			pcPtr;
  PNETACLITEM		psItem;

  while( *pszAddr == ' ' )
    pszAddr++;

  pcMask = strchr( pszAddr, '/' );
  if ( pcMask != NULL )
  {
    ulLength = pcMask - pszAddr;
    if ( ulLength == 0 || ulLength >= sizeof(acBuf) )
      return FALSE;
    memcpy( &acBuf, pszAddr, ulLength );
    acBuf[ulLength] = '\0';
    pcMask++;
    pszAddr = &acBuf;

    lMask = strtol( pcMask, &pcPtr, 10 ); 
    if ( *pcPtr == '.' )
    {
      if ( !SysInetAddr( pcMask, &sMask ) )
        *(PULONG)&sMask = 0xFFFFFFFF;
    }
    else
    {
      if ( lMask < 0 || lMask > 48 )
        return FALSE;
      while( (lMask--) > 0 )
        *(PULONG)&sMask = (*(PULONG)&sMask >> 1) | 0x80000000;
      *(PULONG)&sMask = ntohl( *(PULONG)&sMask );
    }
  }
  else
    *(PULONG)&sMask = 0xFFFFFFFF;

  if ( !SysInetAddr( pszAddr, &sAddr ) )
    return FALSE;

  *(PULONG)&sAddr = *(PULONG)&sAddr & *(PULONG)&sMask; 

  if ( psNetAcl->ulCount == 0 ||
       (psNetAcl->ulCount & 0x07) == 0x07 )
  {
    psItem = realloc( psNetAcl->pasItems,
               (((psNetAcl->ulCount+1) & 0xFFFFFFF8) + 0x08) * sizeof(NETACLITEM) );
    if ( psItem == NULL )
      return NULL;
    psNetAcl->pasItems = psItem;
  }

  psItem = &psNetAcl->pasItems[ psNetAcl->ulCount++ ];
  psItem->bAllow = bAllow;
  psItem->sAddr = sAddr;
  psItem->sMask = sMask;

  return TRUE;
}

BOOL NetAclTest(PNETACL psNetAcl, struct in_addr *psAddr)
{
  ULONG		ulIdx;
  PNETACLITEM	psItem;

  if ( psNetAcl->ulCount == 0 )
    return TRUE;

  for( ulIdx = 0; ulIdx < psNetAcl->ulCount; ulIdx++ )
  {
    psItem = &psNetAcl->pasItems[ulIdx];
    if ( (*(PULONG)psAddr & *(PULONG)&psItem->sMask) == *(PULONG)&psItem->sAddr )
      return psItem->bAllow;
  }

  return FALSE;
}

ULONG NetAclGetCount(PNETACL psNetAcl)
{
  return psNetAcl->ulCount;
}

BOOL NetAclGetItem(PNETACL psNetAcl, ULONG ulIdx, PCHAR pcBuf)
{
  _AclItemAddrToStr( &psNetAcl->pasItems[ulIdx], pcBuf );
  return psNetAcl->pasItems[ulIdx].bAllow;
}
 
