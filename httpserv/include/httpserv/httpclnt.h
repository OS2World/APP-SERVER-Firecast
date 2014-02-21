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

#ifndef HTTPCLNT_H
#define HTTPCLNT_H

#include "system.h"
#include "httpserv\reqfields.h"

#define HTTPCLNT_RES_OK			0
#define HTTPCLNT_RES_ERROR		1
#define HTTPCLNT_RES_INVALID_URL	2

#define HTTPCLNT_STATE_DONE			0x0001
#define HTTPCLNT_STATE_SEARCH_NAME		0x0002
#define HTTPCLNT_STATE_CONNECTING		0x0003
#define HTTPCLNT_STATE_SEND_HEADER		0x0004
#define HTTPCLNT_STATE_READ_HEADER		0x0005
#define HTTPCLNT_STATE_SEND_BODY		0x0006
#define HTTPCLNT_STATE_READ_BODY		0x0007
#define HTTPCLNT_STATE_ERROR_MASK		0xFF00
#define HTTPCLNT_STATE_NAME_NOT_FOUND		0x0100
#define HTTPCLNT_STATE_ERROR_CREATE_SOCKET 	0x0200
#define HTTPCLNT_STATE_CONNECTION_CLOSED 	0x0300
#define HTTPCLNT_STATE_CONNECTION_REFUSED 	0x0400
#define HTTPCLNT_STATE_ADDRESS_UNREACH 		0x0500
#define HTTPCLNT_STATE_NO_BUFFERS_LEFT 		0x0600
#define HTTPCLNT_STATE_CANNOT_CONNECT 		0x0700
#define HTTPCLNT_STATE_INVALID_RESPONSE		0x0800

#define CLNT_CONNECTION_BUF_SIZE	2048

typedef BOOL HTTPCLNTREADCALLBACK(PVOID, PCHAR, ULONG, PULONG);
typedef HTTPCLNTREADCALLBACK *PHTTPCLNTREADCALLBACK;

typedef BOOL HTTPCLNTWRITECALLBACK(PVOID, PCHAR, ULONG, PULONG);
typedef HTTPCLNTWRITECALLBACK *PHTTPCLNTWRITECALLBACK;

typedef BOOL HTTPCLNTHEADERCALLBACK(PVOID, ULONG, PREQFIELDS);
typedef HTTPCLNTHEADERCALLBACK *PHTTPCLNTHEADERCALLBACK;

typedef struct _HTTPCLNT {
  USHORT			usPort;
  ULONG				ulSocket;
  ULONG				ulState;
  ULONG				ulFlags;
  ULLONG			ullLength;
  ULONG				ulChunkBytes;
  PVOID				pObject;
  PSZ				pszAddr;
  ULONG				ulRespCode;
  PHTTPCLNTREADCALLBACK 	cbRead;
  PHTTPCLNTWRITECALLBACK 	cbWrite;
  PHTTPCLNTHEADERCALLBACK 	cbHeader;
  REQFIELDS			sFields;
  ULONG				ulBufPos;
  ULONG				ulTransmitPos;
  CHAR				acBuf[CLNT_CONNECTION_BUF_SIZE];
} HTTPCLNT, *PHTTPCLNT;

PHTTPCLNT HTTPClntCreate(PVOID pObject,
                         PHTTPCLNTREADCALLBACK cbRead,
                         PHTTPCLNTWRITECALLBACK cbWrite,
                         PHTTPCLNTHEADERCALLBACK cbHeader);
VOID HTTPClntDestroy(PHTTPCLNT psHTTPClnt);
ULONG HTTPClntRequest(PHTTPCLNT psHTTPClnt, PSZ pszMethod, PSZ pszURL,
                      PREQFIELDS psFields);
ULONG HTTPClntProcess(PHTTPCLNT psHTTPClnt);
#define HTTPClntGetState(ps) ((ps)->ulState)

#endif // HTTPSERV_H
