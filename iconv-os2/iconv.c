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

//#define ICONV_THREAD_SAFE 1
#undef ICONV_THREAD_SAFE

#include "iconv.h"
#include <uconv.h>
#include <stdlib.h>
#include <string.h>
#ifdef ICONV_THREAD_SAFE
#define INCL_DOSSEMAPHORES
#define INCL_DOSERRORS
#include <os2.h>
#endif

#define		MAX_CP_NAME_LEN		64

typedef struct iuconv_obj {
  UconvObject	uo_tocode;
  UconvObject	uo_fromcode;
  int		buf_len;
  UniChar	*buf;
#ifdef ICONV_THREAD_SAFE
  HMTX		hMtx;
#endif
} iuconv_obj;


static int uconv_open(const char *code, UconvObject *uobj)
{
  UniChar	uc_code[MAX_CP_NAME_LEN];
  const char	*ch = code;
  int		i;

  if ( !stricmp(code, "UTF-16") )
  {
    *uobj = NULL;
    return ULS_SUCCESS;
  }
  else if ( !code )
    uc_code[0] = 0;
  else
    for(i=0; i < MAX_CP_NAME_LEN; i++)
    {
      uc_code[i] = (unsigned short)*ch;
      if ( !(*ch) ) break;
      ch++;
    }

  return UniCreateUconvObject(&uc_code, uobj);
}


extern iconv_t iconv_open (const char* tocode, const char* fromcode)
{
  UconvObject	uo_tocode;
  UconvObject	uo_fromcode;
  int		rc;
  iuconv_obj	*iuobj;

  if ( stricmp(tocode, fromcode) )
  {
    rc = uconv_open(fromcode, &uo_fromcode);
    if ( rc != ULS_SUCCESS )
    {
      errno = EINVAL;
      return (iconv_t)(-1);
    }

    rc = uconv_open(tocode, &uo_tocode);
    if ( rc != ULS_SUCCESS )
    {
      UniFreeUconvObject(uo_tocode);
      errno = EINVAL;
      return (iconv_t)(-1);
    }
  }
  else {
    uo_tocode = NULL;
    uo_fromcode = NULL;
  }

  iuobj = malloc( sizeof(iuconv_obj) );
  iuobj->uo_tocode = uo_tocode;
  iuobj->uo_fromcode = uo_fromcode;
  iuobj->buf_len = 0;
  iuobj->buf = NULL;
#ifdef ICONV_THREAD_SAFE
  DosCreateMutexSem( NULL, &iuobj->hMtx, 0, FALSE );
#endif

  return iuobj;
}

extern size_t iconv (iconv_t cd, const char* * inbuf, size_t *inbytesleft, char* * outbuf, size_t *outbytesleft)
{
  UconvObject	uo_tocode = ((iuconv_obj *)(cd))->uo_tocode;
  UconvObject	uo_fromcode = ((iuconv_obj *)(cd))->uo_fromcode;
  size_t	nonIdenticalConv = 0;  
  UniChar	*uc_buf;
  size_t	uc_buf_len;
  UniChar	**uc_str;
  size_t	*uc_str_len;
  int		rc;
  size_t	ret = (size_t)(-1);

#ifdef ICONV_THREAD_SAFE
  DosRequestMutexSem( ((iuconv_obj *)(cd))->hMtx, SEM_INDEFINITE_WAIT );
#endif

  if ( uo_tocode && uo_fromcode && 
       (( ((iuconv_obj *)(cd))->buf_len >> 1 ) < (*inbytesleft)) )
  {
    free( ((iuconv_obj *)(cd))->buf );
    ((iuconv_obj *)(cd))->buf_len = *inbytesleft << 1;
    ((iuconv_obj *)(cd))->buf = (UniChar *)malloc( ((iuconv_obj *)(cd))->buf_len );
  }

  if ( uo_fromcode )
  {
    if ( uo_tocode )
    {
      uc_buf = ((iuconv_obj *)(cd))->buf;
      uc_buf_len = ((iuconv_obj *)(cd))->buf_len;
      uc_str = &uc_buf;
    }
    else {
      uc_str = (UniChar **)outbuf;
      uc_buf_len = *outbytesleft;
    }
    uc_buf_len = uc_buf_len >> 1;
    uc_str_len = &uc_buf_len;
    rc = UniUconvToUcs( uo_fromcode, (void **)inbuf, inbytesleft, 
                        uc_str, uc_str_len, &nonIdenticalConv );
    uc_buf_len = uc_buf_len << 1;
    if ( !uo_tocode )
      *outbytesleft = uc_buf_len;

    if ( rc != ULS_SUCCESS )
    {
      errno = EILSEQ;
      goto done;
    }
    else
      if ( *inbytesleft && !*uc_str_len )
      {
        errno = E2BIG;
        goto done;
      }

    if ( !uo_tocode )
      return nonIdenticalConv;

    uc_buf = ((iuconv_obj *)(cd))->buf;
    uc_buf_len = ((iuconv_obj *)(cd))->buf_len - uc_buf_len;
    uc_str = &uc_buf;
    uc_str_len = &uc_buf_len;
  }
  else {
    uc_str = (UniChar **)inbuf;
    uc_str_len = inbytesleft;
  }

  *uc_str_len = *uc_str_len>>1;
  rc = UniUconvFromUcs( uo_tocode, uc_str, uc_str_len, (void **)outbuf, outbytesleft,
                        &nonIdenticalConv );
  if ( rc != ULS_SUCCESS )
  {
    switch ( rc )
    {
      case ULS_BUFFERFULL:
        errno = E2BIG;
        break;
      case ULS_ILLEGALSEQUENCE:
        errno = EILSEQ;
        break;
      case ULS_INVALID:
        errno = EINVAL;
        break;
    }
    goto done;
  }
  else
    if ( *uc_str_len && !*outbytesleft )
    {
      errno = E2BIG;
      goto done;
    }

  ret = nonIdenticalConv;

done:

#ifdef ICONV_THREAD_SAFE
  DosReleaseMutexSem( ((iuconv_obj *)(cd))->hMtx );
#endif
  return ret;
}

extern int iconv_close (iconv_t cd)
{
  if ( !cd )
    return 0;

#ifdef ICONV_THREAD_SAFE
  DosCloseMutexSem( ((iuconv_obj *)(cd))->hMtx );
#endif
  UniFreeUconvObject( ((iuconv_obj *)(cd))->uo_tocode );
  UniFreeUconvObject( ((iuconv_obj *)(cd))->uo_fromcode );
  free( ((iuconv_obj *)(cd))->buf );
  free(cd);

  return 0;
}
