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

#include <io.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys\stat.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <sys\stat.h>
#include <time.h>
#include <sys\timeb.h> 
#include "httpserv\fileres.h"
#include "httpserv\netmessages.h"

#include <malloc.h>

#define MAX_TYPE_NAME_LENGTH	32
#define MAX_DISK_PATH_LENGTH	320

#define NET_MODULE	NET_MODULE_HTTPSERV

typedef struct _MIMETYPEITEM {
  ULONG		ulExt;
  PSZ		pszType;
} MIMETYPEITEM, *PMIMETYPEITEM;

static ULONG			ulMIMETypesCount = 0;
static PMIMETYPEITEM		pasMIMETypes = NULL;

static LONG lExtBase64Decode[256]={
  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  62,  -1,  -1,  -1,  63,
  52,  53,  54,  55,  56,  57,  58,  59,  60,  61,  -1,  -1,  -1,  -2,  -1,  -1,
  -1,   0,   1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12,  13,  14,
  15,  16,  17,  18,  19,  20,  21,  22,  23,  24,  25,  -1,  -1,  -1,  -1,  -1,
  -1,  26,  27,  28,  29,  30,  31,  32,  33,  34,  35,  36,  37,  38,  39,  40,
  41,  42,  43,  44,  45,  46,  47,  48,  49,  50,  51,  -1,  -1,  -1,  -1,  -1,
  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1 };

static PUCHAR months[12] = 
  {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};

static CHAR alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

#define skip_space(ptr) \
  while( isspace( *ptr ) && *ptr != '\r' && *ptr != '\n' ) ptr++

static PSZ _GetMIMEType( PCHAR pcExt );


typedef struct _DATARESDISK {
  ULONG			ulFile;
  PSZ			pszType;
} DATARESDISK, *PDATARESDISK;

typedef struct _FILERESDISK {
  PFILEMETHODS		psFileMethods;
  PFILERANGE		psFileRange;
  DATARESDISK		sData;
} *PFILERESDISK;

static ULONG _DiskGetMethods( PFILEMETHODS *ppsMethods, PFILEHOST psFileHost,
                              PSZ pszPath, PSZ pszMethod );
static ULONG _DiskOpen(PVOID pData, PFILEHOST psFileHost, PSZ pszPath,
                       PSZ pszParam, PSZ pszMethod, PREQFIELDS psFields,
                       PCONNINFO psConnInfo);
static VOID _DiskClose(PVOID pData);
static ULONG _DiskRead(PVOID pData, PCHAR pcBuf, ULONG ulBufLen,
                     PULONG pulActual);
static ULONG _DiskWrite(PVOID pData, PCHAR pcBuf, ULONG ulBufLen,
                     PULONG pulActual);
static ULONG _DiskSeek(PVOID pData, ULLONG ullPos);
static VOID _DiskGetFields(PVOID pData, RESPCODE rcRespCode, PREQFIELDS psRespFields);
static ULONG _DiskGetTime(PVOID pData, time_t *pTime);
static ULONG _DiskGetLength(PVOID pData, PULLONG pullLength);

static struct _FILEMETHODS	sDiskMethods =
{
  _DiskOpen, _DiskClose, _DiskRead, _DiskWrite,
  _DiskSeek, _DiskGetFields, _DiskGetTime, _DiskGetLength
};


typedef struct _DATARESMEM {
  ULONG			ulPos;
  ULONG			ulLength;
  ULONG			ulMaxLength;
  CHAR			acType[MAX_TYPE_NAME_LENGTH + 1];
  CHAR			acBuf[1];
} DATARESMEM, *PDATARESMEM;

typedef struct _FILERESMEM {
  PFILEMETHODS		psFileMethods;
  PFILERANGE		psFileRange;
  DATARESMEM		sData;
} *PFILERESMEM;

static ULONG _MemRead(PVOID pData, PCHAR pcBuf, ULONG ulBufLen,
                     PULONG pulActual);
static ULONG _MemWrite(PVOID pData, PCHAR pcBuf, ULONG ulBufLen,
                     PULONG pulActual);
static ULONG _MemSeek(PVOID pData, ULLONG ullPos);
static VOID _MemGetFields(PVOID pData, RESPCODE rcRespCode, PREQFIELDS psRespFields);
static ULONG _MemGetLength(PVOID pData, PULLONG pullLength);

static struct _FILEMETHODS	sMemMethods =
{
  NULL, NULL, _MemRead, _MemWrite, _MemSeek, _MemGetFields, NULL, _MemGetLength
};


static GETMETHODSCALLBACK	*cbGetMethods = _DiskGetMethods;


static ULONG _DiskGetMethods( PFILEMETHODS *ppsMethods, PFILEHOST psFileHost, 
                              PSZ pszPath, PSZ pszMethod )
{
  *ppsMethods = &sDiskMethods;
  return sizeof( DATARESDISK );
}


static ULONG _DiskOpen(PVOID pData, PFILEHOST psFileHost, PSZ pszPath,
                       PSZ pszParam, PSZ pszMethod, PREQFIELDS psFields,
                       PCONNINFO psConnInfo)
{
  PDATARESDISK	psDataResDisk = (PDATARESDISK)pData;
  ULONG		ulFlags;
  ULONG		ulLength;
  CHAR		acFile[MAX_DISK_PATH_LENGTH + 1];
  PCHAR		pcPtr;

  if ( psFileHost == NULL )
    return (ULONG)(-1);

  ulLength = strlen( pszPath ) + psFileHost->ulBasePathLen;

  if ( ulLength > MAX_DISK_PATH_LENGTH )
    return FILERES_PATH_TOO_LONG;

  memcpy( acFile, psFileHost->pszBasePath, psFileHost->ulBasePathLen );
  strcpy( &acFile[psFileHost->ulBasePathLen], pszPath );

  pcPtr = &acFile[psFileHost->ulBasePathLen];
  while( pcPtr = strchr( pcPtr, '/' ) )
  {
    *pcPtr = '\\';
    pcPtr++;
  }

  if ( strstr( &acFile[psFileHost->ulBasePathLen], "\\." ) != NULL )
    return FILERES_NOT_FOUND;

  if ( acFile[ulLength-1] == '\\' )
  {
    ulLength--;
    acFile[ulLength] = '\0';
  }

  switch( SysCheckPath( &acFile ) )
  {
    case CHECK_PATH_NOT_FOUND:
      return FILERES_NOT_FOUND;
    case CHECK_PATH_DIRECTORY:
      return FILERES_IS_DIRECTORY;
  //  case CHECK_PATH_FILE:
  }

/*  switch ( ulMode )
  {
    case RESMODE_READ:*/
      ulFlags = O_RDONLY | O_BINARY;
/*      break;
    case RESMODE_WRITE:
      ulFlags = O_CREAT | O_TRUNC | O_WRONLY | O_BINARY;
      break;
    case RESMODE_FULL:
      ulFlags = O_CREAT | O_TRUNC | O_RDWR | O_BINARY;
      break;
  }*/

  psDataResDisk->ulFile = open( &acFile, ulFlags, S_IREAD | S_IWRITE );

  if ( psDataResDisk->ulFile == -1 )
  {
    INFO( NET_MESSAGE_CANTOPENFILE, &acFile, NULL, NULL );
    return FILERES_INTERNAL_ERROR;
  }

  pcPtr = strrchr( acFile, '.' );
  if ( pcPtr != NULL && strchr( pcPtr, '\\' ) == NULL )
  {
    psDataResDisk->pszType = _GetMIMEType( pcPtr+1 );
  }
  else
    psDataResDisk->pszType = NULL;

  return FILERES_OK;
}

static VOID _DiskClose(PVOID pData)
{
  PDATARESDISK	psDataResDisk = (PDATARESDISK)pData;

  close( psDataResDisk->ulFile );
}

static ULONG _DiskRead(PVOID pData, PCHAR pcBuf, ULONG ulBufLen,
                     PULONG pulActual)
{
  PDATARESDISK	psDataResDisk = (PDATARESDISK)pData;
  LONG		lRC;

  lRC = read( psDataResDisk->ulFile, pcBuf, ulBufLen );
  if ( lRC == -1 )
  {
    *pulActual = 0;
    return errno == EBADF ? FILERES_NOT_ALLOWED : FILERES_INTERNAL_ERROR;
  }

  *pulActual = lRC;
  return lRC < ulBufLen ? FILERES_NO_MORE_DATA : FILERES_OK;
}

static ULONG _DiskWrite(PVOID pData, PCHAR pcBuf, ULONG ulBufLen,
                     PULONG pulActual)
{
  PDATARESDISK	psDataResDisk = (PDATARESDISK)pData;
  LONG		lRC;

  lRC = write( psDataResDisk->ulFile, pcBuf, ulBufLen );
  if ( lRC == -1 )
  {
    *pulActual = 0;
    return errno == EBADF ? FILERES_NOT_ALLOWED : FILERES_INTERNAL_ERROR;
  }

  *pulActual = lRC;
  return lRC < ulBufLen ? FILERES_INTERNAL_ERROR : FILERES_OK;
}

static ULONG _DiskSeek(PVOID pData, ULLONG ullPos)
{
  PDATARESDISK	psDataResDisk = (PDATARESDISK)pData;
  LONG		lRC;

  lRC = lseek( psDataResDisk->ulFile, ullPos, SEEK_SET );

  return lRC == -1 ? FILERES_INTERNAL_ERROR : FILERES_OK;
}

static VOID _DiskGetFields(PVOID pData, RESPCODE rcRespCode, PREQFIELDS psRespFields)
{
  PDATARESDISK	psDataResDisk = (PDATARESDISK)pData;

  if ( psDataResDisk == NULL )
    return;

  if ( psDataResDisk->pszType != NULL )
    ReqFldSet( psRespFields, "Content-Type", psDataResDisk->pszType );
}

static ULONG _DiskGetTime(PVOID pData, time_t *pTime)
{
  PDATARESDISK	psDataResDisk = (PDATARESDISK)pData;
  struct stat	sStat;

  if ( fstat( psDataResDisk->ulFile, &sStat ) == -1 )
    return FILERES_INTERNAL_ERROR;

  *pTime = sStat.st_mtime;
  return FILERES_OK;
}

static ULONG _DiskGetLength(PVOID pData, PULLONG pullLength)
{
  PDATARESDISK	psDataResDisk = (PDATARESDISK)pData;

  *pullLength = _filelengthi64( psDataResDisk->ulFile );
  return *pullLength == (-1I64) ? FILERES_INTERNAL_ERROR : FILERES_OK;
}


static ULONG _MemRead(PVOID pData, PCHAR pcBuf, ULONG ulBufLen,
                     PULONG pulActual)
{
  PDATARESMEM	psData = (PDATARESMEM)pData;

  ulBufLen = min( psData->ulLength - psData->ulPos, ulBufLen );
  memcpy( pcBuf, &psData->acBuf[psData->ulPos], ulBufLen );
  psData->ulPos += ulBufLen;
  *pulActual = ulBufLen;

  return psData->ulPos == psData->ulLength ? FILERES_NO_MORE_DATA : FILERES_OK;
}

static ULONG _MemWrite(PVOID pData, PCHAR pcBuf, ULONG ulBufLen,
                       PULONG pulActual)
{
  PDATARESMEM	psData = (PDATARESMEM)pData;

  ulBufLen = min( psData->ulMaxLength - psData->ulPos, ulBufLen );
  if ( ulBufLen == 0 )
    return FILERES_INTERNAL_ERROR;

  memcpy( &psData->acBuf[psData->ulPos], pcBuf, ulBufLen );
  psData->ulPos += ulBufLen;
  if ( psData->ulPos > psData->ulLength )
    psData->ulLength = psData->ulPos;
  *pulActual = ulBufLen;

  return FILERES_OK;
}

static ULONG _MemSeek(PVOID pData, ULLONG ullPos)
{
  PDATARESMEM	psData = (PDATARESMEM)pData;

  if ( ullPos > psData->ulLength )
    ullPos = psData->ulLength;

  psData->ulPos = ullPos;
  return FILERES_OK;
}

static VOID _MemGetFields(PVOID pData, RESPCODE rcRespCode, PREQFIELDS psRespFields)
{
  PDATARESMEM	psData = (PDATARESMEM)pData;
  LONG		lRC;

  if ( psData == NULL )
    return;

  if ( psData->acType[0] != '\0' )
    ReqFldSet( psRespFields, "Content-Type", &psData->acType );
}

static ULONG _MemGetLength(PVOID pData, PULLONG pullLength)
{
  PDATARESMEM	psData = (PDATARESMEM)pData;

  *pullLength = psData->ulLength;
  return FILERES_OK;
}


static PSZ _GetMIMEType( PCHAR pcExt )
{
  ULONG		ulIdx;
  PMIMETYPEITEM	pMimeTypeItem;
  CHAR		acExt[4];

  *(PULONG)&acExt = 0;
  for( ulIdx = 0; ulIdx < 4; ulIdx++ )
  {
    if ( pcExt[ulIdx] == '\0' )
      break;
    acExt[ulIdx] = tolower(pcExt[ulIdx]);
  }

  for( ulIdx = 0; ulIdx < ulMIMETypesCount; ulIdx++ )
  {
    pMimeTypeItem = &pasMIMETypes[ulIdx];
    if ( pMimeTypeItem->ulExt == *(PULONG)&acExt )
      return pMimeTypeItem->pszType;
  }

  return NULL;
}

static BOOL _AddMIMETypeItem( ULONG ulExt, PSZ pszType )
{
  PMIMETYPEITEM	pasNewList;
  PMIMETYPEITEM	psMimeTypeItem;

  if ( ulMIMETypesCount == 0 || (ulMIMETypesCount & 0x0F) == 0x0F )
  {
    pasNewList = realloc( pasMIMETypes,
         (((ulMIMETypesCount+1) & 0xFFFFFFF0) + 0x10) * sizeof(MIMETYPEITEM) );
    if ( pasNewList == NULL )
      return FALSE;

    pasMIMETypes = pasNewList;
  }

  psMimeTypeItem = &pasMIMETypes[ulMIMETypesCount++];
  psMimeTypeItem->ulExt		= ulExt;
  psMimeTypeItem->pszType	= pszType;
  return TRUE;
}

static BOOL _AddMIMEType( PSZ pszString )
{
  PCHAR		pcPtr;
  CHAR		acExt[4];
  ULONG		ulExtCharCount;

  pcPtr = strpbrk( pszString, " \t" );
  if ( pcPtr == NULL || pcPtr == pszString )
    return TRUE;

  *pcPtr = '\0';
  if ( strchr( pszString, '/' ) == NULL )
    return TRUE;

  do
    pcPtr++;
  while ( *pcPtr == ' ' || *pcPtr == '\t' );
  if ( *pcPtr == '\0' )
    return TRUE;

  pszString = strdup( pszString );
  if ( pszString == NULL )
    return FALSE;

  do
  {
    ulExtCharCount = 0;
    *(PULONG)&acExt = 0;
    do
    {
      if ( ulExtCharCount < 4 )
        acExt[ ulExtCharCount++ ] = tolower( *pcPtr );
      pcPtr++;
    }
    while( *pcPtr != ' ' && *pcPtr != '\t' && *pcPtr != '\0' );

    if ( !_AddMIMETypeItem( *(PULONG)&acExt, pszString ) )
      return FALSE;

    while ( *pcPtr == ' ' || *pcPtr == '\t' )
      pcPtr++;
  }
  while( *pcPtr != '\0' );

  return TRUE;
}


static ULONG _hex(CHAR cHex)
{
  if ( cHex >= '0' && cHex <= '9')
    return cHex - '0';
  else if ( cHex >= 'A' && cHex <= 'F' )
    return cHex - 'A' + 10;
  else if( cHex >= 'a' && cHex <= 'f' )
    return cHex - 'a' + 10;
  else
    return (ULONG)(-1);
}

static BOOL _TestCond_IfModifiedSince(PREQFIELDS psFields, time_t timeFileRes)
{
  time_t	timeCond;
  PSZ		pszTimeCond;
  ULONG		ulRC;

  pszTimeCond = ReqFldGet( psFields, "If-Modified-Since" );
  if ( pszTimeCond == NULL )
    return TRUE;

  timeCond = FileResParseTime( pszTimeCond );
  if ( timeCond == 0 )
    return TRUE;

  return timeFileRes > timeCond;
}

static BOOL _TestCond_IfUnmodifiedSince(PREQFIELDS psFields,
               time_t timeFileRes, PSZ pszFieldName)
{
  time_t	timeCond;
  PSZ		pszTimeCond;
  ULONG		ulRC;

  pszTimeCond = ReqFldGet( psFields, pszFieldName );
  if ( pszTimeCond == NULL )
    return TRUE;

  timeCond = FileResParseTime( pszTimeCond );
  if ( timeCond == 0 )
    return TRUE;

  return timeFileRes <= timeCond;
}

static BOOL _MakeRange(PFILERES psFileRes, PSZ pszRange)
{
  ULLONG	ullLength, ullBegin, ullEnd;
  BOOL		bSuff;
  ULONG		ulRC;
  PCHAR		pcPtr;

  if ( pszRange == NULL || strncmp( pszRange, "bytes=", 6 ) != 0 )
    return FALSE;

  ulRC = FileResGetLength( psFileRes, &ullLength );
  if ( ulRC != FILERES_OK )
    return FALSE;

  pszRange += 6;
  if ( *pszRange != '-' )
  {
    ullBegin = strtoll( pszRange, &pcPtr, 10 ); 
    if ( pszRange == pcPtr )
      return FALSE;

    pszRange = pcPtr;
    bSuff = FALSE;
  }
  else
    bSuff = TRUE;

  if ( *pszRange != '-' )
    return FALSE;

  pszRange++;
  ullEnd = strtoll( pszRange, &pcPtr, 10 ); 
  if ( pszRange == pcPtr )
  {
    if ( bSuff )
      return FALSE;
    else
      ullEnd = ullLength - 1;
  }
  else
  {
    if ( bSuff )
    {
      ullBegin = ullLength - ullEnd;
      ullEnd = ullLength - 1;
    }

    if ( ullEnd > (ullLength - 1) )
      ullEnd = ullLength - 1;
  }

  if ( ullEnd < ullBegin )
    return FALSE;

  ulRC = FileResSeek( psFileRes, ullBegin );
  if ( ulRC != FILERES_OK )
    return FALSE;

  psFileRes->psFileRange = malloc( sizeof(FILERANGE) );
  psFileRes->psFileRange->ullBegin = ullBegin;
  psFileRes->psFileRange->ullEnd = ullEnd;

  return TRUE;
}

BOOL FileResInit(PSZ pszMIMETypesFileName)
{
  ULONG		ulFile;
  CHAR		acBuf[128];
  PCHAR		pcPtr;
  ULONG		ulCount;
  ULONG		ulBufPos;
  ULONG		bEOF;
  ULONG		ulActual;
 

  ulFile = open( pszMIMETypesFileName, O_RDONLY | O_BINARY, S_IREAD);
  if ( ulFile == -1 )
  {
    WARNING( NET_MESSAGE_CANTOPENFILE, pszMIMETypesFileName, NULL, NULL );
    return FALSE;
  }

  ulBufPos = 0;
  while( 1 )
  {
    ulCount = (sizeof(acBuf) - 1) - ulBufPos;
    if ( ulCount != 0 )
    {
      ulActual = read( ulFile, &acBuf[ulBufPos], ulCount );
      bEOF = ulCount != ulActual;
      ulCount = ulActual;
      ulBufPos += ulCount;
    }

    pcPtr = memchr( &acBuf, '\n', ulBufPos );
    if ( pcPtr == NULL )
    {
      if ( bEOF )
      {
        pcPtr = &acBuf[ulBufPos];
        *pcPtr = '\0';
      }
      else
      {
        if ( ulBufPos == sizeof(acBuf) )
          ulBufPos = 0;
        continue;
      }
    }
    else
    {
      *pcPtr = '\0';
      pcPtr++;
    }
    ulCount = pcPtr - &acBuf;

    if ( ulCount <= 1 && bEOF )
      break;

    if ( acBuf[0] != '\0' && acBuf[0] != '#' )
    {
      if ( !_AddMIMEType( &acBuf ) )
        break;
    }
    ulBufPos -= ulCount;
    memcpy( &acBuf, pcPtr, ulBufPos );
  }

  close( ulFile );

  return TRUE;
}

VOID FileResDone()
{
  ULONG		ulIdx;
  PSZ		pszLastType = NULL;
  PMIMETYPEITEM	pMimeTypeItem;

  for( ulIdx = 0; ulIdx < ulMIMETypesCount; ulIdx++ )
  {
    pMimeTypeItem = &pasMIMETypes[ulIdx];
    if ( pszLastType != pMimeTypeItem->pszType )
    {
      pszLastType = pMimeTypeItem->pszType;
      free( pMimeTypeItem->pszType );
    }
  }

  if ( pasMIMETypes != NULL )
    free( pasMIMETypes );
}

GETMETHODSCALLBACK *FileResMethodsSelector(GETMETHODSCALLBACK *cbNewGetMethods)
{
  GETMETHODSCALLBACK	*cbOld = cbGetMethods;

  cbGetMethods = cbNewGetMethods;
  return cbOld;
}

ULONG FileResOpen(PFILERES *ppsFileRes, PFILEHOST psFileHost, PSZ pszPath,
                  PSZ pszParam, PSZ pszMethod, PREQFIELDS psFields,
                  PCONNINFO psConnInfo)
{
  ULONG		ulRC;
  ULONG		ulDataSize;
  PFILEMETHODS	psMethods;
  time_t	timeFileRes;

  ulDataSize = cbGetMethods( &psMethods, psFileHost, pszPath, pszMethod );
  if ( ulDataSize == (ULONG)(-1) || psMethods == NULL )
  {
    return FILERES_NOT_FOUND;
  }

  *ppsFileRes = malloc( sizeof(FILERES) - 1 + ulDataSize );
  if ( *ppsFileRes == NULL )
  {
    ERROR_NOT_ENOUGHT_MEM();
    return FILERES_INTERNAL_ERROR;
  }

  (*ppsFileRes)->psFileMethods = psMethods;
  ulRC = (*ppsFileRes)->psFileMethods->FmOpen( (PVOID)&(*ppsFileRes)->acData,
		     psFileHost, pszPath, pszParam, pszMethod, psFields,
                     psConnInfo );
  if ( (ulRC & FILERES_ERROR_MASK) != 0 )
  {
    free( *ppsFileRes );
    *ppsFileRes = NULL;
    return ulRC;
  }

  (*ppsFileRes)->psFileRange = NULL;

  if ( ulRC != FILERES_OK )
    return ulRC;

  ulRC = FileResGetTime( *ppsFileRes, &timeFileRes );
  if ( ulRC == FILERES_OK )
  {
    if ( !_TestCond_IfModifiedSince( psFields, timeFileRes ) )
    {
      FileResClose( *ppsFileRes );
      return FILERES_NOT_MODIFED;
    }
    else if ( !_TestCond_IfUnmodifiedSince( psFields, timeFileRes, "If-Unmodified-Since" ) )
    {
      FileResClose( *ppsFileRes );
      return FILERES_PRECONDITION_FAILED;
    }
  }

  if ( _MakeRange( *ppsFileRes, ReqFldGet( psFields, "Range" ) ) != NULL )
  {
    if ( ulRC == FILERES_OK && 
         !_TestCond_IfUnmodifiedSince( psFields, timeFileRes, "If-Range" ) )
    {
      free( (*ppsFileRes)->psFileRange );
      (*ppsFileRes)->psFileRange = NULL;
    }
    else
      return FILERES_PARTIAL_CONTENT;
  }

  return FILERES_OK;
}

ULONG FileResMemOpen(PFILERES *ppsFileRes, ULONG ulLength, PCHAR pcType)
{
  ULONG		ulTypeNameLen = pcType == NULL ? 0 : strlen( pcType );

  *ppsFileRes = malloc( sizeof( struct _FILERESMEM ) + ulLength );

  if ( *ppsFileRes == NULL )
  {
    ERROR_NOT_ENOUGHT_MEM();
    return FILERES_INTERNAL_ERROR;
  }

  ((PFILERESMEM)(*ppsFileRes))->psFileRange = NULL;
  ((PFILERESMEM)(*ppsFileRes))->psFileMethods = &sMemMethods;
  ((PFILERESMEM)(*ppsFileRes))->sData.ulPos = 0;
  ((PFILERESMEM)(*ppsFileRes))->sData.ulLength = 0;
  ((PFILERESMEM)(*ppsFileRes))->sData.ulMaxLength = ulLength;
  if ( ulTypeNameLen != 0 )
  {
    if ( ulTypeNameLen > MAX_TYPE_NAME_LENGTH )
      ulTypeNameLen = MAX_TYPE_NAME_LENGTH;
    memcpy( &((PFILERESMEM)(*ppsFileRes))->sData.acType, pcType, ulTypeNameLen );
  }
  ((PFILERESMEM)(*ppsFileRes))->sData.acType[ulTypeNameLen] = '\0';

  return FILERES_OK;
}

VOID FileResClose(PFILERES psFileRes)
{
  if ( psFileRes->psFileMethods->FmClose )
    psFileRes->psFileMethods->FmClose( &psFileRes->acData );

  if ( psFileRes->psFileRange != NULL )
    free( psFileRes->psFileRange );

  free( psFileRes );
}

ULONG FileResWrite(PFILERES psFileRes, PCHAR pcBuf, ULONG ulBufLen,
                   PULONG pulActual)
{
  ULONG		ulRC;

  if ( psFileRes->psFileMethods->FmWrite == NULL )
    return FILERES_NOT_ALLOWED;

  ulRC = psFileRes->psFileMethods->FmWrite( &psFileRes->acData, pcBuf, ulBufLen,
                                           pulActual );
  return ulRC;
}

ULONG FileResRead(PFILERES psFileRes, PCHAR pcBuf, ULONG ulBufLen,
                   PULONG pulActual)
{
  ULONG		ulRC;

  if ( psFileRes->psFileMethods->FmRead == NULL )
    return FILERES_NO_MORE_DATA;

  ulRC = psFileRes->psFileMethods->FmRead( &psFileRes->acData, pcBuf, ulBufLen,
                                    pulActual );
  return ulRC;
}

ULONG FileResSeek(PFILERES psFileRes, ULLONG ullPos)
{
  ULONG		ulRC;

  if ( psFileRes->psFileMethods->FmSeek == NULL )
    return FILERES_NOT_ALLOWED;

  ulRC = psFileRes->psFileMethods->FmSeek( &psFileRes->acData, ullPos );
  return ulRC;
}

VOID FileResGetFields(PFILERES psFileRes, RESPCODE rcRespCode,
                      PREQFIELDS psRespFields)
{
  time_t	timeNow, timeFileRes;
  ULONG		ulRC;
  CHAR		acBuf[32];

  struct timeb	sTimeB;
  ULLONG	ullLength;

  ulRC = FileResGetTime( psFileRes, &timeFileRes );
  if ( ulRC == FILERES_OK )
  {
    time( &timeNow );
    if ( timeFileRes > timeNow )
      timeFileRes = timeNow - 1;

    ftime( &sTimeB ); 
    timeFileRes += sTimeB.timezone * 60;
    strftime( &acBuf, 32, "%a, %d %b %Y %T GMT", localtime(&timeFileRes) );
    ReqFldSet( psRespFields, "Last-Modified", &acBuf );
  }

  if ( psFileRes->psFileMethods->FmGetFields != NULL )
    psFileRes->psFileMethods->FmGetFields( &psFileRes->acData, rcRespCode,
                                           psRespFields );

  ulRC = FileResGetLength( psFileRes, &ullLength );
  if ( ulRC == FILERES_OK )
  {
    if ( psFileRes->psFileRange != NULL )
    {
      sprintf( &acBuf, "bytes %llu-%llu/%llu",
               psFileRes->psFileRange->ullBegin,
               psFileRes->psFileRange->ullEnd, ullLength );
      ReqFldSet( psRespFields, "Content-Range", &acBuf );
    
      ullLength = psFileRes->psFileRange->ullEnd -
                  psFileRes->psFileRange->ullBegin + 1;

      free( psFileRes->psFileRange );
      psFileRes->psFileRange = NULL;
    }

    ulltoa( ullLength, &acBuf, 10 );
    ReqFldSet( psRespFields, "Content-Length", &acBuf );
  }

  if ( *(PULONG)rcRespCode == 0x20313034 &&			// "401 "
       ReqFldGet( psRespFields, "WWW-Authenticate" ) == NULL )
  {
    ReqFldSet( psRespFields, "WWW-Authenticate", "Basic realm=\"rootRealm\"" );
  }
}

ULONG FileResGetTime(PFILERES psFileRes, time_t *pTime)
{
  ULONG		ulRC;

  if ( psFileRes->psFileMethods->FmGetTime == NULL )
    return FILERES_NOT_ALLOWED;

  ulRC = psFileRes->psFileMethods->FmGetTime( &psFileRes->acData, pTime );
  return ulRC;
}

ULONG FileResGetLength(PFILERES psFileRes, PULLONG pullLength)
{
  ULONG		ulRC;

  if ( psFileRes->psFileMethods->FmGetLength == NULL )
    return FILERES_NOT_ALLOWED;

  ulRC = psFileRes->psFileMethods->FmGetLength( &psFileRes->acData, pullLength );
  return ulRC;
}


// --- Utils ---

ULONG FileResURLDecode(PCHAR pcDest, PCHAR pcScr, ULONG ulLength)
{
  ULONG		ulIdx;
  ULONG		ulN1, ulN2;
  PCHAR		pcPtrDest = pcDest;
  CHAR		cCurrent;

  if ( pcDest == NULL || pcScr == NULL || ulLength == 0 )
    return 0;

  for( ulIdx = 0; ulIdx < ulLength; ulIdx++ )
  {
    cCurrent = pcScr[ulIdx];
    switch( cCurrent )
    {
      case '%':
        if ( ulIdx + 2 >= ulLength )
          return 0;

        if ( (ulN1 = _hex(pcScr[ulIdx+1])) == (ULONG)(-1) ||
             (ulN2 = _hex(pcScr[ulIdx+2])) == (ULONG)(-1) )
          return 0;

        *pcPtrDest = (CHAR)(ulN1 << 4) + (CHAR)(ulN2);
        pcPtrDest++;
        ulIdx += 2;
        break;
      case '+':
        *pcPtrDest++ = ' ';
        break;
      case '#':
        ulIdx = ulLength;
        break;
      case 0:
        return 0;
      default:
        *pcPtrDest++ = cCurrent;
        break;
    }
  }

  return pcPtrDest - pcDest;
}

PSZ FileResURIParam(PSZ *ppszParamStr, PSZ *ppszValue)
{
  PSZ		pszParamStr = *ppszParamStr;
  PCHAR		pcPtr;
  PCHAR		pcSpliter;
  ULONG		ulLength;

  if ( pszParamStr == NULL || *pszParamStr == '\0' )
    return NULL;

  while( *pszParamStr == '&' )
    pszParamStr++;

  pcPtr = strchr( pszParamStr, '&' );
  if ( pcPtr == NULL )
    pcPtr = strchr( pszParamStr, '\0' );

  pcSpliter = memchr( pszParamStr, '=', pcPtr - pszParamStr );
  if ( pcSpliter == NULL )
    return NULL;
  if ( pcSpliter != pszParamStr )
    *pcSpliter = '\0';

  pcSpliter++;
  ulLength = FileResURLDecode( pcSpliter, pcSpliter, pcPtr - pcSpliter );
  if ( *pcPtr == '&' )
    pcPtr++;
  pcSpliter[ulLength] = '\0';
  *ppszValue = pcSpliter;

  *ppszParamStr = pcPtr;
  return pszParamStr;
}

ULONG FileResBase64Enc(PCHAR pcBuf, ULONG ulBufLen, PSZ pszStr)
{
  ULONG		ulLength, ulEncodedLength;
  ULONG		ulLeft, ulBitqueue, i = 0, j = 0;

  ulLength = strlen( pszStr );
  if ( ulLength == 0 )
    return 0;

  ulEncodedLength = B64ENCODEDLENGTH( ulLength );
  if ( ulEncodedLength < ulBufLen )
    return 0;

  while( i < ulLength )
  {
    ulLeft = ulLength - i;

    if ( ulLeft > 2 )
    {
      ulBitqueue = pszStr[i++];
      ulBitqueue = (ulBitqueue << 8) + pszStr[i++];
      ulBitqueue = (ulBitqueue << 8) + pszStr[i++];
			
      pcBuf[j++] = alphabet[(ulBitqueue & 0xFC0000) >> 18];
      pcBuf[j++] = alphabet[(ulBitqueue & 0x3F000) >> 12];
      pcBuf[j++] = alphabet[(ulBitqueue & 0xFC0) >> 6];
      pcBuf[j++] = alphabet[ulBitqueue & 0x3F];
    }
    else if ( ulLeft == 2 )
    {
      ulBitqueue = pszStr[i++];
      ulBitqueue = (ulBitqueue << 8) + pszStr[i++];
      ulBitqueue <<= 8;

      pcBuf[j++] = alphabet[(ulBitqueue & 0xFC0000) >> 18];
      pcBuf[j++] = alphabet[(ulBitqueue & 0x3F000) >> 12];
      pcBuf[j++] = alphabet[(ulBitqueue & 0xFC0) >> 6];
      pcBuf[j++] = '=';			
    }
    else
    {
      ulBitqueue = pszStr[i++];
      ulBitqueue <<= 16;

      pcBuf[j++] = alphabet[(ulBitqueue & 0xFC0000) >> 18];
      pcBuf[j++] = alphabet[(ulBitqueue & 0x3F000) >> 12];
      pcBuf[j++] = '=';
      pcBuf[j++] = '=';
    }
  }

  return ulEncodedLength;
}

VOID FileResBase64Dec(PCHAR pcBuf, ULONG ulBufLen, PSZ pszB64Str)
{
  ULONG		ulValue;
  ULONG		ulShift = 0;
  ULONG		ulAccum = 0;

  if ( pszB64Str != NULL && ulBufLen != 0 )
    while( *pszB64Str != '\0' )
    {
      ulValue = lExtBase64Decode[ *(pszB64Str++) ];

      if ( ulValue > 63 )
        break;

      ulAccum <<= 6;
      ulShift += 6;
      ulAccum |= ulValue;
      if ( ulShift >= 8 )
      {
        ulShift -= 8;
        ulValue = ulAccum >> ulShift;
        *(pcBuf++) = (CHAR)ulValue;

        ulBufLen--;
        if ( ulBufLen == 0 )
          return;
      }
    }

  *pcBuf = '\0';
}

PSZ FileResGetBasicAuthFld(PREQFIELDS psFields, PCHAR pcBuf, ULONG ulBufLen,
                           PSZ*	ppszPassword)
{
  PSZ	pszAuthField = ReqFldGet( psFields, "Authorization" );

  if ( pszAuthField == NULL || (memicmp( pszAuthField, "Basic ", 6 ) != 0) )
    return NULL;

  pszAuthField += 6;
  while( *pszAuthField == ' ' )
    pszAuthField++;

  ulBufLen--;
  FileResBase64Dec( pcBuf, ulBufLen, pszAuthField );
  pcBuf[ulBufLen] = '\0';

  if ( (pszAuthField = strchr( pcBuf, ':' )) == NULL )
    return NULL;

  *pszAuthField = '\0';
  *ppszPassword = pszAuthField+1;
  return pcBuf;
}

BOOL FileResTestContentType(PREQFIELDS psFields, PSZ pszType)
{
  PSZ		pszTypeField = ReqFldGet( psFields, "Content-Type" );
  ULONG		ulLength = strlen(pszType);
  CHAR		cNextChar;

  if ( pszTypeField == NULL ||
       strnicmp( pszTypeField, pszType, ulLength ) != 0 )
    return FALSE;

  cNextChar = pszTypeField[ulLength];
  return cNextChar == '\0' || cNextChar == ';' || cNextChar == ' ';
}

time_t FileResParseTime(PCHAR pcTime)
{
  struct tm	tm;
  ULONG		ulIdx;
  LONG		lOffsSub;
  struct timeb	time_b;

  if ( strlen( pcTime ) < 20 )
    return 0;

  pcTime = strchr( pcTime, ',' );
  if ( pcTime == NULL )
    return 0;

  tm.tm_mday = (short int)strtol( ++pcTime, &pcTime, 10 );
  skip_space( pcTime );
  if ( *pcTime != '\0' )
  {
    tm.tm_mon = 0;
    for( ulIdx = 0; ulIdx < 12; ulIdx++ )
      if ( strnicmp( pcTime, months[ulIdx], 3 ) == 0 )
      {
        tm.tm_mon = ulIdx;
        break;
      }
    pcTime += 3;
  }

  tm.tm_year = strtol( pcTime, &pcTime, 10 ) - 1900;
  tm.tm_hour = strtol( ++pcTime, &pcTime, 10 );
  tm.tm_min = strtol( ++pcTime, &pcTime, 10 );
  if ( *pcTime == ':' )
    tm.tm_sec = strtol( ++pcTime, &pcTime, 10 );
  else
    tm.tm_sec = 0;

  skip_space( pcTime );
  if ( *pcTime != '\0' && strnicmp( pcTime, "GMT", 3 ) != 0 )
  {
    if ( *pcTime == '-' )
    {
      lOffsSub = -1;
      pcTime++;
    }
    else
    {
      lOffsSub = +1;
      if ( *pcTime == '+' )
        pcTime++;
    }

    ulIdx = *((PULONG)pcTime) & 0x0000FFFF;
    tm.tm_hour -= ( lOffsSub * strtol( (PUCHAR)&ulIdx, NULL, 10 ) );
    tm.tm_min -= ( lOffsSub * strtol( (pcTime+2), NULL, 10 ) );
  }

  ftime( &time_b ); 

  tm.tm_hour -= ( time_b.timezone / 60 );
  tm.tm_min -= ( time_b.timezone % 60 );
  tm.tm_isdst = -1;

  return mktime( &tm );
}
