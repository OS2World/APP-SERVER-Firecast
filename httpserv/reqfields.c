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
#include <stdlib.h>
#include <time.h>
#include <sys\timeb.h>
#include "httpserv\reqfields.h"

static PREQFIELD _FindField( PREQFIELD psReqField, PCHAR pcName,
                             ULONG ulNameLen )
{
  while( psReqField != NULL )
  {
    if ( psReqField->ulNameLen == ulNameLen && 
         memicmp( &psReqField->acData, pcName, ulNameLen ) == 0 )
    {
      return psReqField;
    }
    psReqField = psReqField->psNext;
  }

  return NULL;
}

static BOOL _AddFlield(PREQFIELDS psReqFields, PCHAR pcName,
                       ULONG ulNameLen, PCHAR pcValue, ULONG ulValueLen)
{
  PREQFIELD	psField;
  PREQFIELD	psNewField;

  if ( pcName == NULL || *pcName == '\0' )
    return FALSE;

  if ( ulValueLen == 0 )
  {
    ReqFldDelete( psReqFields, pcName );
    return TRUE;
  }

  psField = _FindField( psReqFields->psFieldList, pcName, ulNameLen );

  if ( psField != NULL && psField->ulValueMaxLen >= ulValueLen )
  {
    psNewField = psField;
  }
  else
  {
    psNewField = malloc( sizeof(REQFIELD) + ulNameLen + ulValueLen );
    if ( psNewField == NULL )
      return FALSE;

    memcpy( &psNewField->acData, pcName, ulNameLen );
    psNewField->ulNameLen = ulNameLen;

    psNewField->ulValueMaxLen = ulValueLen;

    if ( psField == NULL )
    {
      psNewField->psNext = NULL;
      psNewField->ppsSelf = psReqFields->ppsLast;
      *psReqFields->ppsLast = psNewField;
      psReqFields->ppsLast = &psNewField->psNext;
    }
    else
    {
      psNewField->ppsSelf = psField->ppsSelf;
      psNewField->psNext = psField->psNext;
      *psNewField->ppsSelf = psNewField;
      if ( psNewField->psNext != NULL )
        psNewField->psNext->ppsSelf = &psNewField->psNext;
      else
        psReqFields->ppsLast = &psNewField->psNext;

      free( psField );
    }
  }

  memcpy( &psNewField->acData[ulNameLen], pcValue, ulValueLen );
  psNewField->acData[ulNameLen+ulValueLen] = '\0';

  return TRUE;
}

static BOOL _AddFlieldNew(PREQFIELDS psReqFields, PCHAR pcName,
                          ULONG ulNameLen, PCHAR pcValue, ULONG ulValueLen)
{
  PREQFIELD	psField;
  PREQFIELD	psNewField;


  psField = _FindField( psReqFields->psFieldList, pcName, ulNameLen );

  if ( psField != NULL )
  {
    return TRUE;
  }

  psNewField = malloc( sizeof(REQFIELD) + ulNameLen + ulValueLen );
  if ( psNewField == NULL )
    return FALSE;

  memcpy( &psNewField->acData, pcName, ulNameLen );
  memcpy( &psNewField->acData[ulNameLen], pcValue, ulValueLen );
  psNewField->acData[ulNameLen+ulValueLen] = '\0';

  psNewField->ulNameLen = ulNameLen;
  psNewField->ulValueMaxLen = ulValueLen;

  psNewField->psNext = NULL;
  psNewField->ppsSelf = psReqFields->ppsLast;
  *psReqFields->ppsLast = psNewField;
  psReqFields->ppsLast = &psNewField->psNext;

  return TRUE;
}



VOID ReqFldClear(PREQFIELDS psReqFields)
{
  PREQFIELD	psField = psReqFields->psFieldList;
  PREQFIELD	psFreeField;

  while( psField != NULL )
  {
    psFreeField = psField;
    psField = psField->psNext;

    free( psFreeField );
  }
  psReqFields->psFieldList = NULL;
  psReqFields->ppsLast = &psReqFields->psFieldList;
}

VOID ReqFldDone(PREQFIELDS psReqFields)
{
  ReqFldClear( psReqFields );
}

BOOL ReqFldSetString(PREQFIELDS psReqFields, PCHAR pcFieldString,
                     ULONG ulStrLen)
{
  ULONG		ulNameLen;
  PCHAR		pcValue;
  PCHAR		pcPtr;

  if ( pcFieldString == NULL || ulStrLen == 0 )
    return FALSE;

  pcPtr = memchr( pcFieldString, ':', ulStrLen );
  if ( pcPtr == NULL || pcPtr == pcFieldString )
    return FALSE;

  ulNameLen = pcPtr - pcFieldString;

  do {
    pcPtr++;
  }
  while( *pcPtr == ' ' );
  pcValue = pcPtr;

  pcPtr = pcFieldString + ulStrLen;
  if ( pcValue == pcPtr )
    return TRUE;
  while( *(pcPtr-1) == ' ' )
    pcPtr--;

  return _AddFlield( psReqFields, pcFieldString, ulNameLen,
                     pcValue, pcPtr - pcValue );
}

PCHAR ReqFldGet(PREQFIELDS psReqFields, PSZ pszFieldName)
{
  PREQFIELD	psField;

  if ( pszFieldName == NULL )
    return NULL;

  psField = _FindField( psReqFields->psFieldList, 
                        pszFieldName, strlen(pszFieldName) );

  return psField == NULL ? NULL : &psField->acData[ psField->ulNameLen ];
}

VOID ReqFldValid(PREQFIELDS psReqFields, PSZ *ppszNames, ULONG ulNamesCount)
{
  PREQFIELD	psField = psReqFields->psFieldList;
  PREQFIELD	psNextField;
  BOOL		bFound;
  ULONG		ulIdx;
  PCHAR		pcName;
  ULONG		ulNameLen;

  while( psField != NULL )
  {
    bFound = FALSE;
    for( ulIdx = 0; ulIdx < ulNamesCount; ulIdx++ )
    {
      pcName = ppszNames[ulIdx];
      ulNameLen = strlen( pcName );
      if ( memicmp( &psField->acData, pcName, ulNameLen ) == 0 )
      {
        bFound = TRUE;
        break;
      }
    }

    psNextField = psField->psNext;

    if ( !bFound )
    {
      *psField->ppsSelf = psNextField;
      if ( psNextField != NULL )
        psNextField->ppsSelf = psField->ppsSelf;
      else
        psReqFields->ppsLast = psField->ppsSelf;

      free( psField );
    }

    psField = psNextField;
  }
}

VOID ReqFldToString(PREQFIELDS psReqFields, PCHAR pcBuf, ULONG ulBufLen,
                    PULONG pulActual)
{
  PREQFIELD	psField = psReqFields->psFieldList;
  ULONG		ulDataLen;
  ULONG		ulStringLen;
  ULONG		ulActual = 0;
  ULONG		ulNameLen;
  PSZ		pszValue;
  ULONG		ulValueLen;

  while( psField != NULL )
  {
    ulDataLen = strlen( &psField->acData );
    ulStringLen = ulDataLen + 4; // ": " + "\r\n" = 4
    if ( ulBufLen >= ulStringLen )
    {
      ulActual += ulStringLen;
      ulBufLen -= ulStringLen;

      ulNameLen = psField->ulNameLen;
      memcpy( pcBuf, &psField->acData, ulNameLen );
      pcBuf += ulNameLen;
      *(pcBuf++) = ':';
      *(pcBuf++) = ' ';

      pszValue = &psField->acData[ ulNameLen ];
      ulValueLen = ulDataLen - ulNameLen;
      memcpy( pcBuf, pszValue, ulValueLen );
      pcBuf += ulValueLen;
      *(pcBuf++) = '\r';
      *(pcBuf++) = '\n';
    }
    psField = psField->psNext;
  }

  if ( pulActual != NULL )
    *pulActual = ulActual;
}

BOOL ReqFldSet(PREQFIELDS psReqFields, PCHAR pcName, PCHAR pcValue)
{
  if ( pcName == NULL )
    return FALSE;

  return _AddFlield( psReqFields, pcName, strlen(pcName),
                     pcValue, pcValue == NULL ? 0 : strlen(pcValue) );
}

BOOL ReqFldSet2(PREQFIELDS psReqFields, PCHAR pcName, PCHAR pcValue,
                ULONG ulValLen)
{
  if ( pcName == NULL )
    return FALSE;

  return _AddFlield( psReqFields, pcName, strlen(pcName),
                     pcValue, ulValLen );
}

VOID ReqFldDelete(PREQFIELDS psReqFields, PSZ pszFieldName)
{
  PREQFIELD	psField;

  psField = _FindField( psReqFields->psFieldList, 
                        pszFieldName, strlen(pszFieldName) );
  if ( psField )
  {
    *psField->ppsSelf = psField->psNext;

    if ( psField->psNext )
    {
      psField->psNext->ppsSelf = psField->ppsSelf;
    }
    else
    {
      psReqFields->ppsLast = psField->ppsSelf;
    }

    free( psField );
  }
}

BOOL ReqFldCopy(PREQFIELDS psDestFields, PREQFIELDS psSrcFields)
{
  PREQFIELD	psField = psSrcFields->psFieldList;
  PCHAR		pcValue;

  while( psField != NULL )
  {
    pcValue = &psField->acData[psField->ulNameLen];
    if ( !_AddFlield( psDestFields, &psField->acData, psField->ulNameLen,
                pcValue, strlen(pcValue) ) )
    {
      return FALSE;
    }

    psField = psField->psNext;
  }

  return TRUE;
}

BOOL ReqFldCopyNew(PREQFIELDS psDestFields, PREQFIELDS psSrcFields)
{
  PREQFIELD	psField = psSrcFields->psFieldList;
  PCHAR		pcValue;

  while( psField != NULL )
  {
    pcValue = &psField->acData[psField->ulNameLen];
    if ( !_AddFlieldNew( psDestFields, &psField->acData, psField->ulNameLen,
                pcValue, strlen(pcValue) ) )
    {
      return FALSE;
    }

    psField = psField->psNext;
  }

  return TRUE;
}

VOID ReqFldSetCurrentDate(PREQFIELDS psReqField, PSZ pszName)
{
  ULONG		ulSec;
  CHAR		cBuf[32];
  struct timeb	sTimeB;

#ifdef __OS2__
  DosQuerySysInfo( QSV_TIME_LOW, QSV_TIME_LOW, &ulSec, sizeof( ULONG ) );
#else
  time( &ulSec );
#endif
  ftime( &sTimeB ); 

  ulSec += sTimeB.timezone * 60;
  strftime( &cBuf, 32, "%a, %d %b %Y %T GMT", gmtime(&ulSec) );
  ReqFldSet( psReqField, pszName, cBuf );
}
