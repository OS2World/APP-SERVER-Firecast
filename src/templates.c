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
#include "templates.h"
#include "match.h"
#include "ssmessages.h"

#define NET_MODULE	SS_MODULE_TEMPLATES

// keep-alive constants in seconds
#define MAX_KEEPALIVE	300
#define DEF_KEEPALIVE	5

static PTEMPLATE	psTemplatesList = NULL;
static PTEMPLATE	*ppsLastTemplate = &psTemplatesList;
static RWMTX		rwmtxTemplatesList;
static ULONG		ulId = 0;
static HMTX		mtxId;

static VOID _TemplateDestroy(PTEMPLATE psTemplate)
{
  if ( psTemplate->pszMountPointWC != NULL )
    free( psTemplate->pszMountPointWC );

  ReqFldDone( &psTemplate->sDefaultFields );
  ReqFldDone( &psTemplate->sOverrideFields );
  NetAclDone( &psTemplate->sAclSources );
  NetAclDone( &psTemplate->sAclClients );
  SysMutexDestroy( &psTemplate->hmtxQueryLocks );
  free( psTemplate );
}

static BOOL _TemplQuery(PTEMPLATE psTemplate)
{
  SysMutexLock( &psTemplate->hmtxQueryLocks );
  if ( psTemplate->ppsSelf == NULL )
  {
    SysMutexUnlock( &psTemplate->hmtxQueryLocks );
    return FALSE;
  }
  psTemplate->ulQueryLocks++;
  SysMutexUnlock( &psTemplate->hmtxQueryLocks );
  return TRUE;
}



VOID TemplInit()
{
  SysRWMutexCreate( &rwmtxTemplatesList );
  SysMutexCreate( &mtxId );
}

VOID TemplDone()
{
  TemplRemoveAll();
  SysRWMutexDestroy( &rwmtxTemplatesList );
  SysMutexDestroy( &mtxId );
}

ULONG TemplGetNextId()
{
  ULONG		ulRes;

  SysMutexLock( &mtxId );
  ulRes = ulId;
  ulId++;
  if ( ulId == UNKNOWN_ID )
    ulId++;
  SysMutexUnlock( &mtxId );
  
  return ulRes;
}

PTEMPLATE TemplNew(BOOL bDenied)
{
  PTEMPLATE	psTemplate;

  psTemplate = malloc( sizeof(TEMPLATE) );
  if ( psTemplate == NULL )
  {
    ERROR_NOT_ENOUGHT_MEM();
    return NULL;
  }

  psTemplate->psNext = NULL;
  psTemplate->ppsSelf = NULL;
  psTemplate->ulId = TemplGetNextId();
  psTemplate->bDenied = bDenied;
  psTemplate->pszMountPointWC = NULL;
  psTemplate->bHidden = FALSE;
  psTemplate->acPassword[0] = '\0';
  psTemplate->ulMaxClients = 0;
  psTemplate->ulKeepAlive = DEF_KEEPALIVE;
  ReqFldInit( &psTemplate->sDefaultFields );
  ReqFldInit( &psTemplate->sOverrideFields );
  NetAclInit( &psTemplate->sAclSources );
  NetAclInit( &psTemplate->sAclClients );
  psTemplate->ulQueryLocks = 0;
  SysMutexCreate( &psTemplate->hmtxQueryLocks );

  return psTemplate;
}

VOID TemplDestroy(PTEMPLATE psTemplate)
{
  if ( psTemplate == NULL )
    return;

  SysMutexLock( &psTemplate->hmtxQueryLocks );

  SysRWMutexLockWrite( &rwmtxTemplatesList );
  if ( psTemplate->ppsSelf != NULL )
  {
    *psTemplate->ppsSelf = psTemplate->psNext;
    if ( psTemplate->psNext != NULL )
      psTemplate->psNext->ppsSelf = psTemplate->ppsSelf;
    else
      ppsLastTemplate = psTemplate->ppsSelf;

    psTemplate->ppsSelf = NULL;
  }
  SysRWMutexUnlockWrite( &rwmtxTemplatesList );

  if ( psTemplate->ulQueryLocks == 0 )
  {
    SysMutexUnlock( &psTemplate->hmtxQueryLocks );
    _TemplateDestroy( psTemplate );
    return;
  }

  SysMutexUnlock( &psTemplate->hmtxQueryLocks );
}

VOID TemplSetHidden(PTEMPLATE psTemplate, BOOL bHidden)
{
  psTemplate->bHidden = bHidden;
}

VOID TemplSetMountPointWC(PTEMPLATE psTemplate, PSZ pszMountPointWC)
{
  if ( psTemplate->pszMountPointWC != NULL )
    free( psTemplate->pszMountPointWC );

  if ( pszMountPointWC != NULL && *pszMountPointWC == '/' )
    pszMountPointWC++;

  psTemplate->pszMountPointWC = 
    pszMountPointWC == NULL || *pszMountPointWC == '\0'
    ? NULL : strdup( pszMountPointWC );
}

VOID TemplSetPassword(PTEMPLATE psTemplate, PSZ pszPassword)
{
  strncpy( &psTemplate->acPassword, pszPassword, MAX_SOURCE_PASSWORD_LENGTH );
  psTemplate->acPassword[MAX_SOURCE_PASSWORD_LENGTH] = '\0';
}

VOID TemplSetMaxClients(PTEMPLATE psTemplate, ULONG ulMaxClients)
{
  psTemplate->ulMaxClients = ulMaxClients;
}

VOID TemplSetKeepAlive(PTEMPLATE psTemplate, ULONG ulKeepAlive)
{
  psTemplate->ulKeepAlive = ulKeepAlive > MAX_KEEPALIVE ? 
                              MAX_KEEPALIVE : ulKeepAlive;
}

VOID TemplAddField(PTEMPLATE psTemplate, PSZ pszName, PSZ pszValue,
                   BOOL bOverride)
{
  PREQFIELDS	psFieldsDest, psFieldsClear;

  if ( pszName == NULL || pszValue == NULL )
    return;

  if ( bOverride )
  {
    psFieldsDest = &psTemplate->sOverrideFields;
    psFieldsClear = &psTemplate->sDefaultFields;
  }
  else
  {
    psFieldsDest = &psTemplate->sDefaultFields;
    psFieldsClear = &psTemplate->sOverrideFields;
  }

  ReqFldSet( psFieldsDest, pszName, pszValue );
  ReqFldDelete( psFieldsClear, pszName );
}

BOOL TemplAclSourcesAdd(PTEMPLATE psTemplate, PSZ pszAddr, BOOL bAllow)
{
  return NetAclAdd( &psTemplate->sAclSources, pszAddr, bAllow);
}

BOOL TemplAclClientsAdd(PTEMPLATE psTemplate, PSZ pszAddr, BOOL bAllow)
{
  return NetAclAdd( &psTemplate->sAclClients, pszAddr, bAllow);
}

VOID TemplReady(PTEMPLATE psTemplate)
{
  SysRWMutexLockWrite( &rwmtxTemplatesList );
  psTemplate->ppsSelf = ppsLastTemplate;
  *ppsLastTemplate = psTemplate;
  ppsLastTemplate = &psTemplate->psNext;
  SysRWMutexUnlockWrite( &rwmtxTemplatesList );
}

PTEMPLATE TemplQuery(PSZ pszMountPoint, PCONNINFO psConnInfo, PSZ pszPassword)
{
  PTEMPLATE	psTemplate;

  if ( pszMountPoint == NULL )
    return NULL;

  if ( *pszMountPoint == '/' )
    pszMountPoint++;

  SysRWMutexLockRead( &rwmtxTemplatesList );

  psTemplate = psTemplatesList;
  while( psTemplate != NULL )
  {
    if ( ( psTemplate->pszMountPointWC == NULL ||
           match(psTemplate->pszMountPointWC, pszMountPoint) )
         &&
         ( psConnInfo == NULL ||
           NetAclTest(&psTemplate->sAclSources, &psConnInfo->sClientAddr) ) )
    {
      if ( TemplGetDenied( psTemplate ) )
      {
        WARNING( SS_MESSAGE_TEMPLDENIED, psConnInfo, pszMountPoint, NULL );
        psTemplate = NULL;
      }
      else if ( pszPassword != NULL &&
                strncmp( &psTemplate->acPassword, pszPassword,
                         MAX_SOURCE_PASSWORD_LENGTH ) != 0 )
      {
        WARNING( SS_MESSAGE_TEMPLINVALIDPSWD, psConnInfo, pszMountPoint, NULL );
        psTemplate = NULL;
      }
      else if ( !_TemplQuery( psTemplate ) )
        psTemplate = NULL;
      
      break;
    }
    psTemplate = psTemplate->psNext;
  }

  SysRWMutexUnlockRead( &rwmtxTemplatesList );
  return psTemplate;
}

BOOL TemplQueryPtr(PTEMPLATE psTemplate)
{
  return _TemplQuery( psTemplate );
}

VOID TemplRelease(PTEMPLATE psTemplate)
{
  if ( psTemplate == NULL )
    return;

  SysMutexLock( &psTemplate->hmtxQueryLocks );
  if ( psTemplate->ulQueryLocks > 0 )
  {
    psTemplate->ulQueryLocks--;
    if ( psTemplate->ulQueryLocks == 0 && psTemplate->ppsSelf == NULL )
    {
      SysMutexUnlock( &psTemplate->hmtxQueryLocks );
      _TemplateDestroy( psTemplate );
      return;
    }
  }
  SysMutexUnlock( &psTemplate->hmtxQueryLocks );
}

VOID TemplApplyFields(PTEMPLATE psTemplate, PREQFIELDS psFields)
{
  ReqFldCopy( psFields, &psTemplate->sOverrideFields );
  ReqFldCopyNew( psFields, &psTemplate->sDefaultFields );
}

VOID TemplRemoveAll()
{
  PTEMPLATE	psTemplate;
  PTEMPLATE	psTemplateNext;

  SysRWMutexLockWrite( &rwmtxTemplatesList );

  psTemplate = psTemplatesList;
  while( psTemplate != NULL )
  {
    psTemplateNext = psTemplate->psNext;
    SysMutexLock( &psTemplate->hmtxQueryLocks );
    if ( psTemplate->ulQueryLocks == 0 )
    {
      SysMutexUnlock( &psTemplate->hmtxQueryLocks );
      _TemplateDestroy( psTemplate );
    }
    else
    {
      psTemplate->ppsSelf = NULL;
      SysMutexUnlock( &psTemplate->hmtxQueryLocks );
    }
    psTemplate = psTemplateNext;
  }

  psTemplatesList = NULL;
  ppsLastTemplate = &psTemplatesList;

  SysRWMutexUnlockWrite( &rwmtxTemplatesList );
}

BOOL TemplTestPassword(PTEMPLATE psTemplate, PSZ pszPassword)
{
  return pszPassword != NULL &&
         strncmp( &psTemplate->acPassword, pszPassword,
                  strlen( &psTemplate->acPassword ) ) == 0;
}

BOOL TemplTestAclClients(PTEMPLATE psTemplate, PCONNINFO psConnInfo)
{
  return NetAclTest( &psTemplate->sAclClients, &psConnInfo->sClientAddr );
}

PTEMPLATE TemplQueryList()
{
  SysRWMutexLockRead( &rwmtxTemplatesList );
  return psTemplatesList;
}

VOID TemplReleaseList()
{
  SysRWMutexUnlockRead( &rwmtxTemplatesList );
}
