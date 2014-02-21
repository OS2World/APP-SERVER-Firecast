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
#include "httpserv/fileres.h"
#include "ssfileres.h"
#include "sources.h"
#include "clients.h"
#include "shoutserv.h"
#include "control.h"
#include "logmsg.h"
#include "ssmessages.h"

#define PD_TEMPLATES	0
#define PD_HOSTS	1
#define PD_GENERAL	2
#define PD_ADMIN	3
#define PD_RELAYS	4

#define NET_MODULE	SS_MODULE_FILERES

static GETMETHODSCALLBACK	*PrevGetMethods;


// object: Source

typedef struct _DATARESSOURCE {
  PSOURCE	psSource;
} DATARESSOURCE, *PDATARESSOURCE;

static ULONG _SourceOpen(PVOID pData, PFILEHOST psFileHost, PSZ pszPath,
                         PSZ pszParam, PSZ pszMethod, PREQFIELDS psFields,
                         PCONNINFO psConnInfo);
static VOID _SourceClose(PVOID pData);
static ULONG _SourceWrite(PVOID pData, PCHAR pcBuf, ULONG ulBufLen,
                     PULONG pulActual);

static struct _FILEMETHODS	sSourceMethods =
{
  _SourceOpen, _SourceClose, NULL, _SourceWrite, NULL, NULL, NULL, NULL
};


// object: Client

typedef struct _DATARESCLIENT {
  CLIENT	sClient;
} DATARESCLIENT, *PDATARESCLIENT;

static ULONG _ClientOpen(PVOID pData, PFILEHOST psFileHost, PSZ pszPath,
                         PSZ pszParam, PSZ pszMethod, PREQFIELDS psFields,
                         PCONNINFO psConnInfo);
static VOID _ClientClose(PVOID pData);
static ULONG _ClientRead(PVOID pData, PCHAR pcBuf, ULONG ulBufLen,
                     PULONG pulActual);
static VOID _ClientGetFields(PVOID pData, RESPCODE rcRespCode, PREQFIELDS psRespFields);

static struct _FILEMETHODS	sClientMethods =
{
  _ClientOpen, _ClientClose, _ClientRead, NULL, NULL, _ClientGetFields, NULL, NULL
};


// object: Meta

static ULONG _MetaDataOpen(PVOID pData, PFILEHOST psFileHost, PSZ pszPath,
                         PSZ pszParam, PSZ pszMethod, PREQFIELDS psFields,
                         PCONNINFO psConnInfo);
static VOID _MetaDataClose(PVOID pData);

static struct _FILEMETHODS	sMetaDataMethods =
{
  _MetaDataOpen, _MetaDataClose, NULL, NULL, NULL, NULL, NULL
};


// object: WebInterface

typedef struct _DATAWEBINTERFACE {
  PCHAR		pcOutput;
  ULONG		ulOutputSize;
  ULONG		ulOutputPos;
  CONFIGINPUT	sConfigInput;
  BOOL		bInputFieldData;
  ULONG		ulPostData; // PD_XXXXX
} DATAWEBINTERFACE, *PDATAWEBINTERFACE;

static ULONG _WebInterfaceOpen(PVOID pData, PFILEHOST psFileHost, PSZ pszPath,
                         PSZ pszParam, PSZ pszMethod, PREQFIELDS psFields,
                         PCONNINFO psConnInfo);
static VOID _WebInterfaceClose(PVOID pData);
static ULONG _WebInterfaceRead(PVOID pData, PCHAR pcBuf, ULONG ulBufLen,
                               PULONG pulActual);
static ULONG _WebInterfaceWrite(PVOID pData, PCHAR pcBuf, ULONG ulBufLen,
                                PULONG pulActual);
static VOID _WebInterfaceGetFields(PVOID pData, RESPCODE rcRespCode, PREQFIELDS psRespFields);

static struct _FILEMETHODS	sWebInterfaceMethods =
{
  _WebInterfaceOpen, _WebInterfaceClose, _WebInterfaceRead, _WebInterfaceWrite,
  NULL, _WebInterfaceGetFields
};


// object: Log

typedef struct _DATARESLOG {
  LOG	sLog;
} DATARESLOG, *PDATARESLOG;

static ULONG _LogOpen(PVOID pData, PFILEHOST psFileHost, PSZ pszPath,
                      PSZ pszParam, PSZ pszMethod, PREQFIELDS psFields, PCONNINFO psConnInfo);
static VOID _LogClose(PVOID pData);
static ULONG _LogRead(PVOID pData, PCHAR pcBuf, ULONG ulBufLen, PULONG pulActual);
static VOID _LogGetFields(PVOID pData, RESPCODE rcRespCode, PREQFIELDS psRespFields);

static struct _FILEMETHODS	sLogMethods =
{
  _LogOpen, _LogClose, _LogRead, NULL, NULL, _LogGetFields
};


// object: M3U

typedef struct _DATAM3U {
  PFILERES	psFileRes;
} DATAM3U, *PDATAM3U;

static ULONG _M3UOpen(PVOID pData, PFILEHOST psFileHost, PSZ pszPath,
                      PSZ pszParam, PSZ pszMethod, PREQFIELDS psFields, PCONNINFO psConnInfo);
static VOID _M3UClose(PVOID pData);
static ULONG _M3URead(PVOID pData, PCHAR pcBuf, ULONG ulBufLen, PULONG pulActual);
static VOID _M3UGetFields(PVOID pData, RESPCODE rcRespCode, PREQFIELDS psRespFields);

static struct _FILEMETHODS	sM3UMethods =
{
  _M3UOpen, _M3UClose, _M3URead, NULL, NULL, _M3UGetFields
};


// object: PLS

typedef struct _DATAPLS {
  PFILERES	psFileRes;
} DATAPLS, *PDATAPLS;

static ULONG _PLSOpen(PVOID pData, PFILEHOST psFileHost, PSZ pszPath,
                      PSZ pszParam, PSZ pszMethod, PREQFIELDS psFields, PCONNINFO psConnInfo);
static VOID _PLSClose(PVOID pData);
static ULONG _PLSRead(PVOID pData, PCHAR pcBuf, ULONG ulBufLen, PULONG pulActual);
static VOID _PLSGetFields(PVOID pData, RESPCODE rcRespCode, PREQFIELDS psRespFields);

static struct _FILEMETHODS	sPLSMethods =
{
  _PLSOpen, _PLSClose, _PLSRead, NULL, NULL, _PLSGetFields
};


static ULONG _TestAdminLogin(PREQFIELDS psFields, PCONNINFO psConnInfo)
{
  CHAR		acName[MAX_ADMIN_NAME_LENGTH+1];
  PSZ		pszPassword;

  if ( FileResGetBasicAuthFld( psFields, &acName, sizeof(acName), &pszPassword )
       == NULL )
  {
    return FILERES_UNAUTHORIZED;
  }

  if ( !CtrlTestAcl( psConnInfo ) )
    return FILERES_FORBIDDEN;

  if ( !CtrlTestLogin( &acName, pszPassword ) )
    return FILERES_UNAUTHORIZED;

  return FILERES_OK;
}


// _GetMethods - is a new method's (object's) selector
// It must test file's web-path and return methods and size of object's data

static ULONG _GetMethods( PFILEMETHODS *ppsMethods, PFILEHOST psFileHost, 
                          PSZ pszPath, PSZ pszMethod )
{
  PSOURCE	psSource;
  ULONG		ulLength;
  ULONG		ulRC;

  if ( strcmp( pszMethod, "SOURCE" ) == 0 )
  {
    *ppsMethods = &sSourceMethods;
    return sizeof( DATARESSOURCE );
  }
  else if ( strcmp( pszMethod, "GET" ) == 0 )
  {
    if ( stricmp( pszPath, "/admin/metadata" ) == 0 ||
         stricmp( pszPath, "/admin.cgi" ) == 0 )
    {
      *ppsMethods = &sMetaDataMethods;
      return 0;
    }

    if ( stricmp( pszPath, "/admin/source" ) == 0 ||
         stricmp( pszPath, "/admin/sources" ) == 0 ||
         stricmp( pszPath, "/admin/source-info" ) == 0 ||
         stricmp( pszPath, "/admin/sources-info" ) == 0 ||
         stricmp( pszPath, "/admin/templates" ) == 0 ||
         stricmp( pszPath, "/admin/hosts" ) == 0 ||
         stricmp( pszPath, "/admin/general" ) == 0 ||
         stricmp( pszPath, "/admin/admin" ) == 0 ||
         stricmp( pszPath, "/admin/relays" ) == 0 )
    {
      *ppsMethods = &sWebInterfaceMethods;
      return sizeof( DATAWEBINTERFACE );
    }

    if ( stricmp( pszPath, "/admin/log" ) == 0 )
    {
      *ppsMethods = &sLogMethods;
      return sizeof( DATARESLOG );
    }

    if ( stricmp( pszPath, "/listen.pls" ) == 0 )
    {
      *ppsMethods = &sPLSMethods;
      return sizeof( DATAPLS );
    }

    ulLength = strlen( pszPath );
    if ( (ulLength > 5) && (stricmp( &pszPath[ulLength-4], ".M3U" ) == 0) )
    {
      pszPath[ulLength-4] = '\0';
      psSource = SourceQuery( pszPath );
      if ( psSource != NULL )
      {
        SourceRelease( psSource );
        *ppsMethods = &sM3UMethods;
        return sizeof( DATAM3U );
      }
    }

    psSource = SourceQuery( pszPath );
    if ( psSource != NULL )
    {
      SourceRelease( psSource );
      *ppsMethods = &sClientMethods;
      return sizeof( DATARESCLIENT );
    }
  }
  else if ( strcmp( pszMethod, "POST" ) == 0 )
  {
    if ( stricmp( pszPath, "/admin/templates" ) == 0 ||
         stricmp( pszPath, "/admin/hosts" ) == 0 ||
         stricmp( pszPath, "/admin/general" ) == 0 ||
         stricmp( pszPath, "/admin/admin" ) == 0 ||
         stricmp( pszPath, "/admin/relays" ) == 0 )
    {
      *ppsMethods = &sWebInterfaceMethods;
      return sizeof( DATAWEBINTERFACE );
    }
  }

  return PrevGetMethods( ppsMethods, psFileHost, pszPath, pszMethod );
}


/********************************************************************/
/*                                                                  */
/*                    "Source" object's methods                     */
/*                                                                  */
/********************************************************************/

static ULONG _SourceOpen(PVOID pData, PFILEHOST psFileHost, PSZ pszPath,
                         PSZ pszParam, PSZ pszMethod, PREQFIELDS psFields,
                         PCONNINFO psConnInfo)
{
  PDATARESSOURCE	psData = (PDATARESSOURCE)pData;
  ULONG			ulRC;
  CHAR			acBuf[128];
  PSZ			pszPassword;
  PTEMPLATE		psTemplate;

  SysStrAddr( acBuf, sizeof(acBuf), &psConnInfo->sClientAddr );

  if ( FileResGetBasicAuthFld( psFields, &acBuf, sizeof(acBuf), &pszPassword )
       == NULL )
  {
    return FILERES_UNAUTHORIZED;
  }

  psTemplate = TemplQuery( pszPath, psConnInfo, pszPassword );
  if ( psTemplate == NULL )
    return FILERES_NOT_FOUND;

  ulRC = SourceCreate( &psData->psSource, pszPath, psFields, psConnInfo,
                    psTemplate );

  TemplRelease( psTemplate );

  switch( ulRC )
  {
    case SCRES_INVALID_MPOINT:
      return FILERES_NOT_FOUND;
    case SCRES_MPOINT_EXIST:
      return FILERES_NOT_ALLOWED;
    case SCRES_NOT_ENOUGHT_MEM:
      return FILERES_INTERNAL_ERROR;
    case SCRES_INVALID_STREAM:
      return FILERES_INTERNAL_ERROR;
  }
 
  return FILERES_OK_ICES;
}

static VOID _SourceClose(PVOID pData)
{
  PDATARESSOURCE	psData = (PDATARESSOURCE)pData;

  SourceDestroy( psData->psSource );
}

static ULONG _SourceWrite(PVOID pData, PCHAR pcBuf, ULONG ulBufLen,
                     PULONG pulActual)
{
  PDATARESSOURCE	psData = (PDATARESSOURCE)pData;
  ULONG			ulRC;

  ulRC = SourceWrite( psData->psSource, pcBuf, ulBufLen );

  if ( pulActual != NULL )
    *pulActual = ulBufLen;
  return ulRC == SCRES_OK ? FILERES_WAIT_DATA : FILERES_INTERNAL_ERROR;
}

static ULONG _SourceGetLength(PVOID pData, PULLONG pullLength)
{
  *pullLength = 0;
  return FILERES_NOT_ALLOWED;
}


/********************************************************************/
/*                                                                  */
/*                    "Client" object's methods                     */
/*                                                                  */
/********************************************************************/

static ULONG _ClientOpen(PVOID pData, PFILEHOST psFileHost, PSZ pszPath,
                         PSZ pszParam, PSZ pszMethod, PREQFIELDS psFields,
                         PCONNINFO psConnInfo)
{
  PDATARESCLIENT	psData = (PDATARESCLIENT)pData;
  PSOURCE		psSource;
  ULONG			ulRC;

  psSource = SourceQuery( pszPath );
  if ( psSource == NULL )
    return FILERES_NOT_FOUND;

  ulRC = ClientCreate( &psData->sClient, psSource, psFields, psConnInfo );
  if ( ulRC != SCRES_OK )
  {
    SourceRelease( psSource );
    return ulRC == SCRES_ACCESS_DENIED ? FILERES_FORBIDDEN : FILERES_NOT_FOUND;
  }

  SourceRelease( psSource );
  return FILERES_OK;
}

static VOID _ClientClose(PVOID pData)
{
  PDATARESCLIENT	psData = (PDATARESCLIENT)pData;

  ClientDestroy( &psData->sClient );
}

static ULONG _ClientRead(PVOID pData, PCHAR pcBuf, ULONG ulBufLen,
                     PULONG pulActual)
{
  PDATARESCLIENT	psData = (PDATARESCLIENT)pData;
  ULONG			ulRC;

  ulRC = ClientRead( &psData->sClient, pcBuf, &ulBufLen );

  if ( ulRC == SCRES_OK )
  {
    *pulActual = ulBufLen;
    return FILERES_OK;
  }
  else
    return FILERES_NO_MORE_DATA;
}

static VOID _ClientGetFields(PVOID pData, RESPCODE rcRespCode, PREQFIELDS psRespFields)
{
  PDATARESCLIENT	psData = (PDATARESCLIENT)pData;
  ULONG			ulRC;

  if ( psData != NULL )
    ulRC = ClientGetFields( &psData->sClient, psRespFields );
}


/********************************************************************/
/*                                                                  */
/*                    "Meta" object's methods                     */
/*                                                                  */
/********************************************************************/

static ULONG _MetaDataOpen(PVOID pData, PFILEHOST psFileHost, PSZ pszPath,
                         PSZ pszParam, PSZ pszMethod, PREQFIELDS psFields,
                         PCONNINFO psConnInfo)
{
  PSZ			pszKey;
  PSZ			pszValue;
  PSZ			pszKeyMode = NULL;
  PSZ			pszKeyMount = NULL;
  PSZ			pszKeySong = NULL;
  PSZ			pszKeyPass = NULL;
  PSOURCE		psSource;
  BOOL			bUpdated;
  CHAR			acBuf[128];

  while( pszKey = FileResURIParam( &pszParam, &pszValue ) )
  {
    strlwr( pszKey );
    if ( strcmp( pszKey, "mode" ) == 0 )
      pszKeyMode = pszValue;
    else if ( strcmp( pszKey, "mount" ) == 0 ) // icecast
      pszKeyMount = pszValue;
    else if ( strcmp( pszKey, "song" ) == 0 )
      pszKeySong = pszValue;
    else if ( strcmp( pszKey, "pass" ) == 0 ) // shoutcast
      pszKeyPass = pszValue;
  }

  if ( pszKeyMode == NULL || stricmp( pszKeyMode, "updinfo" ) != 0 )
  {
    WARNING( SS_MESSAGE_INVALIDMETAREQUEST, psConnInfo, pszKeyMode, NULL );
    return FILERES_OK;
  }

  if ( pszKeyMount != NULL )
  {
    if ( FileResGetBasicAuthFld( psFields, &acBuf, sizeof(acBuf), &pszKeyPass )
         == NULL )
    {
      WARNING( SS_MESSAGE_UNAUTHORIZED, psConnInfo, pszPath, NULL );
      return FILERES_UNAUTHORIZED;
    }
    psSource = SourceQuery( pszKeyMount );
  }
  else
    psSource = ShoutServQuerySource( psConnInfo->usServPort + 1,
                                     &psConnInfo->sClientAddr );

  if ( psSource == NULL )
  {
    DBGINFO( SS_MESSAGE_SOURCENOTFOUND, psConnInfo, pszKeyMount, NULL );
    return FILERES_OK;
  }

  bUpdated = SourceTestPassword( psSource, pszKeyPass ) &&
             SourceSetMeta( psSource, pszKeySong );
  DBGINFO( SS_MESSAGE_SETMETADATA, psConnInfo, pszKeyMount, (PVOID)bUpdated );

  SourceRelease( psSource );

  return FILERES_OK;
}

static VOID _MetaDataClose(PVOID pData)
{
}


/********************************************************************/
/*                                                                  */
/*                 "WebInterface" object's methods                  */
/*                                                                  */
/********************************************************************/

static ULONG _WebInterfaceOpen(PVOID pData, PFILEHOST psFileHost, PSZ pszPath,
                         PSZ pszParam, PSZ pszMethod, PREQFIELDS psFields,
                         PCONNINFO psConnInfo)
{
  PDATAWEBINTERFACE	psData = (PDATAWEBINTERFACE)pData;
  ULONG			ulId = UNKNOWN_ID;
  ULONG			ulClientId = UNKNOWN_ID;
  PSZ			pszKey;
  PSZ			pszValue;
  BOOL			bStop;
  BOOL			bRes;
  ULONG			ulRes;
  ULONG			ulPart;

  psData->pcOutput = NULL;
  psData->ulOutputSize = 0;
  psData->ulOutputPos = 0;
  CtrlInputNew( &psData->sConfigInput );
  psData->bInputFieldData = FALSE;

  if ( strcmp( pszMethod, "POST" ) == 0 )
  {
    if ( !FileResTestContentType(psFields, "text/plain") )
      return FILERES_NOT_IMPLEMENTED;

    if ( stricmp( pszPath, "/admin/templates" ) == 0 )
      psData->ulPostData = PD_TEMPLATES;
    else if ( stricmp( pszPath, "/admin/hosts" ) == 0 )
      psData->ulPostData = PD_HOSTS;
    else if ( stricmp( pszPath, "/admin/general" ) == 0 )
      psData->ulPostData = PD_GENERAL;
    else if ( stricmp( pszPath, "/admin/admin" ) == 0 )
      psData->ulPostData = PD_ADMIN;
    else if ( stricmp( pszPath, "/admin/relays" ) == 0 )
      psData->ulPostData = PD_RELAYS;
    else
      return FILERES_NOT_ALLOWED;

    ulRes = _TestAdminLogin( psFields, psConnInfo );
    return ulRes != FILERES_OK ? ulRes : FILERES_WAIT_DATA;
  }

  // get URI fields

  bStop = FALSE;
  while( pszKey = FileResURIParam( &pszParam, &pszValue ) )
  {
    strlwr( pszKey );
    if ( strcmp( pszKey, "id" ) == 0 )
    {
      ulId = strtoul( pszValue, &pszKey, 10 );
      if ( pszValue == pszKey )
        ulId = UNKNOWN_ID;
    }
    else if ( strcmp( pszKey, "cid" ) == 0 )
    {
      ulClientId = strtoul( pszValue, &pszKey, 10 );
      if ( pszValue == pszKey )
        ulClientId = UNKNOWN_ID;
    }
    else if ( strcmp( pszKey, "stop" ) == 0 )
      bStop = *(PUSHORT)pszValue == (USHORT)(0x0031); // "1\0"
  }

  pszPath += 7;		// "/admin/"

  // public info

  if ( stricmp( pszPath, "sources-info" ) == 0 )
  {
    bRes = CtrlQueryTextSources( &psData->pcOutput, &psData->ulOutputSize,
                                 "sources", 0 );
  }
  else if ( stricmp( pszPath, "source-info" ) == 0 )
  {
    bRes = CtrlQueryTextSource( &psData->pcOutput, &psData->ulOutputSize,
                                "source", ulId, FLSRC_META_HISTORY );
  }

  // admin's interface

  else if ( stricmp( pszPath, "sources" ) == 0 )
  {
    ulRes = _TestAdminLogin( psFields, psConnInfo );
    if ( ulRes != FILERES_OK )
      return ulRes;

    bRes = CtrlQueryTextSources( &psData->pcOutput, &psData->ulOutputSize,
	  "sources", FLSRC_ADMIN_MODE | FLSRC_GET_HIDDEN | FLSRC_TEMPLATE_ID );
  }
  else if ( stricmp( pszPath, "source" ) == 0 )
  {
    ulRes = _TestAdminLogin( psFields, psConnInfo );
    if ( ulRes != FILERES_OK )
      return ulRes;

    if ( bStop )
    {
      if ( ulClientId == UNKNOWN_ID )
        bRes = CtrlQueryTextSourceStop( &psData->pcOutput,
                                        &psData->ulOutputSize, ulId );
      else
        bRes = CtrlQueryTextClientStop( &psData->pcOutput,
                                     &psData->ulOutputSize, ulId, ulClientId );
    }
    else
      bRes = CtrlQueryTextSource( &psData->pcOutput, &psData->ulOutputSize,
               "source", ulId, FLSRC_ADMIN_MODE | FLSRC_GET_HIDDEN |
               FLSRC_TEMPLATE_ID | FLSRC_META_HISTORY | FLSRC_CLIENTS |
               FLSRC_SRCADDR );
  }
  else
  {
    if ( stricmp( pszPath, "templates" ) == 0 )
      ulPart = CFGPART_TEMPLATES;
    else if ( stricmp( pszPath, "hosts" ) == 0 )
      ulPart = CFGPART_HOSTS;
    else if ( stricmp( pszPath, "general" ) == 0 )
      ulPart = CFGPART_GENERAL;
    else if ( stricmp( pszPath, "admin" ) == 0 )
      ulPart = CFGPART_ADMIN;
    else if ( stricmp( pszPath, "relays" ) == 0 )
      ulPart = CFGPART_RELAYS;
    else 
      return FILERES_NOT_FOUND;

    ulRes = _TestAdminLogin( psFields, psConnInfo );
    if ( ulRes != FILERES_OK )
      return ulRes;

    if ( ( ulPart == CFGPART_RELAYS ) && ( ulId != UNKNOWN_ID ) )
    {
      if ( bStop )
        bRes = CtrlQueryTextRemoveRelay( &psData->pcOutput,
                                         &psData->ulOutputSize, ulId );
      else
        bRes = CtrlQueryTextResetRelay( &psData->pcOutput,
                                        &psData->ulOutputSize, ulId );
    }
    else
      bRes = CtrlQueryTextConfig( &psData->pcOutput, &psData->ulOutputSize,
                                  ulPart );
  }

  return bRes ? FILERES_OK : FILERES_INTERNAL_ERROR;
}

static VOID _WebInterfaceClose(PVOID pData)
{
  PDATAWEBINTERFACE	psData = (PDATAWEBINTERFACE)pData;

  CtrlInputDestroy( &psData->sConfigInput );

  if ( psData->pcOutput != NULL )
    CtrlReleaseText( psData->pcOutput );
}

static ULONG _WebInterfaceRead(PVOID pData, PCHAR pcBuf, ULONG ulBufLen,
                               PULONG pulActual)
{
  PDATAWEBINTERFACE	psData = (PDATAWEBINTERFACE)pData;
  ULONG			ulLength;

  ulLength = min( ulBufLen, psData->ulOutputSize - psData->ulOutputPos );
  if ( ulLength != 0 )
  {
    memcpy( pcBuf, &psData->pcOutput[psData->ulOutputPos], ulLength );
    psData->ulOutputPos += ulLength;
  }
  *pulActual = ulLength;

  return psData->ulOutputSize == psData->ulOutputPos ?
           FILERES_NO_MORE_DATA : FILERES_OK;
}

static ULONG _WebInterfaceWrite(PVOID pData, PCHAR pcBuf, ULONG ulBufLen,
                                PULONG pulActual)
{
  PDATAWEBINTERFACE	psData = (PDATAWEBINTERFACE)pData;
  BOOL			bRes;

  if ( ulBufLen == 0 )
  {
    ULONG	ulMsgFlags;
/*
    switch( psData->ulPostData )
    {
      case PD_TEMPLATES:
        ulMsgFlags = FLMSG_PART_TEMPLATES;
        break;
      case PD_HOSTS:
        ulMsgFlags = FLMSG_PART_HOSTS;
        break;
      case PD_GENERAL:
        ulMsgFlags = FLMSG_PART_GENERAL;
        break;
      case PD_ADMIN:
        ulMsgFlags = FLMSG_PART_ADMIN;
        break;
    }
*/
    bRes = CtrlInputApply( &psData->sConfigInput );

    ulMsgFlags = FLMSG_PART_ADMIN | FLMSG_PART_GENERAL
                 | FLMSG_PART_HOSTS | FLMSG_PART_TEMPLATES | FLMSG_PART_RELAYS;

    CtrlQueryTextMessage( &psData->pcOutput, &psData->ulOutputSize,
                          bRes ? MSGID_CFG_UPDATED : MSGID_XML_PARCE_ERROR,
                          FLSRC_ADMIN_MODE | ulMsgFlags );

    return FILERES_OK;
  }

  *pulActual = ulBufLen;

  if ( !psData->bInputFieldData )
  {
    PCHAR	pcPtr = memchr( pcBuf, '=', ulBufLen );

    if ( pcPtr == NULL )
      return FILERES_OK;

    psData->bInputFieldData = TRUE;

    pcPtr++;
    ulBufLen -= pcPtr - pcBuf;
    if ( ulBufLen == 0 )
      return FILERES_OK;

    pcBuf = pcPtr;
  }

  bRes = CtrlInputWrite( &psData->sConfigInput, pcBuf, ulBufLen );
  if ( !bRes )
  {
    CtrlQueryTextMessage( &psData->pcOutput, &psData->ulOutputSize,
                          MSGID_XML_PARCE_ERROR, FLSRC_ADMIN_MODE );
    return FILERES_OK;
  }

  return FILERES_WAIT_DATA;
}

static VOID _WebInterfaceGetFields(PVOID pData, RESPCODE rcRespCode, PREQFIELDS psRespFields)
{
  PDATAWEBINTERFACE	psData = (PDATAWEBINTERFACE)pData;
  CHAR			acBuf[36];

  if ( psData != NULL )
  {
    ReqFldSet( psRespFields, "Content-Type", "application/xml" );

    ulltoa( psData->ulOutputSize, &acBuf, 10 ); 
    ReqFldSet( psRespFields, "Content-Length", &acBuf );
  }
}


/********************************************************************/
/*                                                                  */
/*                      "Log" object's methods                      */
/*                                                                  */
/********************************************************************/

static ULONG _LogOpen(PVOID pData, PFILEHOST psFileHost, PSZ pszPath,
                      PSZ pszParam, PSZ pszMethod, PREQFIELDS psFields, PCONNINFO psConnInfo)
{
  PDATARESLOG	psData = (PDATARESLOG)pData;
  BOOL		bRes;
  ULONG		ulRes;

  ulRes = _TestAdminLogin( psFields, psConnInfo );
  if ( ulRes != FILERES_OK )
    return ulRes;

  bRes = LogOpen( &psData->sLog );

  return bRes ? FILERES_OK : FILERES_NOT_FOUND;
}

static VOID _LogClose(PVOID pData)
{
  PDATARESLOG	psData = (PDATARESLOG)pData;

  LogClose( &psData->sLog );
}

static ULONG _LogRead(PVOID pData, PCHAR pcBuf, ULONG ulBufLen, PULONG pulActual)
{
  PDATARESLOG	psData = (PDATARESLOG)pData;

  *pulActual = LogRead( &psData->sLog, pcBuf, ulBufLen );
  return *pulActual == ulBufLen ? FILERES_OK : FILERES_NO_MORE_DATA;
}

static VOID _LogGetFields(PVOID pData, RESPCODE rcRespCode, PREQFIELDS psRespFields)
{
  ReqFldSet( psRespFields, "Content-Type", "text/plain" );
}


/********************************************************************/
/*                                                                  */
/*                      "M3U" object's methods                      */
/*                                                                  */
/********************************************************************/

static ULONG _M3UOpen(PVOID pData, PFILEHOST psFileHost, PSZ pszPath,
                      PSZ pszParam, PSZ pszMethod, PREQFIELDS psFields, PCONNINFO psConnInfo)
{
  PDATAM3U	psData = (PDATAM3U)pData;
  PSZ		pszName;
  PSZ		pszPtr;
  CHAR		acBuf[16];
  ULONG		ulLength;
  ULONG		ulRC;
  ULONG		ulActual;
  PSOURCE	psSource;

  pszName = ReqFldGet( psFields, "Host" );
  if ( pszName != NULL )
  {
    pszPtr = strchr( pszName, ':' );
    ulLength = pszPtr != NULL ? pszPtr - pszName : strlen( pszName );
  }
  else if ( FileHostGetNamesCount(psFileHost) > 0 )
  {
    pszName = FileHostGetName( psFileHost, 0 );
    ulLength = strlen( pszName );
  }
  else
    return FILERES_NOT_FOUND;

  ulRC = FileResMemOpen( &psData->psFileRes, 320, "audio/x-mpegurl" );
  if ( ulRC != FILERES_OK )
    return FILERES_INTERNAL_ERROR;

  FileResWrite( psData->psFileRes, "http://", 7, &ulActual );
  FileResWrite( psData->psFileRes, pszName, ulLength, &ulActual );

  ulLength = sprintf( &acBuf, ":%u/", psConnInfo->usServPort );
  ulRC = FileResWrite( psData->psFileRes, &acBuf, ulLength, &ulActual );

  psSource = SourceQuery( pszPath );
  if ( psSource != NULL )
  {
    pszPtr = SourceGetMountPoint( psSource );
    ulRC = FileResWrite( psData->psFileRes, pszPtr,
                         strlen( pszPtr ), &ulActual );
    SourceRelease( psSource );
  }
  ulRC = FileResWrite( psData->psFileRes, "\r\n", 2, &ulActual );

  if ( ulRC != FILERES_OK )
    FileResClose( psData->psFileRes );
  else
    FileResSeek( psData->psFileRes, 0 );

  return ulRC;
}

static VOID _M3UClose(PVOID pData)
{
  PDATAM3U	psData = (PDATAM3U)pData;

  FileResClose( psData->psFileRes );
}

static ULONG _M3URead(PVOID pData, PCHAR pcBuf, ULONG ulBufLen, PULONG pulActual)
{
  PDATAM3U	psData = (PDATAM3U)pData;
  ULONG		ulRC;
 
  ulRC = FileResRead( psData->psFileRes, pcBuf, ulBufLen, pulActual );

  return ulRC;
}

static VOID _M3UGetFields(PVOID pData, RESPCODE rcRespCode, PREQFIELDS psRespFields)
{
  PDATAM3U	psData = (PDATAM3U)pData;

  FileResGetFields( psData->psFileRes, rcRespCode, psRespFields );
}


/********************************************************************/
/*                                                                  */
/*                      "PLS" object's methods                      */
/*                                                                  */
/********************************************************************/

static ULONG _PLSOpen(PVOID pData, PFILEHOST psFileHost, PSZ pszPath,
                      PSZ pszParam, PSZ pszMethod, PREQFIELDS psFields, PCONNINFO psConnInfo)
{
  PDATAPLS	psData = (PDATAPLS)pData;
  PSZ		pszName;
  ULONG		ulNameLength;
  PSZ		pszPtr;
  CHAR		acBuf[64];
  ULONG		ulLength;
  ULONG		ulRC;
  ULONG		ulActual;
  PSOURCE	psSource;
  ULONG		ulCount;

  pszName = ReqFldGet( psFields, "Host" );
  if ( pszName != NULL )
  {
    pszPtr = strchr( pszName, ':' );
    ulNameLength = pszPtr != NULL ? pszPtr - pszName : strlen( pszName );
  }
  else if ( FileHostGetNamesCount(psFileHost) > 0 )
  {
    pszName = FileHostGetName( psFileHost, 0 );
    ulNameLength = strlen( pszName );
  }
  else
    return FILERES_NOT_FOUND;

  ulLength = 35;
  psSource = SourceQueryList();
  for( ulCount = 0; psSource != NULL; psSource = SourceGetNext( psSource ) )
    if ( !SourceGetHidden( psSource ) )
    {
      ulCount++;
      ulLength += 23 + ulNameLength + strlen( SourceGetMountPoint( psSource ) );
    }
  SourceReleaseList();

  ulRC = FileResMemOpen( &psData->psFileRes, ulLength, "audio/x-scpls" );
  if ( ulRC != FILERES_OK )
    return FILERES_INTERNAL_ERROR;

  ulLength = sprintf( &acBuf, "[playlist]\r\nNumberOfEntries=%u\r\n", ulCount );
  FileResWrite( psData->psFileRes, &acBuf, ulLength, &ulActual );
  psSource = SourceQueryList();
  for( ulCount = 1; psSource != NULL; psSource = SourceGetNext( psSource ) )
  {
    if ( SourceGetHidden( psSource ) )
      continue;

    ulLength = sprintf( &acBuf, "File%u=http://", ulCount++ );
    FileResWrite( psData->psFileRes, &acBuf, ulLength, &ulActual );
    FileResWrite( psData->psFileRes, pszName, ulNameLength, &ulActual );
    ulLength = sprintf( &acBuf, ":%u/%s\r\n", psConnInfo->usServPort,
                                          SourceGetMountPoint( psSource ) );
    ulRC = FileResWrite( psData->psFileRes, &acBuf, ulLength, &ulActual );
    if ( ulRC != FILERES_OK )
      break;
  }
  SourceReleaseList();

  if ( ulRC != FILERES_OK )
    FileResClose( psData->psFileRes );
  else
    FileResSeek( psData->psFileRes, 0 );

  return ulRC;
}

static VOID _PLSClose(PVOID pData)
{
  PDATAM3U	psData = (PDATAM3U)pData;

  FileResClose( psData->psFileRes );
}

static ULONG _PLSRead(PVOID pData, PCHAR pcBuf, ULONG ulBufLen, PULONG pulActual)
{
  PDATAM3U	psData = (PDATAM3U)pData;
  ULONG		ulRC;
 
  ulRC = FileResRead( psData->psFileRes, pcBuf, ulBufLen, pulActual );

  return ulRC;
}

static VOID _PLSGetFields(PVOID pData, RESPCODE rcRespCode, PREQFIELDS psRespFields)
{
  PDATAM3U	psData = (PDATAM3U)pData;

  ReqFldSet( psRespFields, "Cache-Control",
             "no-store, no-cache, must-revalidate" );
  ReqFldSetCurrentDate( psRespFields, "Expires" );
  FileResGetFields( psData->psFileRes, rcRespCode, psRespFields );
}


/********************************************************************/
/*                                                                  */
/*                         External routines                        */
/*                                                                  */
/********************************************************************/

VOID SsFileResInit()
{
  // set new selector
  PrevGetMethods = FileResMethodsSelector( _GetMethods );
}

VOID SsFileResDone()
{
  // set old selector
  FileResMethodsSelector( PrevGetMethods );
}
