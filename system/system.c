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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifndef WITH_SYSTEM_THREADS
#include <process.h>
#endif
#include "system.h"

#ifdef __OS2__

#include <types.h>
#include <unistd.h>
#include <arpa\inet.h>
#include <netdb.h>
#include <sys\socket.h>
#include <sys\time.h>
#include <sys\ioctl.h>
#include <net\route.h>
#include <net\if.h>
#include <net\if_arp.h>
#include <nerrno.h>

#else

#define SOCEWOULDBLOCK			WSAEWOULDBLOCK
#define SOCEINTR			WSAEINTR
#define SOCECONNREFUSED			WSAECONNREFUSED
#define SOCENETUNREACH			WSAENETUNREACH
#define SOCENOBUFS			WSAENOBUFS 
#define sock_errno()			WSAGetLastError()

#endif

#define		THREAD_STACK		32*1024
#define		LISTEN_QUEUE_LENGTH	8


VOID SysInit()
{
#ifdef __OS2__
  sock_init();
#else
//  UCHAR		WSAStartup_buff[1024];
  WSADATA	wsad;

  WSAStartup( 0x0202, &wsad /*(LPWSADATA)&WSAStartup_buff*/ );
#endif
}

VOID SysDone()
{
#ifndef __OS2__
  WSACleanup();
#endif

}

VOID SysTime(PULONG pulTime)
{
#ifdef __OS2__
  DosQuerySysInfo( QSV_MS_COUNT, QSV_MS_COUNT, pulTime, sizeof(ULONG) );
#else
  *pulTime = (ULONG)GetTickCount();
#endif
}

VOID SysMutexCreate(PHMTX phMtx)
{
#ifdef __OS2__
  DosCreateMutexSem( NULL, phMtx, 0, FALSE );
#else
  *phMtx = CreateMutex( NULL, FALSE, NULL );
#endif
}

VOID SysMutexDestroy(PHMTX phMtx)
{
#ifdef __OS2__
  DosCloseMutexSem( *phMtx );
#else
  CloseHandle( *phMtx );
#endif
}

#ifndef SysMutexLock
VOID SysMutexLock(PHMTX phMtx)
{
#ifdef __OS2__
  DosRequestMutexSem( *phMtx, SEM_INDEFINITE_WAIT );
#else
  WaitForSingleObject( *phMtx, INFINITE );
#endif
}
#else
VOID dbgSysMutexLock(PHMTX phMtx, PSZ pszFile, ULONG ulLine)
{
  ULONG		ulRC;

  while( TRUE )
  {
    ulRC = DosRequestMutexSem( *phMtx, 1000 );
    if ( ulRC == NO_ERROR )
      break;
    if ( ulRC == ERROR_TIMEOUT )
      printf("%s #%u: DosRequestMutexSem(), ERROR_TIMEOUT\n", pszFile, ulLine);
    else
      printf("%s #%u: DosRequestMutexSem(), rc = %u\n", pszFile, ulLine, ulRC);
  }
}
#endif

VOID SysMutexUnlock(PHMTX phMtx)
{
#ifdef __OS2__
  DosReleaseMutexSem( *phMtx );
#else
  ReleaseMutex( *phMtx );
#endif
}


#ifdef WITH_SYSTEM_THREADS

ULONG SysThreadCreate(PTHREAD pThread, PTHREADPROC ThreadProc, PVOID pData)
{
#ifdef __OS2__
  APIRET	ulRC;

  ulRC = DosCreateThread( pThread, (PFNTHREAD)ThreadProc, (ULONG)pData,
                          0, THREAD_STACK );
  switch ( ulRC )
  {
     case NO_ERROR:
       return RET_OK;
     case ERROR_NOT_ENOUGH_MEMORY:
       return RET_NOT_ENOUGH_MEMORY;
     default:
       return RET_CANT_START_THREAD;
  }
#else
  DWORD		dwThreadId;

  *pThread = CreateThread( NULL, THREAD_STACK, ThreadProc, pData, 0,
                           &dwThreadId );
  if ( *pThread == 0 )
  {
    return RET_CANT_START_THREAD;
  }
  else
  {
    CloseHandle( *pThread );
    return RET_OK;
  }
#endif
}

VOID SysWaitThread(PTHREAD pThread)
{
#ifdef __OS2__
  DosWaitThread( pThread, DCWW_WAIT );
#else
  WaitForSingleObject( *pThread, INFINITE );
#endif
}

#else

ULONG SysThreadCreate(PTHREAD pThread, PTHREADPROC ThreadProc, PVOID pData)
{
#ifdef __OS2__
    LONG	tid;
#define ERROR_TID_VALUE	-1
#else 
    ULONG	tid; 
#define ERROR_TID_VALUE	((ULONG)(-1))
#endif 

  tid = _beginthread( ThreadProc, 
#ifdef __OS2__
                 NULL,
#endif
                 THREAD_STACK, pData );

  if ( tid == ERROR_TID_VALUE )
    return RET_CANT_START_THREAD;

  *pThread = (THREAD)tid;
  return RET_OK;
}

#endif // WITH_SYSTEM_THREADS

VOID SysSleep( ULONG ulTime )
{
#ifdef __OS2__
  DosSleep( ulTime );
#else
  Sleep( ulTime );
#endif
}

VOID SysRWMutexCreate(PRWMTX psRWMtx)
{
  SysMutexCreate( &psRWMtx->hMtx );
  psRWMtx->ulReadLocks = 0;
}

VOID SysRWMutexDestroy(PRWMTX psRWMtx)
{
  SysMutexDestroy( &psRWMtx->hMtx );
}

#ifndef SysRWMutexLockWrite
VOID SysRWMutexLockWrite(PRWMTX psRWMtx)
{
  while( 1 )
  {
    SysMutexLock( &psRWMtx->hMtx );
    if ( psRWMtx->ulReadLocks == 0 )
      break;
    SysMutexUnlock( &psRWMtx->hMtx );
    SysSleep( 1 );
  }
}
#else
VOID dbgSysRWMutexLockWrite(PRWMTX psRWMtx, PSZ pszFile, ULONG ulLine)
{
  ULONG		ulIdx;

  while( 1 )
  {
    for( ulIdx = 0; ulIdx < 500; ulIdx++ )
    {
      SysMutexLock( &psRWMtx->hMtx );
      if ( psRWMtx->ulReadLocks == 0 )
        return;
      SysMutexUnlock( &psRWMtx->hMtx );
      SysSleep( 1 );
    }
    printf("%s #%u: dbgSysRWMutexLockWrite(), TIMEOUT\n", pszFile, ulLine);
  }
}
#endif

VOID SysRWMutexUnlockWrite(PRWMTX psRWMtx)
{
  SysMutexUnlock( &psRWMtx->hMtx );
}

#ifndef SysRWMutexLockRead
VOID SysRWMutexLockRead(PRWMTX psRWMtx)
{
  SysMutexLock( &psRWMtx->hMtx );
  psRWMtx->ulReadLocks++;
  SysMutexUnlock( &psRWMtx->hMtx );
}
#else
VOID dbgSysRWMutexLockRead(PRWMTX psRWMtx, PSZ pszFile, ULONG ulLine)
{
  dbgSysMutexLock( &psRWMtx->hMtx, pszFile, ulLine );
  psRWMtx->ulReadLocks++;
  SysMutexUnlock( &psRWMtx->hMtx );
}
#endif

VOID SysRWMutexUnlockRead(PRWMTX psRWMtx)
{
  SysMutexLock( &psRWMtx->hMtx );
  if ( psRWMtx->ulReadLocks > 0 )
    psRWMtx->ulReadLocks--;
  SysMutexUnlock( &psRWMtx->hMtx );
}



VOID SysSockClose(ULONG ulSocket)
{
#ifdef __OS2__
  soclose( ulSocket );
#else
  closesocket( ulSocket );
#endif
}

BOOL SysInetAddr(PSZ pszAddr, struct in_addr *psInAddr )
{
  psInAddr->s_addr = inet_addr( pszAddr );

  return psInAddr->s_addr != (u_long)(-1);
}

VOID SysStrAddr(PSZ pszBuf, ULONG ulBufLen, struct in_addr *psInAddr)
{
  ulBufLen--;
  strncpy( pszBuf, inet_ntoa(*psInAddr), ulBufLen );
  pszBuf[ ulBufLen ] = '\0';
}

BOOL SysGetHostByName(PSZ pszAddr, struct in_addr *psInAddr)
{
  struct hostent	*psHostEnt;

  psHostEnt = gethostbyname( pszAddr );
  if ( psHostEnt == NULL )
    return FALSE;

  memcpy( psInAddr, psHostEnt->h_addr, sizeof(struct in_addr) );
  return TRUE;
}

ULONG SysCreateServSock(PULONG pulSock, struct in_addr *psInAddr,
                        USHORT usPort, ULONG ulQueueLen)
{
  struct sockaddr_in	sSockAddrIn;
  ULONG			ulVal = 1;
  ULONG			ulLen = sizeof(struct sockaddr_in);

  if ( pulSock == NULL || usPort == 0 )
    return RET_INVALID_PARAM;

  *pulSock = socket( AF_INET, SOCK_STREAM, 0 );
  if ( *pulSock == -1 )
    return RET_ERROR_CREATE_SOCKET;

  setsockopt( *pulSock, SOL_SOCKET, SO_REUSEADDR, (const void *)&ulVal,
              sizeof(ULONG) );

  if ( psInAddr == NULL )
    sSockAddrIn.sin_addr.s_addr = INADDR_ANY;
  else
    sSockAddrIn.sin_addr = *psInAddr;
  sSockAddrIn.sin_family = AF_INET;
  sSockAddrIn.sin_port = htons(usPort);

  if ( bind( *pulSock, (struct sockaddr *)&sSockAddrIn, ulLen ) == -1 )
  {
    #ifdef __OS2__
    soclose( *pulSock );
    #else
    closesocket( *pulSock );
    #endif
    return RET_ERROR_BIND_SOCKET;
  }

  if ( listen( *pulSock, ulQueueLen ) == -1 )
  {
    #ifdef __OS2__
    soclose( *pulSock );
    #else
    closesocket( *pulSock );
    #endif
    return RET_ERROR_CREATE_SOCKET;
  }

  return RET_OK;
}

ULONG SysCreateClientSock(PULONG pulSock, struct in_addr *psInAddr, USHORT usPort)
{
  struct sockaddr_in	sSockAddrIn;
  ULONG			ulVal = 1;
  int			iErrNo;

  *pulSock = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
  if ( *pulSock == (ULONG)(-1) )
    return RET_ERROR_CREATE_SOCKET;

  #ifdef __OS2__
  ioctl( *pulSock, FIONBIO, (PCHAR)&ulVal );
  //#elif unix
  //int flags = fcntl(fd, F_GETFL, 0);
  //fcntl(fd, F_SETFL, flags | O_NONBLOCK);
  #else
  ioctlsocket( *pulSock, FIONBIO, &ulVal );
  #endif

  sSockAddrIn.sin_family = AF_INET;
  sSockAddrIn.sin_addr = *psInAddr;
  sSockAddrIn.sin_port = htons(usPort);

  do
  {
    if ( connect( *pulSock, (struct sockaddr *)&sSockAddrIn,
                  sizeof(struct sockaddr_in) ) == 0 )
    {
      return RET_OK;
    }

    iErrNo = sock_errno();
  }
  while( iErrNo == SOCEINTR );

  #ifdef __OS2__
    if ( iErrNo == SOCEINPROGRESS )
      return RET_WAIT_CONNECT;
    soclose( *pulSock );
  #else
    if ( iErrNo == WSAEWOULDBLOCK )
      return RET_WAIT_CONNECT;
    closesocket( *pulSock );
  #endif
  switch( iErrNo )
  {
  case SOCECONNREFUSED:
    return RET_REFUSED;
  case SOCENETUNREACH:
    return RET_UNREACH;
  case SOCENOBUFS:
    return RET_NO_BUFFERS;
  }

  return RET_CANNOT_CONNECT;
}

ULONG SysWaitClientSock(PULONG pulSock)
{
//  fd_set		fdsRead;
  fd_set		fdsWrite;
  ULONG			ulSock = *pulSock;
  struct timeval	sTV = { 0 };
  int			iErrLen;
  int			iError;

  if ( ulSock == (ULONG)(-1) )
    return RET_ERROR_CREATE_SOCKET;

//  FD_ZERO( &fdsRead );
  FD_ZERO( &fdsWrite );
//  FD_SET( ulSock, &fdsRead );
  FD_SET( ulSock, &fdsWrite );

//  sTV.tv_usec = 500;
  iErrLen = select( ulSock + 1, NULL/*&fdsRead*/, &fdsWrite, NULL, &sTV );

  if ( iErrLen < 0 )
    return RET_CANNOT_CONNECT;

  if ( ( iErrLen == 0 ) || 
       ( /*!FD_ISSET(ulSock, &fdsRead) &&*/ !FD_ISSET(ulSock, &fdsWrite) ) )
    return RET_WAIT_CONNECT;

  iErrLen = sizeof( iError );
  if ( getsockopt( ulSock, SOL_SOCKET, SO_ERROR, (PCHAR)&iError, &iErrLen ) < 0
       || iError != 0 )
  {
    return RET_CANNOT_CONNECT;
  }

  return RET_OK;
}

BOOL SysSelect(fd_set *fdsRead, fd_set *fdsWrite, fd_set *fdsExcept, ULONG ulTimeout)
{
  LONG			lRC;
  struct timeval	sTimeout;

  sTimeout.tv_sec = ulTimeout / 1000;
  sTimeout.tv_usec = (ulTimeout % 1000) * 1000;

  lRC = select( 0, fdsRead, fdsWrite, fdsExcept, &sTimeout );

  if ( lRC == -1 )
  {
    SysSleep( ulTimeout );
/*    FD_ZERO( fdsRead );
    FD_ZERO( fdsWrite );
    FD_ZERO( fdsExcept );*/
  }

  return lRC > 0;
}

BOOL SysSockAccept(PULONG pulSock, struct in_addr *psInAddr)
{
  LONG			lRC;
  struct sockaddr_in	sSockAddr;
  struct sockaddr_in	sClientAddr;
  int			cbSockAddr = sizeof(struct sockaddr_in);
  ULONG			ulVal;
  struct linger		sLinger = { 0, 0 };

  lRC = accept( *pulSock, (struct sockaddr *)&sClientAddr, &cbSockAddr );
  if ( lRC < 0 )
    return FALSE;

  *pulSock = lRC;

  if ( psInAddr != NULL )
  {
    if ( psInAddr->s_addr != INADDR_ANY )
    {
      cbSockAddr = sizeof(struct sockaddr_in);
      getsockname( *pulSock, (struct sockaddr *)&sSockAddr, &cbSockAddr );
      if ( sSockAddr.sin_addr.s_addr != psInAddr->s_addr )
      {
        #ifdef __OS2__
        soclose( *pulSock );
        #else
        closesocket( *pulSock );
        #endif
        return FALSE;
      }
    }
    *psInAddr = sClientAddr.sin_addr;
  }

  setsockopt( *pulSock, SOL_SOCKET, SO_LINGER, (const PVOID)&sLinger, sizeof(struct linger) );
  ulVal = 1;
  setsockopt( *pulSock, SOL_SOCKET, SO_KEEPALIVE, (const PVOID)&ulVal, sizeof(ULONG) );
  ulVal = 1;
  #ifdef __OS2__
  ioctl( *pulSock, FIONBIO, (PUCHAR)&ulVal );
  #else
  ioctlsocket( *pulSock, FIONBIO, &ulVal );
  #endif
  return TRUE;
}

BOOL SysSockAddr(PULONG pulSock, struct in_addr *psInAddr)
{
  struct sockaddr_in	sSockAddr;
  int			cbSockAddr = sizeof(struct sockaddr_in);
  int			rc;

  rc = getsockname( *pulSock, (struct sockaddr *)&sSockAddr, &cbSockAddr );
  if ( rc == -1 )
    return FALSE;

  *psInAddr = sSockAddr.sin_addr;
  return TRUE;
}

BOOL SysSockPeerAddr(PULONG pulSock, struct in_addr *psInAddr)
{
  struct sockaddr_in	sSockAddr;
  int			cbSockAddr = sizeof(struct sockaddr_in);
  int			rc;

  rc = getpeername( *pulSock, (struct sockaddr *)&sSockAddr, &cbSockAddr );
  if ( rc == -1 )
    return FALSE;

  *psInAddr = sSockAddr.sin_addr;
  return TRUE;
}

BOOL SysSockRead(ULONG ulSock, PCHAR pcBuf, ULONG ulBufLen, PULONG pulActual)
{
  LONG		lRC;

  lRC = recv( ulSock, pcBuf, ulBufLen, 0 );

  if ( lRC == 0 )
    return FALSE;

  if ( lRC == -1 )
  {
    if ( sock_errno() != SOCEWOULDBLOCK )
      return FALSE;

    lRC = 0;
  }
  *pulActual = lRC;

  return TRUE;
}

BOOL SysSockWrite(ULONG ulSock, PCHAR pcBuf, ULONG ulBufLen, PULONG pulActual)
{
  LONG		lRC;

  lRC = send( ulSock, pcBuf, ulBufLen, 0 );

  if ( lRC < 0 )
  {
    if ( sock_errno() != SOCEWOULDBLOCK )
      return FALSE;
    lRC = 0;
  }

  *pulActual = lRC;

  return TRUE;
}

BOOL SysSockReadReady(ULONG ulSock)
{
#ifdef __OS2__
  return os2_select( (int *)&ulSock, 1, 0, 0, 0 ) > 0;
#else
  fd_set		sFD;
  struct timeval	sTV = {0}, *psTV = &sTV;
  LONG			lRC;

  FD_ZERO( &sFD );
  FD_SET( ulSock, &sFD );

  lRC = select( ulSock+1, &sFD, NULL, NULL, psTV );

  return lRC < 0 ? FALSE : FD_ISSET( ulSock, &sFD );
#endif
}

BOOL SysSockWriteReady(ULONG ulSock)
{
#ifdef __OS2__
  return os2_select( (int *)&ulSock, 0, 1, 0, 0 ) > 0;
#else
  fd_set		sFD;
  struct timeval	sTV = {0}, *psTV = &sTV;
  LONG			lRC;

  FD_ZERO( &sFD );
  FD_SET( ulSock, &sFD );

  lRC = select( ulSock+1, NULL, &sFD, NULL, psTV );

  return lRC < 0 ? FALSE : FD_ISSET( ulSock, &sFD );
#endif
}

USHORT SysSockPort(ULONG ulSock)
{
  struct sockaddr_in	sSockAddr;
  int			cbSockAddr = sizeof( struct sockaddr_in );
  int			rc;

  rc = getsockname( (int)ulSock, (struct sockaddr *)&sSockAddr, &cbSockAddr );
  return rc == -1 ? 0 : ntohs(sSockAddr.sin_port);
}

BOOL SysSockConnected(ULONG ulSock)
{
  LONG		lRC;
  CHAR		cBuf;

  lRC = recv( ulSock, &cBuf, sizeof(CHAR), MSG_PEEK );

  if ( lRC == 0 || (lRC == -1 && (sock_errno() != SOCEWOULDBLOCK)) )
    return FALSE;

  return TRUE;
}


/*
BOOL SysMsgPipeCreate(PMSGPIPE psMsgPipe, ULONG ulSize)
{
  ULONG		ulRC;

  ulRC = DosCreatePipe( &psMsgPipe->hRead, &psMsgPipe->hWrite, ulSize );

  return ulRC == NO_ERROR;
}

VOID SysMsgPipeDestroy(PMSGPIPE psMsgPipe)
{
  DosClose( psMsgPipe->hRead );
  DosClose( psMsgPipe->hWrite );
}

BOOL SysMsgPipeWrite(PMSGPIPE psMsgPipe, PVOID pMsg, ULONG ulMsgSize)
{
  ULONG		ulRC;

  ulRC = DosWrite( psMsgPipe->hWrite, pMsg, ulMsgSize, &ulMsgSize );

  return ulRC == NO_ERROR;
}

BOOL SysMsgPipeRead(PMSGPIPE psMsgPipe, PVOID pMsg, PULONG pulMsgSize)
{
  ULONG		ulRC;

  ulRC = DosRead( psMsgPipe->hRead, pMsg, *pulMsgSize, pulMsgSize );

  return ulRC == NO_ERROR;
}
*/

ULONG SysCheckPath(PSZ pszPath)
{
#ifdef __OS2__
  FILESTATUS3L  sInfo;
  ULONG		ulRC;

  ulRC = DosQueryPathInfo( pszPath, FIL_STANDARDL, &sInfo, sizeof(FILESTATUS3L) );
  if ( ulRC != NO_ERROR || sInfo.attrFile & FILE_HIDDEN != 0 )
    return CHECK_PATH_NOT_FOUND;

  if ( (sInfo.attrFile & FILE_DIRECTORY) != 0 )
    return CHECK_PATH_DIRECTORY;

  return CHECK_PATH_FILE;
#else
  WIN32_FIND_DATA	sFindData;
  ULONG			ulRes;
  HANDLE		hFind;

  hFind = FindFirstFile( pszPath, &sFindData );
  if ( hFind == INVALID_HANDLE_VALUE )
    return CHECK_PATH_NOT_FOUND;

  if ( (sFindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 )
    ulRes = CHECK_PATH_DIRECTORY;
  else
    ulRes = CHECK_PATH_FILE;

  FindClose( hFind );
  return ulRes;
#endif
}
