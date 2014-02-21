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

#ifndef CONFIG_H
#define CONFIG_H

#include "system.h"

#define FLSRC_GET_HIDDEN	0x0001
#define FLSRC_TEMPLATE_ID	0x0002
#define FLSRC_META_HISTORY	0x0004
#define FLSRC_CLIENTS		0x0008
#define FLSRC_ADMIN_MODE	0x0010
#define FLSRC_SRCADDR		0x0020

#define FLMSG_PART_SOURCES	0x0100
#define FLMSG_PART_TEMPLATES	0x0200
#define FLMSG_PART_HOSTS	0x0400
#define FLMSG_PART_GENERAL	0x0800
#define FLMSG_PART_ADMIN	0x1000
#define FLMSG_PART_RELAYS	0x2000

#define	MSGID_INVALID_REQUEST		0
#define	MSGID_SOURCE_NOT_EXISTS		1
#define	MSGID_SOURCE_STOPPED		2
#define	MSGID_CLIENT_NOT_EXISTS		3
#define	MSGID_CLIENT_STOPPED		4
#define MSGID_XML_PARCE_ERROR		5
#define MSGID_CFG_UPDATED		6
#define MSGID_RELAY_NOT_EXISTS		7
#define MSGID_RELAY_REMOVED		8
#define MSGID_RELAY_RESET		9
#define MSGID_RELAY_NO_RESET		10

#define CFGPART_HOSTS			0
#define CFGPART_TEMPLATES		1
#define CFGPART_GENERAL			2
#define CFGPART_ADMIN			3
#define CFGPART_RELAYS			4

#define MAX_ADMIN_NAME_LENGTH		32
#define MAX_ADMIN_PASSWORD_LENGTH	32

typedef struct _CONFIGINPUT {
  PVOID		pCtx;
} CONFIGINPUT, *PCONFIGINPUT;

VOID CtrlInit(PSZ pszFileName);
VOID CtrlDone();
BOOL CtrlStoreConfig();
BOOL CtrlQueryTextSources(PCHAR *ppcText, PULONG pulSize, PSZ pszXSLT,
			  ULONG ulFlags);
BOOL CtrlQueryTextSource(PCHAR *ppcText, PULONG pulSize, PSZ pszXSLT,
			 ULONG ulId, ULONG ulFlags);
BOOL CtrlQueryTextSourceStop(PCHAR *ppcText, PULONG pulSize, ULONG ulId);
BOOL CtrlQueryTextClientStop(PCHAR *ppcText, PULONG pulSize,
                             ULONG ulSourceId, ULONG ulClientId);
BOOL CtrlQueryTextRemoveRelay(PCHAR *ppcText, PULONG pulSize, ULONG ulRelayId);
BOOL CtrlQueryTextResetRelay(PCHAR *ppcText, PULONG pulSize, ULONG ulRelayId);
BOOL CtrlQueryTextConfig(PCHAR *ppcText, PULONG pulSize, ULONG ulPart);
BOOL CtrlQueryTextMessage(PCHAR *ppcText, PULONG pulSize, ULONG ulMsgId,
                          ULONG ulFlags);
VOID CtrlReleaseText(PCHAR pcText);
BOOL CtrlTestAcl(PCONNINFO psConnInfo);
BOOL CtrlTestLogin(PSZ pszName, PSZ pszPassword);

VOID CtrlInputNew(PCONFIGINPUT psConfigInput);
VOID CtrlInputDestroy(PCONFIGINPUT psConfigInput);
BOOL CtrlInputWrite(PCONFIGINPUT psConfigInput, PCHAR pcBuf, ULONG ulBufLen);
BOOL CtrlInputApply(PCONFIGINPUT psConfigInput);

#endif // CONFIG_H
