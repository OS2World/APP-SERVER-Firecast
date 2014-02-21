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

#include "httpserv/netmessages.h"

#ifdef __OS2__
#define SERVER_STRING	"Firecast (OS/2 / eComStation, bld. "__DATE__")"
#else
#define SERVER_STRING	"Firecast (Windows, bld. "__DATE__")"
#endif

#define SS_MODULE_TEMPLATES	101
#define SS_MODULE_SOURCES	102
#define SS_MODULE_CONTROL	103
#define SS_MODULE_FILERES	104
#define SS_MODULE_FORMAT	105
#define SS_MODULE_RELAYS	106

#define SS_MESSAGE_TEMPLDENIED			5001
#define SS_MESSAGE_TEMPLINVALIDPSWD		5002
#define SS_MESSAGE_MPOINTEXIST			5003
#define SS_MESSAGE_CLIENTACCESSDENIED		5004
#define SS_MESSAGE_TOOMANYCLIENTS		5005
#define SS_MESSAGE_NEWSOURCE			5006
#define SS_MESSAGE_DESTROYSOURCE		5007
#define SS_MESSAGE_CANTLOADCFG			5008
#define SS_MESSAGE_CANTMAKEDEFCFG		5009
#define SS_MESSAGE_CANTSAVECFG			5010
#define SS_MESSAGE_INVALIDMETAREQUEST		5011
#define SS_MESSAGE_UNAUTHORIZED			5012
#define SS_MESSAGE_SOURCENOTFOUND		5013
#define SS_MESSAGE_SETMETADATA			5014
#define SS_MESSAGE_INVALIDDATAFORMAT		5015
#define SS_MESSAGE_RELAY_STATE			5016
#define SS_MESSAGE_RELAY_HTTP_STATUS_CODE	5017
#define SS_MESSAGE_RELAY_HTTP_CONTENT_TYPE	5018
