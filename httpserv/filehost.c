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
#include "httpserv\filehost.h"

static PFILEHOST	psHostList = NULL;
static PFILEHOST	*ppsLastHost = &psHostList;
static RWMTX		rwmtxHostList;
static ULONG		ulQueryLocks = 0;


static VOID _FileHostDestroy(PFILEHOST psFileHost)
{
  ULONG		ulIdx;

  for( ulIdx = 0; ulIdx < psFileHost->ulNamesCount; ulIdx++ )
    free( psFileHost->paszNames[ulIdx] );
  if ( psFileHost->paszNames != NULL)
    free( psFileHost->paszNames );

  if ( psFileHost->pszBasePath != NULL)
    free( psFileHost->pszBasePath );

  for( ulIdx = 0; ulIdx < psFileHost->ulIndexFilesCount; ulIdx++ )
    free( psFileHost->paszIndexFiles[ulIdx] );
  if ( psFileHost->paszIndexFiles != NULL)
    free( psFileHost->paszIndexFiles );

  NetAclDone( &psFileHost->sAcl );

  SysMutexDestroy( &psFileHost->hmtxQueryLocks );

  free( psFileHost );
}


VOID FileHostInit()
{
  SysRWMutexCreate( &rwmtxHostList );
}

VOID FileHostDone()
{
  FileHostRemoveAll();
  SysRWMutexDestroy( &rwmtxHostList );
}

PFILEHOST FileHostNew()
{
  PFILEHOST	psFileHost;

  psFileHost = malloc( sizeof(FILEHOST) );
  if ( psFileHost != NULL )
  {
    psFileHost->ppsSelf			= NULL;
    psFileHost->ulNamesCount		= 0;
    psFileHost->paszNames		= NULL;
    psFileHost->pszBasePath		= NULL;
    psFileHost->ulIndexFilesCount	= 0;
    psFileHost->paszIndexFiles		= NULL;
    psFileHost->ulQueryLocks		= 0;
    NetAclInit( &psFileHost->sAcl );
    SysMutexCreate( &psFileHost->hmtxQueryLocks );
  }
  return psFileHost;
}

VOID FileHostDestroy(PFILEHOST psFileHost)
{
  SysMutexLock( &psFileHost->hmtxQueryLocks );

  SysRWMutexLockWrite( &rwmtxHostList );
  if ( psFileHost->ppsSelf != NULL )
  {
    *psFileHost->ppsSelf = psFileHost->psNext;
    if ( psFileHost->psNext != NULL )
      psFileHost->psNext->ppsSelf = psFileHost->ppsSelf;
    else
      ppsLastHost = psFileHost->ppsSelf;
  }
  SysRWMutexUnlockWrite( &rwmtxHostList );

  if ( psFileHost->ulQueryLocks == 0 )
  {
    SysMutexUnlock( &psFileHost->hmtxQueryLocks );
    _FileHostDestroy( psFileHost );
    return;
  }
  psFileHost->ppsSelf = NULL;

  SysMutexUnlock( &psFileHost->hmtxQueryLocks );
}

BOOL FileHostAddName(PFILEHOST psFileHost, PSZ pszName)
{
  ULONG		ulIdx;
  PSZ		*paszNames;

  if ( pszName == NULL || pszName[0] == '\0' )
    return FALSE;

  for( ulIdx = 0; ulIdx < psFileHost->ulNamesCount; ulIdx++ )
    if ( stricmp( psFileHost->paszNames[ulIdx], pszName ) == 0 )
      return FALSE;

  pszName = strdup( pszName );
  if ( pszName == NULL )
    return FALSE;

  if ( psFileHost->ulNamesCount == 0 ||
       (psFileHost->ulNamesCount & 0x07) == 0x07 )
  {
    paszNames = realloc( psFileHost->paszNames,
               (((psFileHost->ulNamesCount+1) & 0xFFFFFFF8) + 0x08) * sizeof(PSZ) );
    if ( paszNames == NULL )
    {
      free( pszName );
      return FALSE;
    }
    psFileHost->paszNames = paszNames;
  }

  psFileHost->paszNames[ psFileHost->ulNamesCount++ ] = pszName;
  return TRUE;
}

BOOL FileHostSetBasePath(PFILEHOST psFileHost, PSZ pszBasePath)
{
  ULONG		ulLength;
  PSZ		pszNewBasePath;

  if ( pszBasePath == NULL || pszBasePath[0] == '\0' ||
       strlen( pszBasePath ) > FILE_HOST_MAX_BASE_PATH_LEN )
    return FALSE;

  ulLength = strlen( pszBasePath );
  if ( pszBasePath[ulLength-1] == '\\' || pszBasePath[ulLength-1] == '/' )
  {
    ulLength--;
    if ( ulLength == 0 )
      return FALSE;
  }

  pszNewBasePath = malloc( ulLength + 1 );
  if ( pszNewBasePath == NULL )
    return FALSE;

  memcpy( pszNewBasePath, pszBasePath, ulLength );
  pszNewBasePath[ulLength] = '\0';

  if ( psFileHost->pszBasePath != NULL )
    free( psFileHost->pszBasePath );

  psFileHost->pszBasePath = pszNewBasePath;
  psFileHost->ulBasePathLen = ulLength;
  return TRUE;
}

BOOL FileHostAddIndexFile(PFILEHOST psFileHost, PSZ pszIndexFile)
{
  ULONG		ulIdx;
  PSZ		*paszIndexFiles;

  if ( pszIndexFile == NULL || pszIndexFile[0] == '\0' ||
       strlen( pszIndexFile ) > FILE_HOST_MAX_INDEX_FILE_NAME_LEN )
    return FALSE;

  for( ulIdx = 0; ulIdx < psFileHost->ulIndexFilesCount; ulIdx++ )
    if ( stricmp( psFileHost->paszIndexFiles[ulIdx], pszIndexFile ) == 0 )
      return FALSE;

  pszIndexFile = strdup( pszIndexFile );
  if ( pszIndexFile == NULL )
    return FALSE;

  if ( psFileHost->ulIndexFilesCount == 0 ||
       (psFileHost->ulIndexFilesCount & 0x07) == 0x07 )
  {
    paszIndexFiles = realloc( psFileHost->paszIndexFiles,
               (((psFileHost->ulIndexFilesCount+1) & 0xFFFFFFF8) + 0x08) *
               sizeof(PSZ) );
    if ( paszIndexFiles == NULL )
    {
      free( pszIndexFile );
      return FALSE;
    }
    psFileHost->paszIndexFiles = paszIndexFiles;
  }

  psFileHost->paszIndexFiles[ psFileHost->ulIndexFilesCount++ ] = pszIndexFile;
  return TRUE;
}

VOID FileHostReady(PFILEHOST psFileHost)
{
  psFileHost->ulQueryLocks = 0;
  psFileHost->psNext	= NULL;

  SysRWMutexLockWrite( &rwmtxHostList );
  psFileHost->ppsSelf	= ppsLastHost;
  *ppsLastHost = psFileHost;
  ppsLastHost = &psFileHost->psNext;
  SysRWMutexUnlockWrite( &rwmtxHostList );
}

PFILEHOST FileHostQuery(PSZ pszName)
{
  PFILEHOST	psFileHost;
  ULONG		ulIdx;
  ULONG		ulLength;
  PCHAR		pcPtr;
  BOOL		bFound;

  if ( pszName != NULL )
  {
    if ( pcPtr = strchr( pszName, ':' ) )
      ulLength = pcPtr - pszName;
    else
      ulLength = strlen(pszName);
  }
 
  SysRWMutexLockRead( &rwmtxHostList );

  psFileHost = psHostList;
  while( psFileHost )
  {
    if ( psFileHost->ulNamesCount == 0 )
    {
      bFound = TRUE;
    }
    else
    {
      bFound = FALSE;
      if ( pszName != NULL )
        for( ulIdx = 0; ulIdx < psFileHost->ulNamesCount; ulIdx++ )
          if ( ulLength == strlen( psFileHost->paszNames[ulIdx] ) &&
               memicmp( psFileHost->paszNames[ulIdx], pszName, ulLength ) == 0 )
          {
            bFound = TRUE;
            break;
          }
    }

    if ( bFound )
    {
      if ( psFileHost->pszBasePath == NULL )
        psFileHost = NULL;
      break;
    }

    psFileHost = psFileHost->psNext;
  }

  if ( psFileHost )
  {
    SysMutexLock( &psFileHost->hmtxQueryLocks );
    psFileHost->ulQueryLocks++;
    SysMutexUnlock( &psFileHost->hmtxQueryLocks );
  }

  SysRWMutexUnlockRead( &rwmtxHostList );

  return psFileHost;
}

VOID FileHostRelease(PFILEHOST psFileHost)
{
  if ( psFileHost == NULL )
    return;

  SysMutexLock( &psFileHost->hmtxQueryLocks );
  if ( psFileHost->ulQueryLocks > 0 )
  {
    psFileHost->ulQueryLocks--;
    if ( psFileHost->ulQueryLocks == 0 && psFileHost->ppsSelf == NULL )
    {
      SysMutexUnlock( &psFileHost->hmtxQueryLocks );
      _FileHostDestroy( psFileHost ); // ppsSelf=NULL ==> don't need lock here
      return;
    }
  }
  SysMutexUnlock( &psFileHost->hmtxQueryLocks );
}

VOID FileHostRemoveAll()
{
  PFILEHOST	psFileHost;
  PFILEHOST	psFileHostNext;

  SysRWMutexLockWrite( &rwmtxHostList );

  psFileHost = psHostList;
  while( psFileHost )
  {
    psFileHostNext = psFileHost->psNext;
    SysMutexLock( &psFileHost->hmtxQueryLocks );
    if ( psFileHost->ulQueryLocks == 0 )
    {
      SysMutexUnlock( &psFileHost->hmtxQueryLocks );
      _FileHostDestroy( psFileHost );
    }
    else
    {
      psFileHost->ppsSelf = NULL;
      SysMutexUnlock( &psFileHost->hmtxQueryLocks );
    }
    psFileHost = psFileHostNext;
  }
  psHostList = NULL;
  ppsLastHost = &psHostList;

  SysRWMutexUnlockWrite( &rwmtxHostList );
}

BOOL FileHostAclAdd(PFILEHOST psFileHost, PSZ pszAddr, BOOL bAllow)
{
  return NetAclAdd( &psFileHost->sAcl, pszAddr, bAllow);
}

BOOL FileHostTestAcl(PFILEHOST psFileHost, PCONNINFO psConnInfo)
{
  return NetAclTest( &psFileHost->sAcl, &psConnInfo->sClientAddr );
}

PFILEHOST FileHostQueryList()
{
  SysRWMutexLockRead( &rwmtxHostList );
  return psHostList;
}

VOID FileHostReleaseList()
{
  SysRWMutexUnlockRead( &rwmtxHostList );
}
