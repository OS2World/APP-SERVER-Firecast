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
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <sys\timeb.h> 
#include <io.h>
#include <ctype.h>
#include "httpserv/netserv.h"
#include "httpserv/requests.h"
#include "httpserv/netacl.h"
#include "httpserv/filehost.h"
#include "shoutserv.h"
#include "templates.h"
#include "clients.h"
#include "relays.h"
#include "control.h"
#include "ssmessages.h"
#include "logmsg.h"

#define MSGTYPE_ERROR		0
#define MSGTYPE_STOP		1
#define MSGTYPE_INFORMATION	2
#define MSGTYPE_WARNING		3

#define NET_MODULE	SS_MODULE_CONTROL

typedef struct _MESSAGE {
  ULONG		ulType;
  PSZ		pszText;
} MESSAGE, *PMESSAGE;

static PSZ	apszMessageTypes[] =
  { "ERROR", "STOP", "INFORMATION", "WARNING" };
static MESSAGE	asMessages[] =
{
  { MSGTYPE_ERROR, "Invalid request" },			// 0, MSGID_INVALID_REQUEST
  { MSGTYPE_STOP, "The source does not exist" },	// 1, MSGID_SOURCE_NOT_EXISTS
  { MSGTYPE_INFORMATION, "The source is disconnected" },// 2, MSGID_SOURCE_STOPPED
  { MSGTYPE_STOP, "The client does not exist" },	// 3, MSGID_CLIENT_NOT_EXISTS
  { MSGTYPE_INFORMATION, "The client is disconnected" },// 4, MSGID_CLIENT_STOPPED
  { MSGTYPE_ERROR, "Cannot parce data" },		// 5, MSGID_XML_PARCE_ERROR
  { MSGTYPE_INFORMATION, "Configuration is updated" },	// 6, MSGID_CFG_UPDATED
  { MSGTYPE_STOP, "The relay does not exist" },		// 7, MSGID_RELAY_NOT_EXISTS
  { MSGTYPE_INFORMATION, "The relay was removed" },	// 8, MSGID_RELAY_REMOVED
  { MSGTYPE_WARNING, "Force relay reconnect" },		// 9, MSGID_RELAY_RESET
  { MSGTYPE_STOP, "Don't need reconnect" }		// 10, MSGID_RELAY_NO_RESET
};

static CHAR	acAdminName[MAX_ADMIN_NAME_LENGTH+1] = {"admin"};
static CHAR	acAdminPassword[MAX_ADMIN_PASSWORD_LENGTH+1] = {"password"};
static NETACL	sAdminAcl;
static PSZ	pszConfigFile = NULL;

static xmlNodePtr _NewTextNode(xmlDocPtr xmlDoc, xmlNodePtr xmlNode,
                               PSZ pszName, PSZ pszValue)
{
  xmlChar	*pxcEncStr;
  xmlNodePtr	xmlNewNode;

  pxcEncStr = xmlEncodeEntitiesReentrant( xmlDoc, pszValue );
  if ( pxcEncStr != NULL )
  {
    xmlNewNode = xmlNewChild( xmlNode, NULL, pszName, pxcEncStr );
    xmlFree( pxcEncStr );
    if ( xmlNewNode != NULL )
      return xmlNewNode;
  }

  return NULL;
}

static xmlNodePtr _NewRootNode(PSZ pszName)
{
  xmlNodePtr	xmlNode;

  xmlNode = xmlNewNode( NULL, pszName );
  if ( xmlNode == NULL )
  {
    ERROR_NOT_ENOUGHT_MEM();
    return NULL;
  }

  return xmlNode;
}

static xmlDocPtr _NewDoc()
{
  xmlDocPtr	xmlDoc;

  xmlDoc = xmlNewDoc( "1.0" );
  if ( xmlDoc == NULL )
  {
    ERROR_NOT_ENOUGHT_MEM();
    return NULL;
  }

  return xmlDoc;
}

static PCHAR _NodeContent(xmlNodePtr xmlNode)
{
  xmlChar	*pxcContent;

  if ( xmlNode->xmlChildrenNode == NULL ||
       xmlNode->xmlChildrenNode->content == NULL )
  {
    return NULL;
  }

  pxcContent = xmlNode->xmlChildrenNode->content;
  while( isblank(*pxcContent) )
    pxcContent++;

  return *pxcContent == '\0' ? NULL : pxcContent;
}

static BOOL _DocToText(xmlDocPtr xmlDoc, xmlNodePtr xmlNode, PSZ pszXSLT,
                       PCHAR *ppcText, PULONG pulSize)
{
  CHAR		acBuf[128];

  xmlDocSetRootElement( xmlDoc, xmlNode );

  if ( pszXSLT != NULL )
  {
    sprintf( &acBuf, "type=\"text/xsl\" href=\"%s.xsl\"", pszXSLT );
    xmlAddPrevSibling( xmlNode, xmlNewDocPI(xmlDoc, "xml-stylesheet", &acBuf) );
  }

  xmlDocDumpMemoryEnc( xmlDoc, (xmlChar **)ppcText, (int *)pulSize, "UTF-8" );
  xmlFreeDoc( xmlDoc );

  return *ppcText != NULL;
}

static VOID _GetGMTOffset(PCHAR pcGMTLocalDiffStr)
{
  struct timeb	time_b;
  LONG		lGMTOffs;
  LONG		lGMTOffsH;
  LONG		lGMTOffsM;

  ftime( &time_b ); 

  lGMTOffs  = time_b.timezone;
  lGMTOffsH = lGMTOffs / 60;
  lGMTOffsM = lGMTOffs % 60;

  sprintf( pcGMTLocalDiffStr, " %c%0.2d%0.2d", 
    (lGMTOffs<=0 ? '+' : '-'), abs(lGMTOffsH), abs(lGMTOffsM) );
}

static VOID _StoreSourceFields(xmlDocPtr xmlDoc, xmlNodePtr xmlNodeSection,
                               PREQFIELDS psReqFields)
{
  PREQFIELD	psReqField;
  CHAR		acBuf[128];
  ULONG		ulLength;
  xmlNodePtr	xmlNode;

  psReqField = ReqFldGetFirst( psReqFields );
  while( psReqField != NULL )
  {
    ulLength = min( sizeof(acBuf)-1, psReqField->ulNameLen );
    memcpy( &acBuf, &psReqField->acData, ulLength );
    acBuf[ulLength] = '\0';

    xmlNode = _NewTextNode( xmlDoc, xmlNodeSection, "field",
                           ReqFldGetValue(psReqField) );
    xmlNewProp( xmlNode, "name", &acBuf );

    psReqField = ReqFldGetNext( psReqField );
  }
}

static VOID _StoreSourceMeta(xmlDocPtr xmlDoc, xmlNodePtr xmlNodeSection,
                             PSOURCE psSource,
                             ULONG ulCount, PSZ pszGMTLocalDiffStr)
{
  xmlNodePtr	xmlNode;
  xmlNodePtr	xmlNode1;
  BOOL		bRes;
  ULONG		ulIdx;
  CHAR		acBufTime[50];
  PSZ		pszMetaData;

  xmlNode = xmlNewChild( xmlNodeSection, NULL, "meta-history", NULL );
  for( ulIdx = 0; ulIdx < min(METADATA_HISTORY_ITEMS,ulCount); ulIdx++ )
  {
    bRes = SourceGetMetaHistoryItem( psSource, ulIdx, &acBufTime,
                                     sizeof(acBufTime)-7, &pszMetaData );
    if ( bRes )
    {
      xmlNode1 = _NewTextNode( xmlDoc, xmlNode, "metadata", pszMetaData );
      strcpy( strchr( &acBufTime, '\0' ), pszGMTLocalDiffStr );
      xmlNewProp( xmlNode1, "time", &acBufTime );
    }
  }
}

static VOID _StoreSourceClients(xmlDocPtr xmlDoc, xmlNodePtr xmlNodeSection,
                                PSOURCE psSource)
{
  xmlNodePtr	xmlNode;
  xmlNodePtr	xmlNode1;
  PCLIENT	psClient;
  CHAR		acBuf[24];

  if ( SourceGetClientsCount(psSource) == 0 )
    return;

  xmlNode = xmlNewChild( xmlNodeSection, NULL, "clients", NULL );

  psClient = SourceQueryClientsList( psSource );
  while( psClient != NULL )
  {
    xmlNode1 = xmlNewChild( xmlNode, NULL, "client", NULL );
    ultoa( ClientGetId( psClient ), &acBuf, 10 );
    xmlNewProp( xmlNode1, "id", &acBuf );

    ClientGetAddr( psClient, &acBuf, sizeof(acBuf) );
    xmlNewChild( xmlNode1, NULL, "addr", &acBuf );
    _NewTextNode( xmlDoc, xmlNode1, "user-agent", ClientGetUserAgent(psClient) );

    psClient = ClientNext( psClient );
  }
  SourceReleaseClientsList( psSource );
}

static VOID _StoreSource(xmlDocPtr xmlDoc, xmlNodePtr xmlNodeSection,
                         PSOURCE psSource,
                         PSZ pszGMTLocalDiffStr, ULONG ulFlags)
{
  CHAR		acBuf[32];

  ultoa( SourceGetId( psSource ), &acBuf, 10 );
  xmlNewProp( xmlNodeSection, "id", &acBuf );

  if ( (ulFlags & FLSRC_TEMPLATE_ID) != 0 )
  {
    ultoa( SourceGetTemplateId( psSource ), &acBuf, 10 );
    xmlNewChild( xmlNodeSection, NULL, "template-id", &acBuf );
  }

  _NewTextNode( xmlDoc, xmlNodeSection, "mount-point",
               SourceGetMountPoint(psSource) );

  if ( (ulFlags & FLSRC_SRCADDR) != 0 )
  {
    _NewTextNode( xmlDoc, xmlNodeSection, "source-address",
                  SourceGetSourceAddr(psSource,&acBuf) );
  }

  _StoreSourceFields( xmlDoc, xmlNodeSection, SourceGetFields( psSource ) );
  ultoa( SourceGetClientsCount( psSource ), &acBuf, 10 );
  xmlNewChild( xmlNodeSection, NULL, "clients-count", &acBuf );

  _StoreSourceMeta( xmlDoc, xmlNodeSection, psSource, 
                    (ulFlags & FLSRC_META_HISTORY) ? METADATA_HISTORY_ITEMS : 1,
                    pszGMTLocalDiffStr );
  if ( (ulFlags & FLSRC_CLIENTS) != 0 )
    _StoreSourceClients( xmlDoc, xmlNodeSection, psSource );
}

static VOID _StoreTemplFields(xmlDocPtr xmlDoc, xmlNodePtr xmlNodeSection,
                              PREQFIELDS psReqFields, BOOL bOverride)
{
  PREQFIELD	psReqField;
  CHAR		acBuf[128];
  ULONG		ulLength;
  xmlNodePtr	xmlNode;

  psReqField = ReqFldGetFirst( psReqFields );
  while( psReqField != NULL )
  {
    ulLength = min( sizeof(acBuf)-1, psReqField->ulNameLen );
    memcpy( &acBuf, &psReqField->acData, ulLength );
    acBuf[ulLength] = '\0';

    xmlNode = _NewTextNode( xmlDoc, xmlNodeSection, "field",
                            ReqFldGetValue(psReqField) );
    xmlNewProp( xmlNode, "name", &acBuf );
    xmlNewProp( xmlNode, "override", bOverride ? "1" : "0" );

    psReqField = ReqFldGetNext( psReqField );
  }
}

static VOID _StoreAcl(xmlDocPtr xmlDoc, xmlNodePtr xmlNodeSection,
                      PNETACL psNetAcl)
{
  ULONG		ulIdx;
  BOOL		bAllow;
  CHAR		acBuf[64];
  xmlNodePtr	xmlNode;
  xmlNodePtr 	xmlNodeAccess;
  ULONG		ulCount = NetAclGetCount( psNetAcl );

  if ( ulCount == 0 )
    return;

  xmlNodeAccess = xmlNewChild( xmlNodeSection, NULL, "access", NULL );
  for( ulIdx = 0; ulIdx < ulCount; ulIdx++ )
  {
    bAllow = NetAclGetItem( psNetAcl, ulIdx, &acBuf );

    xmlNode = xmlNewChild( xmlNodeAccess, NULL, "acl", &acBuf );
    xmlNewProp( xmlNode, "action", bAllow ? "allow" : "deny" );
  }
}

static VOID _StoreAdmin(xmlDocPtr xmlDoc, xmlNodePtr xmlNodeSection)
{
  CHAR		acBuf[32];

  _NewTextNode( xmlDoc, xmlNodeSection, "name", &acAdminName);
  _NewTextNode( xmlDoc, xmlNodeSection, "password", &acAdminPassword);
  _NewTextNode( xmlDoc, xmlNodeSection, "log-level", 
                ultoa( LogGetLevel(), &acBuf, 10 ) );
  _NewTextNode( xmlDoc, xmlNodeSection, "log-buffer", 
                ultoa( LogGetBuffSize(), &acBuf, 10 ) );
  _StoreAcl( xmlDoc, xmlNodeSection, &sAdminAcl );
}

static VOID _StoreTemplates(xmlDocPtr xmlDoc, xmlNodePtr xmlNodeSection)
{
  xmlNodePtr	xmlNodeTemplate;
  xmlNodePtr	xmlNode;
  PTEMPLATE	psTemplate;
  ULONG		ulMaxClients;
  CHAR		acBuf[32];

  psTemplate = TemplQueryList();
  while( psTemplate != NULL )
  {
    xmlNodeTemplate = xmlNewChild( xmlNodeSection, NULL, "template", NULL );
    xmlNewProp( xmlNodeTemplate, "denided", 
                TemplGetDenied(psTemplate) ? "1" : "0" );
    xmlNewProp( xmlNodeTemplate, "hidden", 
                TemplGetHidden(psTemplate) ? "1" : "0" );

    _NewTextNode( xmlDoc, xmlNodeTemplate, "mount-point",
                 TemplGetMountPointWC(psTemplate) );

    _StoreAcl( xmlDoc, xmlNodeTemplate, TemplGetAclSources(psTemplate) );

    _NewTextNode( xmlDoc, xmlNodeTemplate, "password",
                 TemplGetPassword(psTemplate) );

    ultoa( TemplGetKeepAlive(psTemplate), &acBuf, 10 );
    _NewTextNode( xmlDoc, xmlNodeTemplate, "keep-alive", acBuf );

    _StoreTemplFields( xmlDoc, xmlNodeTemplate,
                       TemplGetDefaultFields(psTemplate), FALSE );
    _StoreTemplFields( xmlDoc, xmlNodeTemplate,
                       TemplGetOverrideFields(psTemplate), TRUE );

    xmlNode = xmlNewChild( xmlNodeTemplate, NULL, "clients", NULL );
    if ( (ulMaxClients = TemplGetMaxClients( psTemplate )) != 0 )
    {
      xmlNewChild( xmlNode, NULL, "max-clients",
                   ultoa( ulMaxClients, &acBuf, 10 ) );
    }
    _StoreAcl( xmlDoc, xmlNode, TemplGetAclClients(psTemplate) );

    psTemplate = TemplGetNext( psTemplate );
  }
  TemplReleaseList();
}

static VOID _StoreRelays(xmlDocPtr xmlDoc, xmlNodePtr xmlNodeSection, BOOL bExtended)
{
  xmlNodePtr	xmlNodeRelay;
  xmlNodePtr	xmlNode;
  PRELAY	psRelay;
  CHAR		acBuf[24];
  PSZ		pszText;

  psRelay = RelayQueryList();
  while( psRelay != NULL )
  {
    xmlNodeRelay = xmlNewChild( xmlNodeSection, NULL, "relay", NULL );

    if ( bExtended )
    {
      ultoa( RelayGetId( psRelay ), &acBuf, 10 );
      xmlNewProp( xmlNodeRelay, "id", &acBuf );
    }

    xmlNewChild( xmlNodeRelay, NULL, "nailed-up",
                 RelayGetNailedUp(psRelay) ? "1" : "0" );

    _NewTextNode( xmlDoc, xmlNodeRelay, "location",
                  RelayGetLocation(psRelay) );
    _NewTextNode( xmlDoc, xmlNodeRelay, "mount-point",
                  RelayGetMountPoint(psRelay) );

    if ( bExtended )
    {
      switch( RelayGetState( psRelay ) )
      {
        case RELAY_STATE_IDLE:
          pszText = "idle";
          break;
        case RELAY_STATE_CONNECT:
          pszText = "connecting...";
          break;
        case RELAY_STATE_WAIT_CLIENTS:
          pszText = "wait for listeners";
          break;
        case RELAY_STATE_URL_RECEIVED:
          pszText = "M3U file received";
          break;
        case RELAY_STATE_TRANSMIT:
          pszText = "on-line";
          break;
        case RELAY_STATE_NO_TEMPLATE:
          pszText = "template not found";
          break;
        case RELAY_STATE_LOCATION_FAILED:
          pszText = "source failed";
          break;
        case RELAY_STATE_TRANSMIT_ERROR:
          pszText = "transmit error";
          break;
        case RELAY_STATE_INVALID_MPOINT:
          pszText = "invalid mount point";
          break;
        case RELAY_STATE_MPOINT_EXIST:
          pszText = "the mount point already in use";
          break;
        case RELAY_STATE_INVALID_FORMAT:
          pszText = "unknown data format";
          break;
        case RELAY_STATE_ERROR_CREATE_MPOINT:
          pszText = "cannot create mount point";
          break;
        case RELAY_STATE_DISCONNECTED:
          pszText = "disconnected";
          break;
        case RELAY_STATE_NAME_NOT_FOUND:
          pszText = "host name not found";
          break;
        case RELAY_STATE_CONNECTION_CLOSED:
          pszText = "connection closed by peer";
          break;
        case RELAY_STATE_CONNECTION_REFUSED:
          pszText = "connection refused";
          break;
        case RELAY_STATE_ADDRESS_UNREACH:
          pszText = "address unreach";
          break;
        case RELAY_STATE_CANNOT_CONNECT:
          pszText = "cannot connect to host";
          break;
        case RELAY_STATE_INVALID_RESPONSE:
          pszText = "invalid HTTP response";
          break;
        case RELAY_STATE_HTTP_NOT_FOUND:
          pszText = "HTTP status code 404 (Not Found)";
          break;
        default:
          pszText = NULL;
      }

      xmlNode = _NewTextNode( xmlDoc, xmlNodeRelay, "state", pszText );

      ultoa( RelayGetState( psRelay ), &acBuf, 10 );
      xmlNewProp( xmlNode, "code", &acBuf );
      xmlNewProp( xmlNode, "error",
        (RelayGetState( psRelay ) & RELAY_STATE_ERROR_MASK) != 0 ? "1" : "0" );

    }

    psRelay = TemplGetNext( psRelay );
  }
  RelayReleaseList();
}

static VOID _StoreHosts(xmlDocPtr xmlDoc, xmlNodePtr xmlNodeSection)
{
  PFILEHOST	psFileHost;
  xmlNodePtr	xmlNodeHost;
  ULONG		ulIdx;
  PSZ		pszBasePath;

  psFileHost = FileHostQueryList();
  while( psFileHost != NULL )
  {
    xmlNodeHost = xmlNewChild( xmlNodeSection, NULL, "host", NULL );

    pszBasePath = FileHostGetBasePath(psFileHost); 
    if ( pszBasePath != NULL )
    {
      _NewTextNode( xmlDoc, xmlNodeHost, "path", pszBasePath );
    }

    for( ulIdx = 0; ulIdx < FileHostGetNamesCount(psFileHost); ulIdx++ )
    {
      _NewTextNode( xmlDoc, xmlNodeHost, "name", 
                   FileHostGetName(psFileHost,ulIdx) );
    }

    for( ulIdx = 0; ulIdx < FileHostGetIndexFilesCount(psFileHost); ulIdx++ )
    {
      _NewTextNode( xmlDoc, xmlNodeHost, "index", 
                   FileHostGetIndexFile(psFileHost,ulIdx) );
    }

    _StoreAcl( xmlDoc, xmlNodeHost, FileHostGetAcl(psFileHost) );

    psFileHost = FileHostGetNext( psFileHost );
  }
  FileHostReleaseList();
}

static VOID _StoreGeneral(xmlDocPtr xmlDoc, xmlNodePtr xmlNodeSection)
{
  ULONG		ulCount;
  ULONG		ulIdx;
  PSZ		pszAddr;
  USHORT	usPort;
  xmlNodePtr	xmlNode;
  CHAR		acBuf[128];
  PNETSERV	psHTTPNetServ = ReqGetNetServ();


  ulCount = NetServQueryBindList( psHTTPNetServ );
  for( ulIdx = 0; ulIdx < ulCount; ulIdx++ )
  {
    NetServGetBind( psHTTPNetServ, ulIdx, &pszAddr, &usPort );

    if ( ShoutServGetMountPoint( usPort, NULL, 0 ) )
      continue;

    xmlNode = xmlNewChild( xmlNodeSection, NULL, "bind", NULL );
    if ( pszAddr != NULL )
      xmlNewChild( xmlNode, NULL, "interface", pszAddr );
    xmlNewChild( xmlNode, NULL, "port", ultoa( usPort, &acBuf, 10 ) );

    if ( ShoutServGetMountPoint( usPort+1, &acBuf, sizeof(acBuf)-1 ) )
    {
      acBuf[sizeof(acBuf)-1] = '\0';
      xmlNewChild( xmlNode, NULL, "shout-mp", &acBuf );
    }
  }
  NetServReleaseBindList( psHTTPNetServ );
}


static VOID _ParseXMLNodeGeneralBind(xmlNodePtr xmlNodeSection,
                                     PBINDLIST psBindList,
                                     PSHOUTMOUNTPOINTSLIST psShoutMPList)
{
  xmlNodePtr	xmlNode;
  xmlChar	*pxcContent;
  xmlChar	*pxcInterface = NULL;
  xmlChar	*pxcShoutMP = NULL;
  LONG		lPort = 0;
  ULONG		ulRC;

  for( xmlNode = xmlNodeSection->children; xmlNode; xmlNode = xmlNode->next )
  {
    if ( xmlIsBlankNode( xmlNode ) && xmlNode->xmlChildrenNode == NULL )
      continue;

    pxcContent = _NodeContent( xmlNode );

    if ( xmlStrcmp( xmlNode->name, "interface" ) == 0 )
      pxcInterface = pxcContent;
    else if ( xmlStrcmp( xmlNode->name, "port" ) == 0 )
      lPort = atol( pxcContent );
    else if ( xmlStrcmp( xmlNode->name, "shout-mp" ) == 0 )
      pxcShoutMP = pxcContent;
  }

  if ( lPort <= 0 || lPort > 0xFFFE )
    return;

  ulRC = NetServBindListAdd( psBindList, pxcInterface, lPort );
  if ( ulRC == RET_OK && pxcShoutMP != NULL )
  {
    lPort++;
    ulRC = NetServBindListAdd( psBindList, pxcInterface, lPort );
    if ( ulRC == RET_OK )
      ShoutServMountPointsListAdd( psShoutMPList, lPort, pxcShoutMP );
  }
}

static VOID _ApplyXMLNodeAdmin(xmlNodePtr xmlNodeSection)
{
  xmlNodePtr	xmlNode;
  xmlChar	*pxcContent;

  NetAclDone( &sAdminAcl );
  NetAclInit( &sAdminAcl );
  for( xmlNode = xmlNodeSection->children; xmlNode; xmlNode = xmlNode->next )
  {
    if ( xmlIsBlankNode( xmlNode ) )
      continue;

    pxcContent = _NodeContent( xmlNode );

    if ( xmlStrcmp( xmlNode->name, "name" ) == 0 )
    {
      if ( pxcContent != NULL )
      {
        strncpy( &acAdminName, pxcContent, MAX_ADMIN_NAME_LENGTH );
        acAdminName[MAX_ADMIN_NAME_LENGTH] = '\0';
      }
      else
        acAdminName[0] = '\0';
    }
    else if ( xmlStrcmp( xmlNode->name, "password" ) == 0 )
    {
      if ( pxcContent != NULL )
      {
        strncpy( &acAdminPassword, pxcContent, MAX_ADMIN_PASSWORD_LENGTH );
        acAdminPassword[MAX_ADMIN_PASSWORD_LENGTH] = '\0';
      }
      else
        acAdminPassword[0] = '\0';
    }
    else if ( xmlStrcmp( xmlNode->name, "access" ) == 0 )
      _ParseXMLNodeAccess( xmlNode, &sAdminAcl );
    else if ( xmlStrcmp( xmlNode->name, "log-level" ) == 0 )
    {
      if ( pxcContent != NULL )
        LogSetLevel( atol(pxcContent) );
    }
    else if ( xmlStrcmp( xmlNode->name, "log-buffer" ) == 0 )
    {
      if ( pxcContent != NULL )
        LogSetBuff( atol(pxcContent) );
    }
  }
}

static VOID _ApplyXMLNodeGeneral(xmlNodePtr xmlNodeSection)
{
  xmlNodePtr		xmlNode;
  xmlNodePtr		xmlNode1;
  BINDLIST		sBindList;
  SHOUTMOUNTPOINTSLIST	sShoutMPList;

  NetServBindListInit( &sBindList );
  ShoutServMountPointsListInit( &sShoutMPList );

  for( xmlNode = xmlNodeSection->children; xmlNode; xmlNode = xmlNode->next )
  {
    if ( xmlIsBlankNode( xmlNode ) )
      continue;

    if ( xmlStrcmp( xmlNode->name, "bind" ) == 0 )
      _ParseXMLNodeGeneralBind( xmlNode, &sBindList, &sShoutMPList );
  }

  NetServBindListApply( ReqGetNetServ(), &sBindList );
  ShoutServMountPointsListApply( &sShoutMPList );
}

static VOID _ParseXMLNodeAccess(xmlNodePtr xmlNodeSection, PNETACL psNetAcl)
{
  xmlNodePtr	xmlNode;
  xmlChar	*pxcContent;
  xmlChar	*pxcAction;
  BOOL		bAllow;

  for( xmlNode = xmlNodeSection->children; xmlNode; xmlNode = xmlNode->next )
  {
    if ( xmlIsBlankNode( xmlNode ) )
      continue;

    pxcContent = _NodeContent( xmlNode );

    if ( xmlStrcmp( xmlNode->name, "acl" ) == 0 )
    {
      if ( (pxcAction = xmlGetNoNsProp( xmlNode, "action" )) != NULL )
      {
        bAllow = xmlStrcmp( pxcAction, "allow" ) == 0;
        NetAclAdd( psNetAcl, pxcContent, bAllow );
      }
    }
  }
}

static VOID _ParseXMLNodeHostsHost(xmlNodePtr xmlNodeSection,
                                   PFILEHOST psFileHost)
{
  xmlNodePtr	xmlNode;
  xmlChar	*pxcContent;

  for( xmlNode = xmlNodeSection->children; xmlNode; xmlNode = xmlNode->next )
  {
    if ( xmlIsBlankNode( xmlNode ) )
      continue;

    pxcContent = _NodeContent( xmlNode );

    if ( xmlStrcmp( xmlNode->name, "path" ) == 0 )
      FileHostSetBasePath( psFileHost, pxcContent );
    else if ( xmlStrcmp( xmlNode->name, "name" ) == 0 )
      FileHostAddName( psFileHost, pxcContent );
    else if ( xmlStrcmp( xmlNode->name, "index" ) == 0 )
      FileHostAddIndexFile( psFileHost, pxcContent );
    else if ( xmlStrcmp( xmlNode->name, "access" ) == 0 )
      _ParseXMLNodeAccess( xmlNode, FileHostGetAcl(psFileHost) );
  }
}

static VOID _ApplyXMLNodeHosts(xmlNodePtr xmlNodeSection)
{
  xmlNodePtr	xmlNode;
  PFILEHOST	psFileHost;

  FileHostRemoveAll();

  for( xmlNode = xmlNodeSection->children; xmlNode; xmlNode = xmlNode->next )
  {
    if ( xmlIsBlankNode( xmlNode ) )
      continue;

    if ( xmlStrcmp( xmlNode->name, "host" ) == 0 )
    {
      if ( (psFileHost = FileHostNew()) != NULL )
      {
        _ParseXMLNodeHostsHost( xmlNode, psFileHost );
        FileHostReady( psFileHost );
      }
    }
  }
}

static VOID _ParseXMLNodeSourcesTemplateClients(xmlNodePtr xmlNodeSection,
                                                PTEMPLATE psTemplate)
{
  xmlNodePtr	xmlNode;
  xmlChar	*pxcContent;

  for( xmlNode = xmlNodeSection->children; xmlNode; xmlNode = xmlNode->next )
  {
    if ( xmlIsBlankNode( xmlNode ) )
      continue;

    pxcContent = _NodeContent( xmlNode );

    if ( xmlStrcmp( xmlNode->name, "max-clients" ) == 0 )
    {
      if ( pxcContent != NULL )
        TemplSetMaxClients( psTemplate, atol(pxcContent) );
    }
    else if ( xmlStrcmp( xmlNode->name, "access" ) == 0 )
      _ParseXMLNodeAccess( xmlNode, TemplGetAclClients(psTemplate) );
  }
}

static VOID _ParseXMLNodeSourcesTemplate(xmlNodePtr xmlNodeSection,
                                         PTEMPLATE psTemplate)
{
  xmlNodePtr	xmlNode;
  xmlChar	*pxcContent;
  xmlChar	*pxcPropValue;
  BOOL		bOverride;

  for( xmlNode = xmlNodeSection->children; xmlNode; xmlNode = xmlNode->next )
  {
    if ( xmlIsBlankNode( xmlNode ) )
      continue;

    pxcContent = _NodeContent( xmlNode );

    if ( xmlStrcmp( xmlNode->name, "mount-point" ) == 0 )
      TemplSetMountPointWC( psTemplate, pxcContent );
    if ( xmlStrcmp( xmlNode->name, "keep-alive" ) == 0 )
    {
      if ( pxcContent != NULL )
        TemplSetKeepAlive( psTemplate, atol(pxcContent) );
    }
    else if ( xmlStrcmp( xmlNode->name, "access" ) == 0 )
      _ParseXMLNodeAccess( xmlNode, TemplGetAclSources(psTemplate) );
    else if ( xmlStrcmp( xmlNode->name, "password" ) == 0 )
      TemplSetPassword( psTemplate, pxcContent );
    else if ( xmlStrcmp( xmlNode->name, "field" ) == 0 )
    {
      pxcPropValue = xmlGetNoNsProp( xmlNode, "override" );
      bOverride = pxcPropValue != NULL && *pxcPropValue == '1';
      
      TemplAddField( psTemplate, xmlGetNoNsProp( xmlNode, "name" ),
                     pxcContent, bOverride );
    }
    else if ( xmlStrcmp( xmlNode->name, "clients" ) == 0 )
      _ParseXMLNodeSourcesTemplateClients( xmlNode, psTemplate );
  }
}

static VOID _ApplyXMLNodeTemplates(xmlNodePtr xmlNodeSection)
{
  xmlNodePtr	xmlNode;
  PFILEHOST	psFileHost;
  xmlChar	*pxcPropValue;
  BOOL		bDenided;
  BOOL		bHidden;
  PTEMPLATE	psTemplate;

  TemplRemoveAll();

  for( xmlNode = xmlNodeSection->children; xmlNode; xmlNode = xmlNode->next )
  {
    if ( xmlIsBlankNode( xmlNode ) )
      continue;

    if ( xmlStrcmp( xmlNode->name, "template" ) == 0 )
    {
      pxcPropValue = xmlGetNoNsProp( xmlNode, "denided" );
      bDenided = pxcPropValue != NULL && *pxcPropValue == '1';
      pxcPropValue = xmlGetNoNsProp( xmlNode, "hidden" );
      bHidden = pxcPropValue != NULL && *pxcPropValue == '1';

      psTemplate = TemplNew( bDenided );
      if ( psTemplate != NULL )
      {
        TemplSetHidden( psTemplate, bHidden );
        _ParseXMLNodeSourcesTemplate( xmlNode, psTemplate );
        TemplReady( psTemplate );
      }
    }
  }
}

static VOID _ParseXMLNodeRelay(xmlNodePtr xmlNodeSection, PRELAY psRelay)
{
  xmlNodePtr	xmlNode;
  xmlChar	*pxcContent;

  for( xmlNode = xmlNodeSection->children; xmlNode; xmlNode = xmlNode->next )
  {
    if ( xmlIsBlankNode( xmlNode ) )
      continue;

    pxcContent = _NodeContent( xmlNode );

    if ( xmlStrcmp( xmlNode->name, "nailed-up" ) == 0 )
      RelaySetNailedUp( psRelay, (pxcContent != NULL) && (*pxcContent == '1') );
    else if ( xmlStrcmp( xmlNode->name, "location" ) == 0 )
      RelaySetLocation( psRelay, pxcContent );
    else if ( xmlStrcmp( xmlNode->name, "mount-point" ) == 0 )
      RelaySetMountPoint( psRelay, pxcContent );
  }
}

static VOID _ApplyXMLNodeRelay(xmlNodePtr xmlNode)
{
  PRELAY	psRelay;

  psRelay = RelayNew();
  if ( psRelay != NULL )
  {
    _ParseXMLNodeRelay( xmlNode, psRelay );
    RelayReady( psRelay );
  }
}

static VOID _ApplyXMLNodeRelays(xmlNodePtr xmlNodeSection)
{
  xmlNodePtr	xmlNode;
  PRELAY	psRelay;

  RelayRemoveAll();

  for( xmlNode = xmlNodeSection->children; xmlNode; xmlNode = xmlNode->next )
  {
    if ( xmlIsBlankNode( xmlNode ) )
      continue;

    if ( xmlStrcmp( xmlNode->name, "relay" ) == 0 )
    {
      _ApplyXMLNodeRelay( xmlNode );
    }
  }
}

static VOID _ApplyXMLNode(xmlNodePtr xmlNodeSection)
{
  if ( xmlStrcmp( xmlNodeSection->name, "admin" ) == 0 )
  {
    _ApplyXMLNodeAdmin( xmlNodeSection );
  }
  else if ( xmlStrcmp( xmlNodeSection->name, "general" ) == 0 )
  {
    _ApplyXMLNodeGeneral( xmlNodeSection );
  }
  else if ( xmlStrcmp( xmlNodeSection->name, "hosts" ) == 0 )
  {
    _ApplyXMLNodeHosts( xmlNodeSection );
  }
  else if ( xmlStrcmp( xmlNodeSection->name, "templates" ) == 0 )
  {
    _ApplyXMLNodeTemplates( xmlNodeSection );
  }
  else if ( xmlStrcmp( xmlNodeSection->name, "relays" ) == 0 )
  {
    _ApplyXMLNodeRelays( xmlNodeSection );
  }
  else if ( xmlStrcmp( xmlNodeSection->name, "relay" ) == 0 )
  {
    _ApplyXMLNodeRelay( xmlNodeSection );
  }
}

static BOOL _ApplyXMLDoc(xmlDocPtr xmlDocConfig)
{
  xmlNodePtr	xmlNodeRoot = xmlDocGetRootElement( xmlDocConfig );
  xmlNodePtr	xmlNodeSection;

  if ( xmlNodeRoot == NULL ||
     ( xmlStrcmp( xmlNodeRoot->name, "server" ) != 0 ) )
    return FALSE;

  for( xmlNodeSection = xmlNodeRoot->children; xmlNodeSection;
       xmlNodeSection = xmlNodeSection->next )
  {
    if ( !xmlIsBlankNode( xmlNodeSection ) )
      _ApplyXMLNode( xmlNodeSection );
  }

  return TRUE;
}

static BOOL _LoadFile(PSZ pszFileName)
{
  xmlDocPtr	xmlDoc;
  BOOL		bRes;

  xmlDoc = xmlReadFile( pszFileName, "UTF-8",
                           XML_PARSE_NOERROR + XML_PARSE_NOWARNING );
  if ( xmlDoc == NULL )
    return FALSE;

  bRes = _ApplyXMLDoc( xmlDoc );

  xmlFreeDoc( xmlDoc );
  xmlCleanupParser();
  return bRes;
}

static BOOL _DefaultConfig()
{
  BINDLIST		sBindList;
  SHOUTMOUNTPOINTSLIST	sShoutMountPointsList;
  PNETSERV		psHTTPNetServ = ReqGetNetServ();
  PFILEHOST		psFileHost;
  ULONG			ulRC;
  BOOL			bRC;
  PTEMPLATE		psTemplate;

  LogSetLevel( NET_MSGTYPE_DETAIL );
  LogSetBuff( 8*1024 );

  NetServBindListInit( &sBindList );
  ShoutServMountPointsListInit( &sShoutMountPointsList );

  ulRC = NetServBindListAdd( &sBindList, NULL, 8000 );
  if ( ulRC == RET_OK &&
       NetServBindListAdd( &sBindList, NULL, 8001 ) == RET_OK )
  {
    ulRC = ShoutServMountPointsListAdd( &sShoutMountPointsList,
                                        8001, "shout" );
  }
  if ( ulRC != RET_OK )
  {
    BindListDestroy(&sBindList);
    return FALSE;
  }
  NetServBindListApply( psHTTPNetServ, &sBindList );
  ShoutServMountPointsListApply( &sShoutMountPointsList );

  FileHostRemoveAll();
  psFileHost = FileHostNew();
  if ( psFileHost == NULL )
    return FALSE;
  bRC = FileHostSetBasePath( psFileHost, ".\\host" );
  if ( !bRC )
  {
    FileHostDestroy( psFileHost );
    return FALSE;
  }
  FileHostAddIndexFile( psFileHost, "index.html" );
  FileHostAddIndexFile( psFileHost, "index.htm" );
  FileHostReady( psFileHost );

  TemplRemoveAll();
  psTemplate = TemplNew( FALSE );
  if ( psTemplate == NULL )
    return FALSE;
  TemplSetHidden( psTemplate, FALSE );
  TemplSetMountPointWC( psTemplate, "*" );
  TemplSetPassword( psTemplate, "password" );
  TemplSetMaxClients( psTemplate, 100 );
  TemplReady( psTemplate );

  return TRUE;
}


static VOID XMLCALL xml_silent_errfunc(VOID *ctx, const UCHAR *msg, ...) { }

VOID CtrlInit(PSZ pszFileName)
{
  BOOL	bRes;

  xmlSetGenericErrorFunc( NULL, xml_silent_errfunc );

  NetAclInit( &sAdminAcl );
  if ( pszFileName != NULL )
  {
    pszConfigFile = strdup( pszFileName );
    if ( !_LoadFile( pszConfigFile ) )
    {
      bRes = _DefaultConfig();

      ERROR( SS_MESSAGE_CANTLOADCFG, pszFileName, NULL, NULL );

      if ( !bRes )
        ERROR( SS_MESSAGE_CANTMAKEDEFCFG, NULL, NULL, NULL );
      else
        CtrlStoreConfig();
    }
  }
}

VOID CtrlDone()
{
  NetAclDone( &sAdminAcl );
  if ( pszConfigFile != NULL )
    free( pszConfigFile );
}

BOOL CtrlStoreConfig()
{
  xmlDocPtr	xmlDoc;
  xmlNodePtr	xmlNode;
  BOOL		bRes = FALSE;

  if ( pszConfigFile == NULL )
    return FALSE;

  xmlNode = _NewRootNode( "server" );
  if ( xmlNode == NULL )
    return FALSE;

  if ( (xmlDoc = _NewDoc()) == NULL )
  {
    xmlFreeNode( xmlNode );
    return FALSE;
  }

  _StoreAdmin( xmlDoc, xmlNewChild( xmlNode, NULL, "admin", NULL ) );
  _StoreGeneral( xmlDoc, xmlNewChild( xmlNode, NULL, "general", NULL ) );
  _StoreHosts( xmlDoc, xmlNewChild( xmlNode, NULL, "hosts", NULL ) );
  _StoreTemplates( xmlDoc, xmlNewChild( xmlNode, NULL, "templates", NULL ) );
  _StoreRelays( xmlDoc, xmlNewChild( xmlNode, NULL, "relays", NULL ), FALSE );

  xmlDocSetRootElement( xmlDoc, xmlNode );

  {
    PCHAR	pcPtr = strrchr( pszConfigFile, '.' );
    CHAR	acBuf[_MAX_PATH];
    ULONG	ulLength;

    if ( pcPtr != NULL )
      ulLength = pcPtr - pszConfigFile;
    else
      ulLength = strlen( pszConfigFile );

    memcpy( &acBuf, pszConfigFile, ulLength );
    memcpy( &acBuf[ulLength], ".TMP", 5 );
    bRes = xmlSaveFormatFileEnc( &acBuf, xmlDoc, "UTF-8", 1 ) != -1;

    if ( bRes )
    {
      memcpy( &acBuf[ulLength], ".BAK", 5 );
      unlink( &acBuf );
      if ( rename( pszConfigFile, &acBuf ) != 0 )
        unlink( pszConfigFile );

      memcpy( &acBuf[ulLength], ".TMP", 5 );
      rename( &acBuf, pszConfigFile );
    }
  }

  xmlFreeDoc( xmlDoc );

  if ( !bRes )
    ERROR( SS_MESSAGE_CANTSAVECFG, pszConfigFile, NULL, NULL );

  return bRes;
}

BOOL CtrlQueryTextSources(PCHAR *ppcText, PULONG pulSize, PSZ pszXSLT,
			  ULONG ulFlags)
{
  xmlNodePtr	xmlNode;
  PSOURCE	psSource;
  CHAR		acBuf[32];
  CHAR		acGMTLocalDiffStr[7];
  xmlDocPtr	xmlDoc;

  xmlNode = _NewRootNode( "sources" );
  if ( xmlNode == NULL )
    return FALSE;

  if ( (ulFlags & FLSRC_ADMIN_MODE) != 0 )
    xmlNewProp( xmlNode, "admin", "1" );

  acGMTLocalDiffStr[0] = ' ';
  _GetGMTOffset( &acGMTLocalDiffStr[1] );

  if ( (xmlDoc = _NewDoc()) == NULL )
  {
    xmlFreeNode( xmlNode );
    return FALSE;
  }

  psSource = SourceQueryList();
  while( psSource != NULL )
  {
    if ( (ulFlags & FLSRC_GET_HIDDEN) || !SourceGetHidden( psSource ) )
      _StoreSource( xmlDoc, xmlNewChild( xmlNode, NULL, "source", NULL ),
                    psSource, &acGMTLocalDiffStr, ulFlags );
    psSource = SourceGetNext( psSource );
  }
  SourceReleaseList();

  return _DocToText( xmlDoc, xmlNode, pszXSLT, ppcText, pulSize );
}

BOOL CtrlQueryTextSource(PCHAR *ppcText, PULONG pulSize, PSZ pszXSLT,
			 ULONG ulId, ULONG ulFlags)
{
  xmlNodePtr	xmlNode;
  PSOURCE	psSource;
  CHAR		acBuf[32];
  CHAR		acGMTLocalDiffStr[7];
  xmlDocPtr	xmlDoc;

  if ( ulId == UNKNOWN_ID )
    return CtrlQueryTextMessage( ppcText, pulSize, MSGID_INVALID_REQUEST,
				 ulFlags );

  psSource = SourceQueryById( ulId );
  if ( psSource == NULL || 
       ((ulFlags & FLSRC_GET_HIDDEN == 0) && SourceGetHidden(psSource)) )
  {
    return CtrlQueryTextMessage( ppcText, pulSize, MSGID_SOURCE_NOT_EXISTS,
                                 ulFlags | FLMSG_PART_SOURCES );
  }

  xmlNode = _NewRootNode( "source" );
  if ( xmlNode == NULL )
  {
    SourceRelease( psSource );
    return FALSE;
  }

  if ( (ulFlags & FLSRC_ADMIN_MODE) != 0 )
    xmlNewProp( xmlNode, "admin", "1" );

  acGMTLocalDiffStr[0] = ' ';
  _GetGMTOffset( &acGMTLocalDiffStr[1] );

  if ( (xmlDoc = _NewDoc()) == NULL )
  {
    xmlFreeNode( xmlNode );
    return FALSE;
  }

  psSource = SourceQueryById( ulId );
  if ( psSource )
  {
    _StoreSource( xmlDoc, xmlNode, psSource, &acGMTLocalDiffStr, ulFlags );
    SourceRelease( psSource );
  }

  return _DocToText( xmlDoc, xmlNode, pszXSLT, ppcText, pulSize );
}

BOOL CtrlQueryTextSourceStop(PCHAR *ppcText, PULONG pulSize, ULONG ulId)
{
  PSOURCE	psSource;

  if ( ulId == UNKNOWN_ID )
    return CtrlQueryTextMessage( ppcText, pulSize, MSGID_INVALID_REQUEST,
				 FLSRC_ADMIN_MODE | FLMSG_PART_SOURCES );

  psSource = SourceQueryById( ulId );
  if ( psSource == NULL )
  {
    return CtrlQueryTextMessage( ppcText, pulSize, MSGID_SOURCE_NOT_EXISTS,
				 FLSRC_ADMIN_MODE | FLMSG_PART_SOURCES );
  }

  SourceStop( psSource );
  SourceRelease( psSource );

  return CtrlQueryTextMessage( ppcText, pulSize, MSGID_SOURCE_STOPPED,
                               FLSRC_ADMIN_MODE | FLMSG_PART_SOURCES );
}

BOOL CtrlQueryTextClientStop(PCHAR *ppcText, PULONG pulSize,
                             ULONG ulSourceId, ULONG ulClientId)
{
  PSOURCE	psSource;
  BOOL		bRes;

  if ( ulSourceId == UNKNOWN_ID || ulClientId == UNKNOWN_ID )
  {
    return CtrlQueryTextMessage( ppcText, pulSize, MSGID_INVALID_REQUEST,
				 FLSRC_ADMIN_MODE | FLMSG_PART_SOURCES );
  }

  psSource = SourceQueryById( ulSourceId );
  if ( psSource == NULL )
  {
    return CtrlQueryTextMessage( ppcText, pulSize, MSGID_SOURCE_NOT_EXISTS,
				 FLSRC_ADMIN_MODE | FLMSG_PART_SOURCES );
  }

  bRes = SourceDetachClientById( psSource, ulClientId );
  SourceRelease( psSource );

  return CtrlQueryTextMessage( ppcText, pulSize, 
           bRes ? MSGID_CLIENT_STOPPED : MSGID_CLIENT_NOT_EXISTS,
           FLSRC_ADMIN_MODE | FLMSG_PART_SOURCES );
}

BOOL CtrlQueryTextRemoveRelay(PCHAR *ppcText, PULONG pulSize, ULONG ulRelayId)
{
  PRELAY	psRelay;

  if ( ulRelayId == UNKNOWN_ID )
  {
    return CtrlQueryTextMessage( ppcText, pulSize, MSGID_INVALID_REQUEST,
				 FLSRC_ADMIN_MODE | FLMSG_PART_RELAYS );
  }

  psRelay = RelayQueryById( ulRelayId );
  if ( psRelay == NULL )
  {
    return CtrlQueryTextMessage( ppcText, pulSize, MSGID_RELAY_NOT_EXISTS,
				 FLSRC_ADMIN_MODE | FLMSG_PART_RELAYS );
  }

  RelayDestroy( psRelay );
  RelayRelease( psRelay );
  CtrlStoreConfig();

  return CtrlQueryTextMessage( ppcText, pulSize, MSGID_RELAY_REMOVED,
                               FLSRC_ADMIN_MODE | FLMSG_PART_RELAYS );
}

BOOL CtrlQueryTextResetRelay(PCHAR *ppcText, PULONG pulSize, ULONG ulRelayId)
{
  PRELAY	psRelay;
  BOOL		bRes;

  if ( ulRelayId == UNKNOWN_ID )
  {
    return CtrlQueryTextMessage( ppcText, pulSize, MSGID_INVALID_REQUEST,
				 FLSRC_ADMIN_MODE | FLMSG_PART_RELAYS );
  }

  psRelay = RelayQueryById( ulRelayId );
  if ( psRelay == NULL )
  {
    return CtrlQueryTextMessage( ppcText, pulSize, MSGID_RELAY_NOT_EXISTS,
				 FLSRC_ADMIN_MODE | FLMSG_PART_RELAYS );
  }

  bRes = RelayReset( psRelay );
  RelayRelease( psRelay );

  return CtrlQueryTextMessage( ppcText, pulSize, 
			       bRes ? MSGID_RELAY_RESET : MSGID_RELAY_NO_RESET,
                               FLSRC_ADMIN_MODE | FLMSG_PART_RELAYS );
}

BOOL CtrlQueryTextConfig(PCHAR *ppcText, PULONG pulSize, ULONG ulPart)
{
  xmlNodePtr	xmlNode;
  xmlDocPtr	xmlDoc;
  PSZ		pszPart;

  switch( ulPart )
  {
    case CFGPART_HOSTS:
      pszPart = "hosts";
      break;
    case CFGPART_TEMPLATES:
      pszPart = "templates";
      break;
    case CFGPART_GENERAL:
      pszPart = "general";
      break;
    case CFGPART_ADMIN:
      pszPart = "admin";
      break;
    case CFGPART_RELAYS:
      pszPart = "relays";
      break;
    default:
      return FALSE;
  }

  xmlNode = _NewRootNode( pszPart );
  if ( xmlNode == NULL )
    return FALSE;

  if ( (xmlDoc = _NewDoc()) == NULL )
  {
    xmlFreeNode( xmlNode );
    return FALSE;
  }

  switch( ulPart )
  {
    case CFGPART_HOSTS:
      _StoreHosts( xmlDoc, xmlNode );
      break;
    case CFGPART_TEMPLATES:
      _StoreTemplates( xmlDoc, xmlNode );
      break;
    case CFGPART_GENERAL:
      _StoreGeneral( xmlDoc, xmlNode );
      break;
    case CFGPART_ADMIN:
      _StoreAdmin( xmlDoc, xmlNode );
      break;
    case CFGPART_RELAYS:
      _StoreRelays( xmlDoc, xmlNode, TRUE );
      break;

  }

  return _DocToText( xmlDoc, xmlNode, pszPart, ppcText, pulSize );
}

BOOL CtrlQueryTextMessage(PCHAR *ppcText, PULONG pulSize, ULONG ulMsgId,
			  ULONG ulFlags)
{
  xmlNodePtr	xmlNode;
  PMESSAGE	psMessage = &asMessages[ulMsgId];
  CHAR		acBuf[32];
  xmlDocPtr	xmlDoc;

  xmlNode = _NewRootNode( "message" );
  if ( xmlNode == NULL )
  {
    return FALSE;
  }

  if ( (xmlDoc = _NewDoc()) == NULL )
  {
    xmlFreeNode( xmlNode );
    return FALSE;
  }

  if ( (ulFlags & FLSRC_ADMIN_MODE) != 0 )
    xmlNewProp( xmlNode, "admin", "1" );

  ultoa( ulMsgId, &acBuf, 10 );
  xmlNewChild( xmlNode, NULL, "message-id", &acBuf );
  xmlNewChild( xmlNode, NULL, "type", apszMessageTypes[psMessage->ulType] );
  _NewTextNode( xmlDoc, xmlNode, "text", psMessage->pszText );

  if ( (ulFlags & FLMSG_PART_ADMIN) != 0 )
    xmlNewChild( xmlNode, NULL, "part", "admin" );
  if ( (ulFlags & FLMSG_PART_GENERAL) != 0 )
    xmlNewChild( xmlNode, NULL, "part", "general" );
  if ( (ulFlags & FLMSG_PART_HOSTS) != 0 )
    xmlNewChild( xmlNode, NULL, "part", "hosts" );
  if ( (ulFlags & FLMSG_PART_TEMPLATES) != 0 )
    xmlNewChild( xmlNode, NULL, "part", "templates" );
  if ( (ulFlags & FLMSG_PART_SOURCES) != 0 )
    xmlNewChild( xmlNode, NULL, "part", "sources" );
  if ( (ulFlags & FLMSG_PART_RELAYS) != 0 )
    xmlNewChild( xmlNode, NULL, "part", "relays" );

  return _DocToText( xmlDoc, xmlNode, "message", ppcText, pulSize );
}

VOID CtrlReleaseText(PCHAR pcText)
{
  if ( pcText != NULL )
    xmlFree( pcText );
}

BOOL CtrlTestAcl(PCONNINFO psConnInfo)
{
  return NetAclTest( &sAdminAcl, &psConnInfo->sClientAddr );
}

BOOL CtrlTestLogin(PSZ pszName, PSZ pszPassword)
{
  return
    ( acAdminName[0] != '\0' && (pszName != NULL && 
      (strncmp( &acAdminName, pszName, MAX_ADMIN_NAME_LENGTH ) == 0)) )
    &&
    ( acAdminPassword[0] != '\0' && (pszPassword != NULL && 
      (strncmp( &acAdminPassword, pszPassword, MAX_ADMIN_NAME_LENGTH ) == 0)) );
}


VOID CtrlInputNew(PCONFIGINPUT psConfigInput)
{
  psConfigInput->pCtx = NULL;
}

VOID CtrlInputDestroy(PCONFIGINPUT psConfigInput)
{
  if ( psConfigInput->pCtx != NULL )
    xmlFreeParserCtxt( (xmlParserCtxtPtr)(psConfigInput->pCtx) );
}

BOOL CtrlInputWrite(PCONFIGINPUT psConfigInput, PCHAR pcBuf, ULONG ulBufLen)
{
  BOOL		bRes;

  if ( psConfigInput->pCtx == NULL )
  {
    psConfigInput->pCtx =
      (PVOID)xmlCreatePushParserCtxt(NULL, NULL, pcBuf, ulBufLen, "config.xml");
    bRes = psConfigInput->pCtx != NULL;
  }
  else
    bRes = xmlParseChunk( (xmlParserCtxtPtr)(psConfigInput->pCtx),
                          pcBuf, ulBufLen, 0 ) == 0;

  return bRes;
}

BOOL CtrlInputApply(PCONFIGINPUT psConfigInput)
{
  xmlDocPtr	xmlDoc;
  xmlNodePtr	xmlNodeRoot;

  if ( xmlParseChunk( (xmlParserCtxtPtr)(psConfigInput->pCtx),
                      NULL, 0, 1 ) != 0 )
    return FALSE;

  xmlDoc = ((xmlParserCtxtPtr)psConfigInput->pCtx)->myDoc;
  if ( xmlDoc == NULL )
    return FALSE;

  xmlNodeRoot = xmlDocGetRootElement( xmlDoc );
  if ( xmlNodeRoot )
  {
    _ApplyXMLNode( xmlNodeRoot );
    CtrlStoreConfig();
  }

  xmlFreeDoc( xmlDoc );
  return TRUE;
}
