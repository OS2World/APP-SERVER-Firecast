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

#ifndef TEMPLATES_H
#define TEMPLATES_H

#include "system.h"
#include "httpserv/reqfields.h"
#include "httpserv/requests.h"
#include "httpserv/netacl.h"

#define MAX_SOURCE_PASSWORD_LENGTH	23
#define UNKNOWN_ID			((ULONG)(-1))

typedef struct _TEMPLATE {
  struct _TEMPLATE	*psNext;
  struct _TEMPLATE	**ppsSelf;
  ULONG			ulId;
  BOOL			bDenied;
  PSZ			pszMountPointWC;
  BOOL			bHidden;
  CHAR			acPassword[MAX_SOURCE_PASSWORD_LENGTH+1];
  ULONG			ulMaxClients;
  ULONG			ulKeepAlive;
  REQFIELDS		sDefaultFields;
  REQFIELDS		sOverrideFields;
  NETACL  		sAclSources;
  NETACL  		sAclClients;
  ULONG			ulQueryLocks;
  HMTX			hmtxQueryLocks;
} TEMPLATE, *PTEMPLATE;


VOID TemplInit();
VOID TemplDone();
ULONG TemplGetNextId();
PTEMPLATE TemplNew(BOOL bDenied);
VOID TemplDestroy(PTEMPLATE psTemplate);
VOID TemplSetHidden(PTEMPLATE psTemplate, BOOL bHidden);
VOID TemplSetMountPointWC(PTEMPLATE psTemplate, PSZ pszMountPointWC);
VOID TemplSetPassword(PTEMPLATE psTemplate, PSZ pszPassword);
VOID TemplSetMaxClients(PTEMPLATE psTemplate, ULONG ulMaxClients);
VOID TemplSetKeepAlive(PTEMPLATE psTemplate, ULONG ulKeepAlive);
VOID TemplAddField(PTEMPLATE psTemplate, PSZ pszName, PSZ pszValue,
                   BOOL bOverride);
BOOL TemplAclSourcesAdd(PTEMPLATE psTemplate, PSZ pszAddr, BOOL bAllow);
BOOL TemplAclClientsAdd(PTEMPLATE psTemplate, PSZ pszAddr, BOOL bAllow);
VOID TemplReady(PTEMPLATE psTemplate);
PTEMPLATE TemplQuery(PSZ pszMountPoint, PCONNINFO psConnInfo, PSZ pszPassword);
BOOL TemplQueryPtr(PTEMPLATE psTemplate);
VOID TemplRelease(PTEMPLATE psTemplate);
VOID TemplApplyFields(PTEMPLATE psTemplate, PREQFIELDS psFields);
VOID TemplRemoveAll();
BOOL TemplTestPassword(PTEMPLATE psTemplate, PSZ pszPassword);
BOOL TemplTestAclClients(PTEMPLATE psTemplate, PCONNINFO psConnInfo);

PTEMPLATE TemplQueryList();
VOID TemplReleaseList();
#define TemplGetNext(ps) (ps->psNext)
#define TemplGetDenied(ps) (ps->bDenied)
#define TemplGetHidden(ps) (ps->bHidden)
#define TemplGetMountPointWC(ps) (ps->pszMountPointWC)
#define TemplGetPassword(ps) (ps->acPassword)
#define TemplGetMaxClients(ps) (ps->ulMaxClients)
#define TemplGetKeepAlive(ps) (ps->ulKeepAlive)
#define TemplGetAclSources(ps) (&ps->sAclSources)
#define TemplGetAclClients(ps) (&ps->sAclClients)
#define TemplGetDefaultFields(ps) (&ps->sDefaultFields)
#define TemplGetOverrideFields(ps) (&ps->sOverrideFields)

#endif // TEMPLATES_H
