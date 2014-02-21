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

#ifndef FILEHOST_H
#define FILEHOST_H

#include "system.h"
#include "httpserv\netacl.h"
#include "httpserv\requests.h"

#define FILE_HOST_MAX_INDEX_FILE_NAME_LEN	32
#define FILE_HOST_MAX_BASE_PATH_LEN		320

typedef struct _FILEHOST {
  struct _FILEHOST	*psNext;
  struct _FILEHOST	**ppsSelf;
  ULONG			ulNamesCount;
  PSZ			*paszNames;
  ULONG			ulBasePathLen;
  PSZ			pszBasePath;
  ULONG			ulIndexFilesCount;
  PSZ			*paszIndexFiles;
  NETACL  		sAcl;
  ULONG			ulQueryLocks;
  HMTX			hmtxQueryLocks;
} FILEHOST, *PFILEHOST;


VOID FileHostInit();
VOID FileHostDone();
PFILEHOST FileHostNew();
VOID FileHostDestroy(PFILEHOST psFileHost);
BOOL FileHostAddName(PFILEHOST psFileHost, PSZ pszName);
BOOL FileHostSetBasePath(PFILEHOST psFileHost, PSZ pszBasePath);
BOOL FileHostAddIndexFile(PFILEHOST psFileHost, PSZ pszIndexFile);
VOID FileHostReady(PFILEHOST psFileHost);
PFILEHOST FileHostQuery(PSZ pszName);
VOID FileHostRelease(PFILEHOST psFileHost);
VOID FileHostRemoveAll();
BOOL FileHostAclAdd(PFILEHOST psFileHost, PSZ pszAddr, BOOL bAllow);
BOOL FileHostTestAcl(PFILEHOST psFileHost, PCONNINFO psConnInfo);

PFILEHOST FileHostQueryList();
VOID FileHostReleaseList();
#define FileHostGetNext(pfh) (pfh->psNext)
#define FileHostGetBasePath(pfh) (pfh->pszBasePath)
#define FileHostGetNamesCount(pfh) (pfh->ulNamesCount)
#define FileHostGetName(pfh,idx) (pfh->paszNames[idx])
#define FileHostGetIndexFilesCount(pfh) (pfh->ulIndexFilesCount)
#define FileHostGetIndexFile(pfh,idx) (pfh->paszIndexFiles[idx])
#define FileHostGetAcl(pfh) (&pfh->sAcl)

#endif // FILEHOST_H
