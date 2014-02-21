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
#define __STDC_WANT_LIB_EXT1__ 1 
#include <time.h>
#include "format.h"
#include "ssmessages.h"

#define NET_MODULE	SS_MODULE_FORMAT

#define TYPE_MP3	"audio/mpeg"
#define TYPE_OGG	"application/ogg"

#define MP3_DEFAULT_METAINT	8192
#define MP3_MIN_METAINT		2048
// METADATA_ITEMS = METADATA_HISTORY_ITEMS + 1 for thread-safe stuff
#define METADATA_ITEMS          (METADATA_HISTORY_ITEMS+1)
#define FREEBUF_MAX_NUMB	5

typedef VOID FMINIT(PFORMAT psFormat);
typedef VOID FMDONE(PFORMAT psFormat);
typedef PFORMATCLIENT FMCLIENTNEW(PFORMAT psFormat, PREQFIELDS psFields);
typedef VOID FMCLIENTDESTROY(PFORMAT psFormat, PFORMATCLIENT psFormatClient);
typedef BOOL FMWRITE(PFORMAT pFormat, PCHAR pcBuf, ULONG ulBufLen);
typedef BOOL FMREAD(PFORMAT pFormat, PFORMATCLIENT psFormatClient, PCHAR pcBuf, PULONG pulBufLen);
typedef VOID FMCLIENTGETFIELDS(PFORMATCLIENT psFormatClient, PREQFIELDS psFields);
typedef VOID FMRESET(PFORMAT psFormat);

typedef struct _FORMATMETHODS {
  ULONG			ulDataSize;
  FMINIT		*FmInit;
  FMDONE		*FmDone;
  FMCLIENTNEW		*FmClientNew;
  FMCLIENTDESTROY	*FmClientDestroy;
  FMWRITE		*FmWrite;
  FMREAD		*FmRead;
  FMCLIENTGETFIELDS	*FmClientGetFields;
  FMRESET		*FmReset;
} FORMATMETHODS, *PFORMATMETHODS;


// FORMAT-object (type independed part)

typedef struct _BUFFER {
  PCHAR		pcData;
  ULONG		ulUsed;
} BUFFER, *PBUFFER;

typedef struct _META {
  time_t	tTime;
  ULONG		ulMaxLength;
  CHAR		acData[1];
} META, *PMETA;

typedef struct _FORMAT {
  BUFFER		asBuffers[32];
  ULONG			ulBufLastPage;
  ULONG			ulBufPos;
  ULONG			ulSyncPos;
  HMTX			hmtxBuffers;

  PMETA			apsMeta[METADATA_ITEMS];
  ULONG			ulMetaIdx;
  HMTX			hmtxMeta;

  PFORMATMETHODS	psMethods;
  CHAR			acFormatData[1];
} FORMAT;


// MP3-format additional data

typedef struct _MP3META {
  ULONG		ulUsed;
  ULONG		ulId;
  ULONG		ulLength;
  CHAR		acData[1];
} MP3META, *PMP3META;

typedef struct _FORMATDATA_MP3 {
  PMP3META	psMP3Meta;
  HMTX		hmtxMP3Meta;
} FORMATDATA_MP3, *PFORMATDATA_MP3;

// MP3-format client's block

typedef struct _FORMATCLIENT_MP3 {
  ULONG		ulBufPos;
  ULONG		ulMetaInterval;
  ULONG		ulMetaCount;
  PMP3META	psMP3Meta;
  ULONG		ulLastMetaId;
} FORMATCLIENT_MP3, *PFORMATCLIENT_MP3;

// MP3-format methods

static VOID _MP3Init(PFORMAT psFormat);
static VOID _MP3Done(PFORMAT psFormat);
static PFORMATCLIENT _MP3ClientNew(PFORMAT psFormat, PREQFIELDS psFields);
static VOID _MP3ClientDestroy(PFORMAT psFormat, PFORMATCLIENT psFormatClient);
static BOOL _MP3Write(PFORMAT psFormat, PCHAR pcBuf, ULONG ulBufLen);
static BOOL _MP3Read(PFORMAT psFormat, PFORMATCLIENT psFormatClient, PCHAR pcBuf, PULONG pulBufLen);
static VOID _MP3ClientGetFields(PFORMATCLIENT psFormatClient, PREQFIELDS psFields);

static FORMATMETHODS	sMP3FormatMethods =
{
  sizeof(FORMATDATA_MP3),
  _MP3Init, _MP3Done, _MP3ClientNew, _MP3ClientDestroy,
  _MP3Write, _MP3Read, _MP3ClientGetFields, NULL
};


// OGG-format additional data

#define MAX_OGG_LOGICAL_STREAMS_NUMB	32

typedef struct _OGGPAGE {
  ULONG		ulSize;
  PCHAR		pcData;
} OGGPAGE, *POGGPAGE;

typedef struct _OGGSTREAM {
  HMTX			hMtx;
  LONG			lSerialNumber;
  ULONG			ulUsed;
  ULONG			ulHdrPages;
  ULONG			ulPagesCount;
  OGGPAGE		asPages[1];
} OGGSTREAM, *POGGSTREAM;

typedef struct _OGGSTREAMSVECTOR {
  POGGSTREAM	*psStreams;
  ULONG		ulCount;
  ULONG		ulMaxCount;
} OGGSTREAMSVECTOR, *POGGSTREAMSVECTOR;

#pragma pack(1)
typedef struct _OGGPAGEHEADER {
  CHAR		acCapturePattern[4];	// 'OggS'
  BYTE		bHeaderVersion;		// 0x0
  BYTE		bHeaderTypeFlags;	// 0x1 - продолжение пакета, 0x2 - BoS, 0x4 - EoS
  LLONG		llAbsoluteGranulePosition;
  LONG		lPageSerialNumber;	// номер логического потока
  LONG		lPageSequenceNo;	// порядковый номер страницы в логическом потоке
  ULONG		ulPageChecksum;		// CRC код страницы 
  BYTE		bPageSegments;		// число сегментов в странице
  // byte[] 	segment_table, таблица размеров сегментов в странице.
} OGGPAGEHEADER, *POGGPAGEHEADER;
#pragma pack(0)

typedef struct _FORMATDATA_OGG {
  OGGSTREAMSVECTOR	sSVec;
  HMTX			hSVecMtx;

  ULONG			ulBytesNeed;
  ULONG			ulBytesCount;
  CHAR			acPageBuf[0x1B + 255 + (255*255)];
} FORMATDATA_OGG, *PFORMATDATA_OGG;

// OGG-format client's block

typedef struct _FORMATCLIENT_OGG {
  OGGSTREAMSVECTOR	sSVec;
  ULONG			ulHdrPage;
  ULONG			ulHdrPos;
  ULONG			ulReadPos;
} FORMATCLIENT_OGG, *PFORMATCLIENT_OGG;

// OGG-format methods

static VOID _OGGInit(PFORMAT psFormat);
static VOID _OGGDone(PFORMAT psFormat);
static PFORMATCLIENT _OGGClientNew(PFORMAT psFormat, PREQFIELDS psFields);
static VOID _OGGClientDestroy(PFORMAT psFormat, PFORMATCLIENT psFormatClient);
static BOOL _OGGWrite(PFORMAT psFormat, PCHAR pcBuf, ULONG ulBufLen);
static BOOL _OGGRead(PFORMAT psFormat, PFORMATCLIENT psFormatClient, PCHAR pcBuf, PULONG pulBufLen);
static VOID _OGGClientGetFields(PFORMATCLIENT psFormatClient, PREQFIELDS psFields);
static VOID _OGGReset(PFORMAT psFormat);

static FORMATMETHODS	sOGGFormatMethods =
{
  sizeof(FORMATDATA_OGG),
  _OGGInit, _OGGDone, _OGGClientNew, _OGGClientDestroy,
  _OGGWrite, _OGGRead, _OGGClientGetFields, _OGGReset
};


static PCHAR	pFreeBufData = NULL;
static ULONG	ulFreeBufDataCount = 0;
static HMTX	hmtxFreeBufData;


static PCHAR _BufAllocData()
{
  PCHAR		pData;

  SysMutexLock( &hmtxFreeBufData );
  if ( pFreeBufData != NULL )
  {
    pData = pFreeBufData;
    pFreeBufData = *(PCHAR *)(pData);
    ulFreeBufDataCount--;
    SysMutexUnlock( &hmtxFreeBufData );
  }
  else
  {
    SysMutexUnlock( &hmtxFreeBufData );
    pData = malloc( sizeof(PCHAR) + 8192 );
  }

  return pData + sizeof(PCHAR);
}

static VOID _BufFreeData(PCHAR pcData)
{
  BOOL	bStore;

  if ( pcData == NULL )
    return;

  pcData -= sizeof(PCHAR);

  SysMutexLock( &hmtxFreeBufData );
  bStore = ulFreeBufDataCount < FREEBUF_MAX_NUMB;
  if ( bStore )
  {
    *(PCHAR *)(pcData) = pFreeBufData;
    pFreeBufData = pcData;
    ulFreeBufDataCount++;
  }
  SysMutexUnlock( &hmtxFreeBufData );

  if ( !bStore )
    free( pcData );
}

static VOID _BufWrite(PFORMAT psFormat, PCHAR pcBuf, ULONG ulBufLen, ULONG ulSyncPos)
{
  PCHAR		pcData;
  ULONG		ulPage;
  ULONG		ulOffset;
  ULONG		ulLength;

  if ( ulBufLen == 0 )
    return;

  SysMutexLock( &psFormat->hmtxBuffers );
  if ( ulSyncPos != (ULONG)(-1) )
    psFormat->ulSyncPos = psFormat->ulBufPos + ulSyncPos;

  while( ulBufLen > 0 )
  {
    ulPage = (psFormat->ulBufPos >> 13) & 0x1F;
    ulOffset = psFormat->ulBufPos & 0x1FFF;

    if ( ulOffset == 0 )
    {
      if ( psFormat->asBuffers[ulPage].pcData == NULL )
      {
        psFormat->asBuffers[ulPage].pcData = _BufAllocData();
      }
      else
      {
        if ( psFormat->ulSyncPos != (ULONG)(-1) &&
             ( (psFormat->ulSyncPos >> 13) & 0x1F ) == ulPage )
        {
          psFormat->ulSyncPos = (ULONG)(-1);
        }

        psFormat->ulBufLastPage = (ulPage + 1) & 0x1F;
      }
      psFormat->asBuffers[ulPage].ulUsed = 0;

      while( psFormat->ulBufLastPage != ulPage
             && ( psFormat->ulSyncPos == (ULONG)(-1) ||
             ((psFormat->ulSyncPos >> 13) & 0x1F) != psFormat->ulBufLastPage )
             && ( psFormat->asBuffers[psFormat->ulBufLastPage].ulUsed == 0 ) )
      {
        _BufFreeData( psFormat->asBuffers[ psFormat->ulBufLastPage ].pcData );
        psFormat->asBuffers[ psFormat->ulBufLastPage ].pcData = NULL;

        psFormat->ulBufLastPage++;
        psFormat->ulBufLastPage &= 0x1F;
      }
    }

    pcData = psFormat->asBuffers[ulPage].pcData;

    ulLength = min( ulBufLen, 8192 - ulOffset );
    memcpy( &pcData[ulOffset], pcBuf, ulLength );

    pcBuf += ulLength;
    ulBufLen -= ulLength;
    psFormat->ulBufPos += ulLength;
  }

  SysMutexUnlock( &psFormat->hmtxBuffers );
}

static BOOL _BufRead(PFORMAT psFormat, PULONG pulBufPos,
                     PCHAR pcBuf, PULONG pulBufLen)
{
  PCHAR		pcData;
  ULONG		ulPage;
  ULONG		ulLeftPage;
  ULONG		ulOffset;
  ULONG		ulLength;
  ULONG		ulBufLen = *pulBufLen;

  if ( *pulBufPos == (ULONG)(-1) )
  {
    *pulBufLen = 0;

    SysMutexLock( &psFormat->hmtxBuffers );
    *pulBufPos = psFormat->ulSyncPos;
    if ( *pulBufPos != (ULONG)(-1) )
      psFormat->asBuffers[ (*pulBufPos >> 13) & 0x1F ].ulUsed++;
    SysMutexUnlock( &psFormat->hmtxBuffers );
  }
  else
  {
    SysMutexLock( &psFormat->hmtxBuffers );
    ulLength = (ULONG)(psFormat->ulBufPos - *pulBufPos);
    SysMutexUnlock( &psFormat->hmtxBuffers );

    if ( ulLength > (31*8192) )
      return FALSE;

    if ( ulLength > ulBufLen )
    {
      ulLeftPage = (*pulBufPos >> 13) & 0x1F;
      ulPage = ulLeftPage;

      while( ulBufLen > 0 )
      {
        ulOffset = *pulBufPos & 0x1FFF;
        pcData = psFormat->asBuffers[ulPage].pcData;

        ulLength = min( ulBufLen, 8192 - ulOffset );
        memcpy( pcBuf, &pcData[ulOffset], ulLength );

        ulBufLen -= ulLength;
        *pulBufPos += ulLength;
        ulPage = (*pulBufPos >> 13) & 0x1F;

        pcBuf += ulLength;
      }

      if ( ulPage != ulLeftPage )
      {
        SysMutexLock( &psFormat->hmtxBuffers );
        psFormat->asBuffers[ ulLeftPage ].ulUsed--;
        psFormat->asBuffers[ ulPage ].ulUsed++;
        SysMutexUnlock( &psFormat->hmtxBuffers );
      }
    }
    else
      *pulBufLen = 0;
  }

  return TRUE;
}

static ULONG _BufUseSyncPos(PFORMAT psFormat)
{
  ULONG		ulSyncPos;

  SysMutexLock( &psFormat->hmtxBuffers );
  ulSyncPos = psFormat->ulSyncPos;
  if ( ulSyncPos != (ULONG)(-1) )
  {
    psFormat->asBuffers[ (ulSyncPos >> 13) & 0x1F ].ulUsed++;
  }
  SysMutexUnlock( &psFormat->hmtxBuffers );

  return ulSyncPos;
}

static VOID _BufUnuse(PFORMAT psFormat, ULONG ulBufPos)
{
  ULONG		ulPage;

  SysMutexLock( &psFormat->hmtxBuffers );
  if ( (psFormat->ulBufPos - ulBufPos) <= (31*8192) )
  {
    ulPage = (ulBufPos >> 13) & 0x1F;
    if ( psFormat->asBuffers[ulPage].ulUsed > 0 )
      psFormat->asBuffers[ulPage].ulUsed--;
  }
  SysMutexUnlock( &psFormat->hmtxBuffers );
}

/********************************************************************/
/*                                                                  */
/*                      MP3-specific routines                       */
/*                                                                  */
/********************************************************************/

static PMP3META __MP3MetaNew(PFORMAT psFormat)
{
  PMP3META		psMP3Meta;
  PMETA			psMeta;
  ULONG			ulStrLength;
  ULONG			ulBlockLength;
  ULONG			ulIdx;

  SysMutexLock( &psFormat->hmtxMeta );
  ulIdx = psFormat->ulMetaIdx;
  SysMutexUnlock( &psFormat->hmtxMeta );

  SysMutexLock( &((PFORMATDATA_MP3)(psFormat->acFormatData))->hmtxMP3Meta );

  psMP3Meta = ((PFORMATDATA_MP3)(psFormat->acFormatData))->psMP3Meta;
  if ( psMP3Meta != NULL )
  {
    if ( psMP3Meta->ulId == ulIdx )
    {
      psMP3Meta->ulUsed++;
      SysMutexUnlock( &((PFORMATDATA_MP3)(psFormat->acFormatData))->hmtxMP3Meta );
      return psMP3Meta;
    }
    else
      __MP3MetaDestroy( psFormat, psMP3Meta );
  }

  psMeta = psFormat->apsMeta[ulIdx == 0 ? METADATA_ITEMS-1 : ulIdx-1];

  if ( psMeta == NULL )
  {
    ulStrLength = 0;
  }
  else
  {
    ulStrLength = strlen( &psMeta->acData );
    if ( ulStrLength > (4096 - 16) )
      ulStrLength = 4096 - 16;
  }
  ulBlockLength = ((ulStrLength + 16 - 1) & 0xF0) + 0x10 + 1;

  psMP3Meta = malloc( sizeof(MP3META) - 1 + ulBlockLength );
  psMP3Meta->ulUsed = 2;
  psMP3Meta->ulId = psFormat->ulMetaIdx;
  psMP3Meta->ulLength = ulBlockLength;

  psMP3Meta->acData[0] = (ulBlockLength & 0xF0) >> 4;
  memcpy( &psMP3Meta->acData[1], "StreamTitle='", 13 );
  memcpy( &psMP3Meta->acData[14], &psMeta->acData, ulStrLength );
  memcpy( &psMP3Meta->acData[14+ulStrLength], "';", 2 );
  memset( &psMP3Meta->acData[16+ulStrLength], '\0',
          ulBlockLength-16-ulStrLength );

  ((PFORMATDATA_MP3)(psFormat->acFormatData))->psMP3Meta = psMP3Meta;

  SysMutexUnlock( &((PFORMATDATA_MP3)(psFormat->acFormatData))->hmtxMP3Meta );

  return psMP3Meta;
}

static VOID __MP3MetaDestroy(PFORMAT psFormat, PMP3META psMP3Meta)
{
  SysMutexLock( &((PFORMATDATA_MP3)(psFormat->acFormatData))->hmtxMP3Meta );
  if ( psMP3Meta->ulUsed > 0 )
    psMP3Meta->ulUsed--;

  if ( psMP3Meta->ulUsed == 0 )
  {
    if ( ((PFORMATDATA_MP3)(psFormat->acFormatData))->psMP3Meta == psMP3Meta )
      ((PFORMATDATA_MP3)(psFormat->acFormatData))->psMP3Meta = NULL;
    free( psMP3Meta );
  }
  SysMutexUnlock( &((PFORMATDATA_MP3)(psFormat->acFormatData))->hmtxMP3Meta );
}


static VOID _MP3Init(PFORMAT psFormat)
{
  SysMutexCreate( &((PFORMATDATA_MP3)(psFormat->acFormatData))->hmtxMP3Meta );
}

static VOID _MP3Done(PFORMAT psFormat)
{
  SysMutexDestroy( &((PFORMATDATA_MP3)(psFormat->acFormatData))->hmtxMP3Meta );
}

static PFORMATCLIENT _MP3ClientNew(PFORMAT psFormat, PREQFIELDS psFields)
{
  PFORMATCLIENT_MP3	psFormatClient;
  PCHAR			pcIcyMetaInt;
  LONG			lIcyMetaInt;

  psFormatClient = (PFORMATCLIENT_MP3)calloc( 1, sizeof(FORMATCLIENT_MP3) );
  if ( psFormatClient == NULL )
  {
    ERROR_NOT_ENOUGHT_MEM();
    return NULL;
  }

  pcIcyMetaInt = ReqFldGet( psFields, "icy-metadata" );
  if ( pcIcyMetaInt != NULL && *pcIcyMetaInt == '1' )
  {
    if ( (pcIcyMetaInt = ReqFldGet( psFields, "icy-metaint" )) != NULL )
    {
      lIcyMetaInt = atol( pcIcyMetaInt );
      if ( lIcyMetaInt < MP3_MIN_METAINT )
        lIcyMetaInt = MP3_MIN_METAINT;
    }
    else
      lIcyMetaInt = MP3_DEFAULT_METAINT;

    psFormatClient->ulMetaInterval = lIcyMetaInt;
  }

  psFormatClient->ulBufPos = (ULONG)(-1);
  psFormatClient->ulLastMetaId = (ULONG)(-1);

  return (PFORMATCLIENT)psFormatClient;
}

static VOID _MP3ClientDestroy(PFORMAT psFormat, PFORMATCLIENT psFormatClient)
{
  PFORMATCLIENT_MP3	psMP3FormatClient = (PFORMATCLIENT_MP3)psFormatClient;

  _BufUnuse( psFormat, psMP3FormatClient->ulBufPos );
  if ( psMP3FormatClient->psMP3Meta != NULL )
    __MP3MetaDestroy( psFormat, psMP3FormatClient->psMP3Meta );

  free( psFormatClient );
}

/*typedef struct _MP3Header {
  int lay;
  int version;
  int error_protection;
  int bitrate_index;
  int sampling_frequency;
  int padding;
  int extension;
  int mode;
  int mode_ext;
  int copyright;
  int original;
  int emphasis;
  int stereo;
} MP3Header;

unsigned int bitrates[3][3][15] =
{
  {
    {0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448},
    {0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384},
    {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320}
  },
  {
    {0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256},
    {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160},
    {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160}
  },
  {
    {0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256},
    {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160},
    {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160}
  }
};

unsigned int s_freq[3][4] =
{
    {44100, 48000, 32000, 0},
    {22050, 24000, 16000, 0},
    {11025, 8000, 8000, 0}
};

unsigned int slotsf[3] = {384, 1152, 1152};

static VOID _TestMP3Header(PCHAR pcBuf)
{
  MP3Header	mh;
  ULONG		ulDataSize;

  if ( pcBuf[0] != 0xFF || (pcBuf[1] & 0xE0) != 0xE0 )
    return;

  switch( (pcBuf[1] >> 3) & 0x3 )
  {
    case 3:
      mh.version = 0;
      break;
    case 2:
      mh.version = 1;
      break;
    case 0:
      mh.version = 2;
      break;
    default:
      mh.version = -1;
      break;
  }

  mh.lay = 4 - ((pcBuf[1] >> 1) & 0x3);
  mh.error_protection = !(pcBuf[1] & 0x1);
  mh.bitrate_index = (pcBuf[2] >> 4) & 0x0F;
  mh.sampling_frequency = (pcBuf[2] >> 2) & 0x3;
  mh.padding = (pcBuf[2] >> 1) & 0x01;
  mh.extension = pcBuf[2] & 0x01;
  mh.mode = (pcBuf[3] >> 6) & 0x3;
  mh.mode_ext = (pcBuf[3] >> 4) & 0x03;
  mh.copyright = (pcBuf[3] >> 3) & 0x01;
  mh.original = (pcBuf[3] >> 2) & 0x1;
  mh.emphasis = pcBuf[3] & 0x3;
  mh.stereo = (mh.mode == 3) ? 1 : 2;

  ulDataSize = s_freq[mh.version][mh.sampling_frequency];
  if ( ulDataSize == 0 )
    ulDataSize = 1;
  else
  {
    ulDataSize = ((slotsf[mh.lay - 1]/8) *
                  bitrates[mh.version][mh.lay - 1][mh.bitrate_index] * 1000)
                  / ulDataSize;

    if ( mh.padding )
      ulDataSize++; // max. 5761
  }

  printf( "ver: %d, bitrate: %d, block length: %d\n",
    mh.version, bitrates[mh.version][mh.lay - 1][mh.bitrate_index],
    ulDataSize );
}*/

static BOOL _MP3Write(PFORMAT psFormat, PCHAR pcBuf, ULONG ulBufLen)
{
/*  PCHAR		pcMark;

  pcMark = memchr( pcBuf, 0xFF, ulBufLen );
  if ( pcMark != NULL )
  {
    if ( ((pcMark - pcBuf) + 4) < ulBufLen )
      _TestMP3Header( pcMark );
  }*/

  _BufWrite( psFormat, pcBuf, ulBufLen, 0 );
  return TRUE;
}

static BOOL _MP3Read(PFORMAT psFormat, PFORMATCLIENT psFormatClient, PCHAR pcBuf, PULONG pulBufLen)
{
  PFORMATCLIENT_MP3	psMP3FormatClient = (PFORMATCLIENT_MP3)psFormatClient;
  ULONG			ulRem;
  BOOL			bRes;
  ULONG			ulCount;

  if ( psMP3FormatClient->ulMetaInterval == 0 )
    return _BufRead( psFormat, &psMP3FormatClient->ulBufPos, pcBuf, pulBufLen );

  ulCount = 0;
  while( ulCount < *pulBufLen )
  {
    if ( psMP3FormatClient->psMP3Meta == NULL )
    {
      ulRem = psMP3FormatClient->ulMetaInterval - psMP3FormatClient->ulMetaCount;
      if ( ulRem == 0 )
      {
        if ( psMP3FormatClient->ulLastMetaId == psFormat->ulMetaIdx )
        {
          *pcBuf = '\0';
          ulCount++;
          pcBuf++;
        }
        else
        {
          psMP3FormatClient->psMP3Meta = __MP3MetaNew( psFormat );
          psMP3FormatClient->ulLastMetaId = psFormat->ulMetaIdx;
        }
        psMP3FormatClient->ulMetaCount = 0;
        continue;
      }
      ulRem = min( ulRem, *pulBufLen - ulCount );

      bRes = _BufRead( psFormat, &psMP3FormatClient->ulBufPos, pcBuf, &ulRem );
      if ( !bRes )
        return FALSE;
      if ( ulRem == 0 )
        break;
    }
    else
    {
      ulRem = psMP3FormatClient->psMP3Meta->ulLength - psMP3FormatClient->ulMetaCount;
      if ( ulRem == 0 )
      {
        __MP3MetaDestroy( psFormat, psMP3FormatClient->psMP3Meta );
        psMP3FormatClient->psMP3Meta = NULL;
        psMP3FormatClient->ulMetaCount = 0;
        continue;
      }
      ulRem = min( ulRem, *pulBufLen - ulCount );
      memcpy( pcBuf,
         &psMP3FormatClient->psMP3Meta->acData[psMP3FormatClient->ulMetaCount],
         ulRem );
    }
    psMP3FormatClient->ulMetaCount += ulRem;
    ulCount += ulRem;
    pcBuf += ulRem;
  }

  *pulBufLen = ulCount;
  return TRUE;
}

static VOID _MP3ClientGetFields(PFORMATCLIENT psFormatClient, PREQFIELDS psFields)
{
  PFORMATCLIENT_MP3	psMP3FormatClient = (PFORMATCLIENT_MP3)psFormatClient;
  CHAR			acBuf[16];

  if ( psMP3FormatClient->ulMetaInterval != 0 )
    ReqFldSet( psFields, "icy-metaint",
               ultoa( psMP3FormatClient->ulMetaInterval, &acBuf, 10 ) );
  ReqFldSet( psFields, "Content-Type", TYPE_MP3 );
}

/********************************************************************/
/*                                                                  */
/*                      OGG-specific routines                       */
/*                                                                  */
/********************************************************************/

static VOID __OGGDestroyStream(POGGSTREAM psStream)
{
  ULONG		ulIdx;

  SysMutexLock( &psStream->hMtx );
  if ( psStream->ulUsed > 0 )
  {
    psStream->ulUsed--;
    if ( psStream->ulUsed != 0 )
    {
      SysMutexUnlock( &psStream->hMtx );
      return;
    }
  }

  SysMutexDestroy( &psStream->hMtx );

  for( ulIdx = 0; ulIdx < psStream->ulPagesCount; ulIdx++ )
    free( psStream->asPages[ulIdx].pcData );

  free( psStream );
}


static BOOL __OGGStreamVecAddFilled(POGGSTREAMSVECTOR psSVec,
                  POGGSTREAM psStream, BOOL bFilledOnly)
{
  POGGSTREAM	*psStreams;

  SysMutexLock( &psStream->hMtx );
  if ( bFilledOnly && ( psStream->ulHdrPages != psStream->ulPagesCount ) )
  {
    SysMutexUnlock( &psStream->hMtx );
    return FALSE;
  }
 
  if ( psSVec->ulCount == psSVec->ulMaxCount )
  {
    psStreams = realloc( psSVec->psStreams,
                  (psSVec->ulMaxCount + 4) * sizeof(POGGSTREAM) );
    if ( psStreams == NULL )
    {
      SysMutexUnlock( &psStream->hMtx );
      free( psStreams );
      return FALSE;
    }

    psSVec->psStreams = psStreams;
    psSVec->ulMaxCount += 4;
  }

  psStream->ulUsed++;
  SysMutexUnlock( &psStream->hMtx );

  psSVec->psStreams[ psSVec->ulCount ] = psStream;
  psSVec->ulCount++;

  return TRUE;
}

static BOOL __OGGStreamVecAdd(POGGSTREAMSVECTOR psSVec, POGGSTREAM psStream)
{
  return __OGGStreamVecAddFilled( psSVec, psStream, FALSE );
}

static VOID __OGGStreamVecDelAll(POGGSTREAMSVECTOR psSVec)
{
  ULONG		ulIdx;

  if ( psSVec->ulCount != 0 )
  {
    for( ulIdx = 0; ulIdx < psSVec->ulCount; ulIdx++ )
      __OGGDestroyStream( psSVec->psStreams[ulIdx] );

    free( psSVec->psStreams );
    psSVec->psStreams = NULL;
    psSVec->ulMaxCount = 0;
    psSVec->ulCount = 0;
  }
}

static VOID __OGGStreamVecDel(POGGSTREAMSVECTOR psSVec, ULONG ulIdx)
{
  __OGGDestroyStream( psSVec->psStreams[ulIdx] );

  psSVec->ulCount--;
  if ( psSVec->ulCount == 0 )
  {
    free( psSVec->psStreams );
    psSVec->psStreams = NULL;
    psSVec->ulMaxCount = 0;
  }
  else
  {
    memcpy( &psSVec->psStreams[ulIdx], &psSVec->psStreams[ulIdx+1], 
            psSVec->ulCount - ulIdx - 1 );
  }
}

static ULONG __OGGStreamVecFind(POGGSTREAMSVECTOR psSVec, LONG lSerialNumber)
{
  ULONG		ulIdx;

  for( ulIdx = 0; ulIdx < psSVec->ulCount; ulIdx++ )
    if ( psSVec->psStreams[ulIdx]->lSerialNumber == lSerialNumber )
    {
      return ulIdx;
    }

  return (ULONG)(-1);
}


static POGGSTREAM __OGGGetStreamOfCurrentPage(PFORMATDATA_OGG psOGGFormatData)
{
  POGGPAGEHEADER	psPageHeader = (POGGPAGEHEADER)&psOGGFormatData->acPageBuf;
  ULONG			ulIdx;

  ulIdx = __OGGStreamVecFind( &psOGGFormatData->sSVec, psPageHeader->lPageSerialNumber );
  if ( ulIdx == (ULONG)(-1) )
    return NULL;

  return psOGGFormatData->sSVec.psStreams[ulIdx];
}

static BOOL __OGGStreamAddPage(PFORMAT psFormat, POGGSTREAM psStream)
{
  PFORMATDATA_OGG	psOGGFormatData = (PFORMATDATA_OGG)(&psFormat->acFormatData);
  ULONG			ulBytesCount;
  PCHAR			pcData;
  POGGPAGE		psPage;
  ULONG			ulIdx;

  if ( psStream->ulPagesCount == psStream->ulHdrPages )
    return FALSE;

  ulBytesCount = psOGGFormatData->ulBytesCount;
  pcData = malloc( ulBytesCount );
  if ( pcData == NULL )
    return FALSE;

  memcpy( pcData, &psOGGFormatData->acPageBuf, ulBytesCount );
  psPage = &psStream->asPages[ psStream->ulPagesCount ];

  psPage->ulSize = ulBytesCount;
  psPage->pcData = pcData;

  SysMutexLock( &psStream->hMtx );
  psStream->ulPagesCount++;

  if ( psStream->ulPagesCount == psStream->ulHdrPages )
    for( ulIdx = 0; ulIdx < psStream->ulPagesCount; ulIdx++ )
    {
      psPage = &psStream->asPages[ulIdx];
      _BufWrite( psFormat, psPage->pcData, psPage->ulSize, (ULONG)(-1) );
    }
  SysMutexUnlock( &psStream->hMtx );

  return TRUE;
}

static VOID __OGGBeginStream(PFORMAT psFormat)
{
  PFORMATDATA_OGG	psOGGFormatData = (PFORMATDATA_OGG)(&psFormat->acFormatData);
  POGGSTREAM		psStream;
  ULONG			ulHdrPages;
  
  if ( __OGGGetStreamOfCurrentPage( psOGGFormatData ) != NULL ||
       psOGGFormatData->sSVec.ulCount == MAX_OGG_LOGICAL_STREAMS_NUMB )
  {
    return;
  }

  ulHdrPages = 3; // must be calculated for different types of logical streams (for vorbis is 3)
  psStream = (POGGSTREAM)malloc( sizeof(OGGSTREAM) - 1 + 
                                 (ulHdrPages * sizeof(OGGPAGE)) );
  if ( psStream == NULL )
    return;

  psStream->lSerialNumber = ((POGGPAGEHEADER)&psOGGFormatData->acPageBuf)->lPageSerialNumber;
  psStream->ulUsed = 0;
  psStream->ulHdrPages = ulHdrPages;
  psStream->ulPagesCount = 0;
  SysMutexCreate( &psStream->hMtx );
 
  SysMutexLock( &psOGGFormatData->hSVecMtx );
  
  if ( __OGGStreamVecAdd( &psOGGFormatData->sSVec, psStream ) )
  {
    __OGGStreamAddPage( psFormat, psStream );
  }
  else
  {
    SysMutexDestroy( &psStream->hMtx );
    free( psStream );
  }

  SysMutexUnlock( &psOGGFormatData->hSVecMtx );
}

static BOOL __OGGEndStream(PFORMATDATA_OGG psOGGFormatData)
{
  ULONG		ulIdx;

  ulIdx = __OGGStreamVecFind( &psOGGFormatData->sSVec,
            ((POGGPAGEHEADER)&psOGGFormatData->acPageBuf)->lPageSerialNumber );
  if ( ulIdx == (ULONG)(-1) )
    return FALSE;

  SysMutexLock( &psOGGFormatData->hSVecMtx );
  __OGGStreamVecDel( &psOGGFormatData->sSVec, ulIdx );
  SysMutexUnlock( &psOGGFormatData->hSVecMtx );

  return TRUE;
}

static BOOL __OGGCommitPage(PFORMAT psFormat)
{
  PFORMATDATA_OGG	psOGGFormatData = (PFORMATDATA_OGG)(&psFormat->acFormatData);
  POGGSTREAM		psStream = __OGGGetStreamOfCurrentPage(psOGGFormatData);

  if ( psStream == NULL )
    return FALSE;

  return !( __OGGStreamAddPage( psFormat, psStream ) );
}


static VOID _OGGInit(PFORMAT psFormat)
{
  PFORMATDATA_OGG	psOGGFormatData = (PFORMATDATA_OGG)(&psFormat->acFormatData);

  SysMutexCreate( &psOGGFormatData->hSVecMtx );
}

static VOID _OGGDone(PFORMAT psFormat)
{
  PFORMATDATA_OGG	psOGGFormatData = (PFORMATDATA_OGG)(&psFormat->acFormatData);

  __OGGStreamVecDelAll( &psOGGFormatData->sSVec );
  SysMutexDestroy( &psOGGFormatData->hSVecMtx );
}

static PFORMATCLIENT _OGGClientNew(PFORMAT psFormat, PREQFIELDS psFields)
{
  PFORMATCLIENT_OGG	psFormatClient;
  PFORMATDATA_OGG	psOGGFormatData = (PFORMATDATA_OGG)(&psFormat->acFormatData);
  ULONG			ulIdx;
  POGGSTREAM		psStream;

  psFormatClient = (PFORMATCLIENT_OGG)calloc( 1, sizeof(FORMATCLIENT_OGG) );
  if ( psFormatClient == NULL )
  {
    ERROR_NOT_ENOUGHT_MEM();
    return NULL;
  }

  psFormatClient->ulReadPos = _BufUseSyncPos( psFormat );

  SysMutexLock( &psOGGFormatData->hSVecMtx );
  for( ulIdx = 0; ulIdx < psOGGFormatData->sSVec.ulCount; ulIdx++ )
  {
    psStream = psOGGFormatData->sSVec.psStreams[ulIdx];
    __OGGStreamVecAddFilled( &psFormatClient->sSVec, psStream, TRUE );
  }
  SysMutexUnlock( &psOGGFormatData->hSVecMtx );

  return (PFORMATCLIENT)psFormatClient;
}

static VOID _OGGClientDestroy(PFORMAT psFormat, PFORMATCLIENT psFormatClient)
{
  PFORMATCLIENT_OGG	psOGGFormatClient = (PFORMATCLIENT_OGG)psFormatClient;

  _BufUnuse( psFormat, psOGGFormatClient->ulReadPos );
  __OGGStreamVecDelAll( &psOGGFormatClient->sSVec );
  free( psFormatClient );
}

static BOOL _OGGWrite(PFORMAT psFormat, PCHAR pcBuf, ULONG ulBufLen)
{
  PFORMATDATA_OGG	psOGGFormatData = (PFORMATDATA_OGG)(&psFormat->acFormatData);
  POGGPAGEHEADER	psPageHeader = (POGGPAGEHEADER)&psOGGFormatData->acPageBuf;
  ULONG			ulLength;
  ULONG			ulIdx;
  BOOL			bNeedStore;

  while( ulBufLen > 0 )
  {
    if ( psOGGFormatData->ulBytesCount == psOGGFormatData->ulBytesNeed )
    do
    {
      if ( psOGGFormatData->ulBytesNeed == 0 )
      {
        psOGGFormatData->ulBytesNeed = sizeof(OGGPAGEHEADER);
        break;
      }
      else if ( psOGGFormatData->ulBytesNeed == sizeof(OGGPAGEHEADER) )
      {
        if ( *(PULONG)(psPageHeader->acCapturePattern) != 'SggO' )
        {
          //printf( "acCapturePattern != 'OggS'\n" );
          return FALSE;
        }

        if ( psPageHeader->bPageSegments != 0 )
        {
          psOGGFormatData->ulBytesNeed = sizeof(OGGPAGEHEADER) +
                                         psPageHeader->bPageSegments;
          break;
        }
      }
      else if ( psOGGFormatData->ulBytesNeed == sizeof(OGGPAGEHEADER) +
                                                psPageHeader->bPageSegments )
      {
        ulLength = 0;
        for( ulIdx = 0; ulIdx < psPageHeader->bPageSegments; ulIdx++ )
          ulLength += psOGGFormatData->acPageBuf[sizeof(OGGPAGEHEADER) + ulIdx];
        if ( ulLength != 0 )
        {
          psOGGFormatData->ulBytesNeed += ulLength;
          break;
        }
      }

      if ( psPageHeader->bHeaderVersion != 0 )
      {
        //printf( "The header version of received page is unknown.\n" );
        return FALSE;
      }

/*        printf("Header len=%d, serial id=%d, N=%d, flags:%u\n",
        psOGGFormatData->ulBytesCount,
        psPageHeader->lPageSerialNumber, psPageHeader->lPageSequenceNo,
        psPageHeader->bHeaderTypeFlags );*/
/*
Need: read info (from second page?), for ex.:

TITLE=Radwind Pt.2
ARTIST=MoozE
ALBUM=Stalker OST

and set metadata
*/

      switch( psPageHeader->bHeaderTypeFlags & (0x02 | 0x04) )
      {
        case 0x02:
          __OGGBeginStream( psFormat );
          bNeedStore = FALSE;
          break;
        case 0x04:
          bNeedStore = __OGGEndStream( psOGGFormatData );
          break;
        case 0x00:
          bNeedStore = __OGGCommitPage( psFormat );
          break;
        default:
          bNeedStore = FALSE;
      }

      if ( bNeedStore )
      {
        _BufWrite( psFormat, &psOGGFormatData->acPageBuf,
          psOGGFormatData->ulBytesCount, psOGGFormatData->ulBytesCount );
      }

      psOGGFormatData->ulBytesNeed = sizeof(OGGPAGEHEADER);
      psOGGFormatData->ulBytesCount = 0;
    }
    while( FALSE );

    ulLength = min( ulBufLen, psOGGFormatData->ulBytesNeed - psOGGFormatData->ulBytesCount );
    memcpy( &psOGGFormatData->acPageBuf[psOGGFormatData->ulBytesCount],
            pcBuf, ulLength );
    psOGGFormatData->ulBytesCount += ulLength;
    pcBuf += ulLength;
    ulBufLen -= ulLength;
  }

  return TRUE;
}

static BOOL _OGGRead(PFORMAT psFormat, PFORMATCLIENT psFormatClient, PCHAR pcBuf, PULONG pulBufLen)
{
  PFORMATCLIENT_OGG	psOGGFormatClient = (PFORMATCLIENT_OGG)psFormatClient;
  POGGSTREAM		psStream;
  POGGPAGE		psPage;
  ULONG			ulBufLen = *pulBufLen;
  ULONG			ulLength;
  BOOL			bRes;

  *pulBufLen = 0;

  while ( psOGGFormatClient->sSVec.ulCount != 0 )
  {
    if ( ulBufLen == 0 )
      return TRUE;

    psStream = psOGGFormatClient->sSVec.psStreams[0];
    if ( psOGGFormatClient->ulHdrPage == psStream->ulPagesCount )
    {
      __OGGStreamVecDel( &psOGGFormatClient->sSVec, 0 );
      psOGGFormatClient->ulHdrPage = 0;
    }
    else
    {
      psPage = &psStream->asPages[psOGGFormatClient->ulHdrPage];

      ulLength = min( psPage->ulSize - psOGGFormatClient->ulHdrPos, ulBufLen );
      memcpy( pcBuf, &psPage->pcData[psOGGFormatClient->ulHdrPos], ulLength );
      pcBuf += ulLength;
      ulBufLen -= ulLength;
      *pulBufLen += ulLength;
      psOGGFormatClient->ulHdrPos += ulLength;
      if ( psOGGFormatClient->ulHdrPos == psPage->ulSize )
      {
        psOGGFormatClient->ulHdrPos = 0;
        psOGGFormatClient->ulHdrPage++;
      }
    }
  }

  bRes = _BufRead( psFormat, &psOGGFormatClient->ulReadPos, pcBuf, &ulBufLen );
  *pulBufLen += ulBufLen;

  return bRes;
}

static VOID _OGGClientGetFields(PFORMATCLIENT psFormatClient, PREQFIELDS psFields)
{
  ReqFldSet( psFields, "Content-Type", TYPE_OGG );
}

static VOID _OGGReset(PFORMAT psFormat)
{
  PFORMATDATA_OGG	psOGGFormatData = (PFORMATDATA_OGG)(&psFormat->acFormatData);

  SysMutexLock( &psOGGFormatData->hSVecMtx );
  __OGGStreamVecDelAll( &psOGGFormatData->sSVec );
  SysMutexUnlock( &psOGGFormatData->hSVecMtx );
  psOGGFormatData->ulBytesNeed = 0;
  psOGGFormatData->ulBytesCount = 0;
}

/********************************************************************/
/*                                                                  */
/*                  Format-independent routines                     */
/*                                                                  */
/********************************************************************/

VOID FormatInit()
{
  SysMutexCreate( &hmtxFreeBufData );
}

VOID FormatDone()
{
  PCHAR		pData;

  while( pFreeBufData != NULL )
  {
    pData = pFreeBufData;
    pFreeBufData = *(PCHAR *)(pData);
    free( pData );
  }

  SysMutexDestroy( &hmtxFreeBufData );
}

PFORMAT FormatNew(PREQFIELDS psFields)
{
  PFORMAT		psFormat;
  PCHAR			pcFldType;
  ULONG			ulLength;
  PFORMATMETHODS	psMethods;

  if ( ( pcFldType = ReqFldGet( psFields, "Content-Type" ) ) == NULL )
    return NULL;
  else
    ulLength = strcspn(pcFldType, " ;");

  if ( strnicmp( pcFldType, TYPE_MP3, ulLength ) == 0 )
  {
    psMethods = &sMP3FormatMethods;
  }
  else if ( strnicmp( pcFldType, TYPE_OGG, ulLength ) == 0 )
  {
    psMethods = &sOGGFormatMethods;
  }
  else
  {
    free( psFormat );
    return NULL;
  }

  psFormat = calloc( 1, sizeof(FORMAT) + psMethods->ulDataSize - 1 );
  if ( psFormat == NULL )
  {
    ERROR_NOT_ENOUGHT_MEM();
    return NULL;
  }

  psFormat->psMethods = psMethods;
  psFormat->ulSyncPos = (ULONG)(-1);
  SysMutexCreate( &psFormat->hmtxBuffers );
  SysMutexCreate( &psFormat->hmtxMeta );

  psMethods->FmInit( psFormat );

  return psFormat;
}

VOID FormatDestroy(PFORMAT psFormat)
{
  ULONG		ulIdx;

  psFormat->psMethods->FmDone( psFormat );

  for( ulIdx = 0; ulIdx < METADATA_ITEMS; ulIdx++ )
    if ( psFormat->apsMeta[ulIdx] != NULL)
      free( psFormat->apsMeta[ulIdx] );

  for( ulIdx = 0; ulIdx < 32; ulIdx++ )
    _BufFreeData( psFormat->asBuffers[ulIdx].pcData );

  SysMutexDestroy( &psFormat->hmtxMeta );
  SysMutexDestroy( &psFormat->hmtxBuffers );
  free( psFormat );
}

PFORMATCLIENT FormatClientNew(PFORMAT psFormat, PREQFIELDS psFields)
{
  return psFormat->psMethods->FmClientNew( psFormat, psFields );
}

VOID FormatClientDestroy(PFORMAT psFormat, PFORMATCLIENT psFormatClient)
{
  psFormat->psMethods->FmClientDestroy( psFormat, psFormatClient );
}

BOOL FormatWrite(PFORMAT psFormat, PCHAR pcBuf, ULONG ulBufLen)
{
  return psFormat->psMethods->FmWrite( psFormat, pcBuf, ulBufLen );
}

BOOL FormatRead(PFORMAT psFormat, PFORMATCLIENT psFormatClient, PCHAR pcBuf, PULONG pulBufLen)
{
  return psFormat->psMethods->FmRead( psFormat, psFormatClient, pcBuf, pulBufLen );
}

VOID FormatClientGetFields(PFORMAT psFormat, PFORMATCLIENT psFormatClient, PREQFIELDS psFields)
{
  psFormat->psMethods->FmClientGetFields( psFormatClient, psFields );
}

BOOL FormatSetMeta(PFORMAT psFormat, PSZ pszMetaData)
{
  ULONG		ulLength = pszMetaData == NULL ? 0 : strlen(pszMetaData);
  PMETA		psMeta;

  SysMutexLock( &psFormat->hmtxMeta );

  psMeta = psFormat->apsMeta[psFormat->ulMetaIdx];
  if (
       ( psMeta != NULL )
     &&
       (
         ( ((ULONG)(psMeta->ulMaxLength - ulLength)) > 16 )
       ||
         ( psMeta->ulMaxLength < ulLength )
       )
     )
  {
    free( psMeta );
    psMeta = NULL;
  }

  if ( psMeta == NULL )
  {
    psMeta = malloc( sizeof(META) + ulLength );
    psFormat->apsMeta[psFormat->ulMetaIdx] = psMeta;
    if ( psMeta == NULL )
    {
      SysMutexUnlock( &psFormat->hmtxMeta );
      ERROR_NOT_ENOUGHT_MEM();
      return FALSE;
    }
    psMeta->ulMaxLength = ulLength;
  }

  memcpy( &psMeta->acData, pszMetaData, ulLength );
  psMeta->acData[ulLength] = '\0';
  psMeta->tTime = time( NULL );

  psFormat->ulMetaIdx++;
  if ( psFormat->ulMetaIdx == METADATA_ITEMS )
    psFormat->ulMetaIdx = 0;

  SysMutexUnlock( &psFormat->hmtxMeta );

  return TRUE;
}

BOOL FormatGetMetaDataHistoryItem(PFORMAT psFormat, ULONG ulIdx,
                                  PCHAR pcBufTime, ULONG ulBufTimeLength,
                                  PSZ *ppszMetaData)
{
  LONG		lIdx;
  PMETA		psMeta;
  struct tm	*psTMBuf;

  if ( ulIdx >= METADATA_ITEMS )
    return FALSE;

  SysMutexLock( &psFormat->hmtxMeta );
  lIdx = (psFormat->ulMetaIdx-1) - ulIdx;
  if ( lIdx < 0 )
    lIdx += METADATA_ITEMS;
  psMeta = psFormat->apsMeta[lIdx];
  SysMutexUnlock( &psFormat->hmtxMeta );

  if ( psMeta == NULL )
    return FALSE;
  *ppszMetaData = &psMeta->acData;

  psTMBuf = localtime( &psMeta->tTime );
  strftime( pcBufTime, ulBufTimeLength, "%a, %d %b %Y %T", psTMBuf );

  return TRUE;
}

VOID FormatReset(PFORMAT psFormat)
{
  if ( psFormat->psMethods->FmReset != NULL )
    psFormat->psMethods->FmReset( psFormat );
}
