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

#ifndef FILERES_H
#define FILERES_H

#include <time.h>
#include "system.h"
#include "httpserv\reqfields.h"
#include "httpserv\filehost.h"
#include "httpserv\requests.h"

#define RESMODE_READ	1
#define RESMODE_WRITE	2
#define RESMODE_FULL	(RESMODE_READ | RESMODE_WRITE)

#define FILERES_OK			0x00
#define FILERES_OK_CONTINUE		0x01
#define FILERES_OK_ICES			0x02
#define FILERES_WAIT_DATA		0x03
#define FILERES_PARTIAL_CONTENT		0x04
#define 	FILERES_ERROR_MASK	0xF0
#define FILERES_INTERNAL_ERROR		0x10
#define FILERES_NOT_FOUND		0x11
#define FILERES_NOT_ALLOWED		0x12
#define FILERES_NO_MORE_DATA		0x13
#define FILERES_IS_DIRECTORY		0x14
#define FILERES_PATH_TOO_LONG		0x15
#define FILERES_UNAUTHORIZED		0x16
#define FILERES_FORBIDDEN		0x17
#define FILERES_NOT_IMPLEMENTED		0x18
#define FILERES_NOT_MODIFED		0x19
#define FILERES_PRECONDITION_FAILED	0X20

typedef ULONG FMOPEN(PVOID pData, PFILEHOST psFileHost, PSZ pszPath,
                     PSZ pszParam, PSZ pszMethod, PREQFIELDS psFields,
                     PCONNINFO psConnInfo);
typedef VOID FMCLOSE(PVOID pData);
typedef ULONG FMREAD(PVOID pData, PCHAR pcBuf, ULONG ulBufLen,
                     PULONG pulActual);
typedef ULONG FMWRITE(PVOID pData, PCHAR pcBuf, ULONG ulBufLen,
                     PULONG pulActual);
typedef ULONG FMSEEK(PVOID pData, ULLONG ullPos);
typedef VOID FMGETFIELDS(PVOID pData, RESPCODE rcRespCode, PREQFIELDS psRespFields);
typedef ULONG FMGETTIME(PVOID pData, time_t *pTime);
typedef ULONG FMGETLENGTH(PVOID pData, PULLONG pullLength);

typedef struct _FILEMETHODS {
  FMOPEN	*FmOpen;
  FMCLOSE	*FmClose;
  FMREAD	*FmRead;
  FMWRITE	*FmWrite;
  FMSEEK	*FmSeek;
  FMGETFIELDS	*FmGetFields;
  FMGETTIME	*FmGetTime;
  FMGETLENGTH   *FmGetLength;
} FILEMETHODS, *PFILEMETHODS;

typedef struct _FILERANGE {
  ULLONG	ullBegin;
  ULLONG	ullEnd;
} FILERANGE, *PFILERANGE;

typedef struct _FILERES {
  PFILEMETHODS		psFileMethods;
  PFILERANGE		psFileRange;
  CHAR			acData[1];
} FILERES, *PFILERES;

typedef ULONG GETMETHODSCALLBACK( PFILEMETHODS *ppsMethods,
                                  PFILEHOST psFileHost,
                                  PSZ pszPath, PSZ pszMethod );

BOOL FileResInit(PSZ pszMIMETypesFileName);
VOID FileResDone();
GETMETHODSCALLBACK *FileResMethodsSelector(GETMETHODSCALLBACK *cbNewGetMethods);
ULONG FileResOpen(PFILERES *ppsFileRes, PFILEHOST psFileHost, PSZ pszPath,
                  PSZ pszParam, PSZ pszMethod, PREQFIELDS psFields,
                  PCONNINFO psConnInfo);
ULONG FileResMemOpen(PFILERES *ppsFileRes, ULONG ulLength, PCHAR pcType);
VOID FileResClose(PFILERES psFileRes);
ULONG FileResWrite(PFILERES psFileRes, PCHAR pcBuf, ULONG ulBufLen,
                   PULONG pulActual);
ULONG FileResRead(PFILERES psFileRes, PCHAR pcBuf, ULONG ulBufLen,
                   PULONG pulActual);
ULONG FileResSeek(PFILERES psFileRes, ULLONG ullPos);
VOID FileResGetFields(PFILERES psFileRes, RESPCODE rcRespCode, PREQFIELDS psRespFields);
ULONG FileResGetTime(PFILERES psFileRes, time_t *pTime);
ULONG FileResGetLength(PFILERES psFileRes, PULLONG pullLength);

// --- Utils ---

#define B64ENCODEDLENGTH(a) ( (4 * (a + ((3 - (a % 3)) % 3)) / 3) )

ULONG FileResURLDecode(PCHAR pcDest, PCHAR pcScr, ULONG ulLength);
PSZ FileResURIParam(PSZ *ppszParamStr, PSZ *ppszValue);
ULONG FileResBase64Enc(PCHAR pcBuf, ULONG ulBufLen, PSZ pszStr);
VOID FileResBase64Dec(PCHAR pcBuf, ULONG ulBufLen, PSZ pszB64Str);
PSZ FileResGetBasicAuthFld(PREQFIELDS psFields, PCHAR pcBuf, ULONG ulBufLen,
                           PSZ*	ppszPassword);
BOOL FileResTestContentType(PREQFIELDS psFields, PSZ pszType);
time_t FileResParseTime(PCHAR pcTime);

#endif // FILERES_H
