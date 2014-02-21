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

#ifndef REQUESTS_H
#define REQUESTS_H

#include "httpserv\netserv.h"
#include "httpserv\reqfields.h"

typedef PCHAR RESPCODE;

typedef struct _CONNINFO {
  struct in_addr	sClientAddr;
  struct in_addr	sServAddr;
  USHORT		usServPort;
} CONNINFO, *PCONNINFO;

typedef RESPCODE REQNEWCALLBACK(PVOID *ppRequest, PCHAR pcProto,
                              PCHAR pcMethod, PCHAR pcURI, PREQFIELDS psFields,
                              PCONNINFO psConnInfo);
typedef VOID REQDESTROYCALLBACK(PVOID pRequest);
typedef RESPCODE REQWRITECALLBACK(PVOID pRequest, PCHAR pcBuf, ULONG ulBufLen,
                                  PULONG pulActual);
typedef VOID REQGETFIELDSCALLBACK(PVOID pRequest, RESPCODE ulRespCode,
                                  PREQFIELDS psRespFields);
typedef ULONG REQREADCALLBACK(PVOID pRequest, PCHAR pcBuf, ULONG ulBufLen,
                              PULONG pulActual);

#define REQ_READ_CB_OK		0
#define REQ_READ_CB_ERROR	1
#define REQ_READ_CB_EOF		2

#define RESP_CODE_200_ICES "200 OK ICES" // work-around with ICES ugly protocol

#define RESP_CODE_100 "100 Continue"
#define RESP_CODE_101 "101 Switching Protocols"
#define RESP_CODE_200 "200 OK"
#define RESP_CODE_201 "201 Created"
#define RESP_CODE_202 "202 Accepted"
#define RESP_CODE_203 "203 Non-Authoritative Information"
#define RESP_CODE_204 "204 No Content"
#define RESP_CODE_205 "205 Reset Content"
#define RESP_CODE_206 "206 Partial Content"
#define RESP_CODE_300 "300 Multiple Choices"
#define RESP_CODE_301 "301 Moved Permanently"
#define RESP_CODE_302 "302 Moved Temporarily"
#define RESP_CODE_303 "303 See Other"
#define RESP_CODE_304 "304 Not Modified"
#define RESP_CODE_305 "305 Use Proxy"
#define RESP_CODE_400 "400 Bad Request"
#define RESP_CODE_401 "401 Unauthorized"
#define RESP_CODE_402 "402 Payment Required"
#define RESP_CODE_403 "403 Forbidden"
#define RESP_CODE_404 "404 Not Found"
#define RESP_CODE_405 "405 Method Not Allowed"
#define RESP_CODE_406 "406 Not Acceptable"
#define RESP_CODE_407 "407 Proxy Authentication Required"
#define RESP_CODE_408 "408 Request Timeout"
#define RESP_CODE_409 "409 Conflict"
#define RESP_CODE_410 "410 Gone"
#define RESP_CODE_411 "411 Length Required"
#define RESP_CODE_412 "412 Precondition Failed"
#define RESP_CODE_413 "413 Request Entity Too Large"
#define RESP_CODE_414 "414 Request-URI Too Long"
#define RESP_CODE_415 "415 Unsupported Media Type"
#define RESP_CODE_500 "500 Internal Server Error"
#define RESP_CODE_501 "501 Not Implemented"
#define RESP_CODE_502 "502 Bad Gateway"
#define RESP_CODE_503 "503 Service Unavailable"
#define RESP_CODE_504 "504 Gateway Timeout"
#define RESP_CODE_505 "505 HTTP Version Not Supported"

typedef struct _REQCB {
  REQNEWCALLBACK	*cbNew;
  REQDESTROYCALLBACK	*cbDestroy;
  REQWRITECALLBACK	*cbWrite;
  REQGETFIELDSCALLBACK  *cbGetFields;
  REQREADCALLBACK	*cbRead;
} REQCB, *PREQCB;

BOOL ReqInit(PREQCB psRequestCB, ULONG ulThreadsInit, PSZ pszServerName);
VOID ReqDone();
PNETSERV ReqGetNetServ();
BOOL ReqFillConnInfo(ULONG ulSocket, PCONNINFO psConnInfo);
PSZ ReqGetServerName();

#endif // REQUESTS_H
