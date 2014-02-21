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

#ifndef MESSAGES_H
#define MESSAGES_H

#include "system.h"

#define NET_MODULE_SERVER	0
#define NET_MODULE_REQUESTS	1
#define NET_MODULE_HTTPSERV	2
#define NET_MODULE_FILERES	3
#define NET_MODULE_RESOLV	4
#define NET_MODULE_HTTPCLNT	5

#define NET_MSGTYPE_ERROR	0
#define NET_MSGTYPE_WARNING	1
#define NET_MSGTYPE_INFO	2
#define NET_MSGTYPE_DETAIL	3
#define NET_MSGTYPE_DEBUG	4

#define NET_MESSAGE_NOTENOUGHTMEM	1000
#define NET_MESSAGE_CANTRESOLV		1001
#define NET_MESSAGE_CANTBINDSOCK	1002
#define NET_MESSAGE_CANTCREATESRVSOCK	1003
#define NET_MESSAGE_SETBIND		1004
#define NET_MESSAGE_REMOVEBIND		1005
#define NET_MESSAGE_CONNECTION		1006
#define NET_MESSAGE_CONNECTIONCLOSE	1007
#define NET_MESSAGE_UNKNOWNHOST		1008
#define NET_MESSAGE_HOSTACCESSDENIED	1009
#define NET_MESSAGE_REQUEST		1010
#define NET_MESSAGE_REQUESTRESULT	1011
#define NET_MESSAGE_CANTOPENFILE	1012
#define NET_MESSAGE_NAMENOTFOUND	1013

typedef VOID NETMSGCALLBACK (ULONG ulModule, ULONG ulType, ULONG ulCode,
                             PVOID pParam1, PVOID pParam2, PVOID pParam3);
typedef NETMSGCALLBACK *PNETMSGCALLBACK;

#undef ERROR
#define ERROR(c,p1,p2,p3) NetMessage(NET_MODULE,NET_MSGTYPE_ERROR,c,p1,p2,p3)
#define WARNING(c,p1,p2,p3) NetMessage(NET_MODULE,NET_MSGTYPE_WARNING,c,p1,p2,p3)
#define INFO(c,p1,p2,p3) NetMessage(NET_MODULE,NET_MSGTYPE_INFO,c,p1,p2,p3)
#define DETAIL(c,p1,p2,p3) NetMessage(NET_MODULE,NET_MSGTYPE_DETAIL,c,p1,p2,p3)
#define DBGINFO(c,p1,p2,p3) NetMessage(NET_MODULE,NET_MSGTYPE_DEBUG,c,p1,p2,p3)

#define ERROR_NOT_ENOUGHT_MEM() NetMessage(NET_MODULE,NET_MSGTYPE_ERROR,NET_MESSAGE_NOTENOUGHTMEM,NULL,NULL,NULL)

VOID NetMessageCBSet(PNETMSGCALLBACK cbFunc, ULONG ulLevel);
VOID NetMessage(ULONG ulModule, ULONG ulType, ULONG ulCode,
                PVOID pParam1, PVOID pParam2, PVOID pParam3);
ULONG NetMessageGetLevel();

#endif // MESSAGES_H
