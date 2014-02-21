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

#ifndef SYSTEM_H
#define SYSTEM_H

// If defined the system API will be used for create threads,
// otherwise OpenWatcom's _beginthread() routine will be used.
//#define WITH_SYSTEM_THREADS 1

typedef long long		LLONG;
typedef unsigned long long	ULLONG;
typedef LLONG			*PLLONG;
typedef ULLONG			*PULLONG;


#ifdef __OS2__

#include <types.h>
#include <netinet\in.h>
#define INCL_DOSSEMAPHORES
#define INCL_DOSQUEUES
#define INCL_DOSPROCESS
#define INCL_DOSMISC
#define INCL_DOSERRORS
#include <os2.h>
typedef TID THREAD;

#ifdef WITH_SYSTEM_THREADS
#define THREADRET VOID
#define THREADCALL APIENTRY
#define THREADRC
#endif

#else

#include <windows.h>
typedef HANDLE HMTX;
typedef HMTX *PHMTX;
typedef HANDLE THREAD;

#ifdef WITH_SYSTEM_THREADS
#define THREADRET DWORD
#define THREADCALL WINAPI
#define THREADRC 1
#endif

#endif

#ifndef WITH_SYSTEM_THREADS
#define THREADRET VOID
#define THREADCALL
#define THREADRC
#endif

typedef THREADRET THREADCALL THREADPROC (PVOID);
typedef THREADPROC *PTHREADPROC;

typedef THREAD *PTHREAD;

#define RET_OK				0
#define RET_INVALID_PARAM		1
#define RET_NOT_ENOUGH_MEMORY		2
#define RET_CANT_START_THREAD		4
#define RET_ERROR_CREATE_SOCKET		5
#define RET_ERROR_BIND_SOCKET		6
#define RET_CANNOT_CONNECT		7
#define RET_WAIT_CONNECT		8
#define RET_REFUSED			9
#define RET_UNREACH			10
#define RET_NO_BUFFERS			11

// SysCheckPath return codes
#define CHECK_PATH_NOT_FOUND		0
#define CHECK_PATH_FILE			1
#define CHECK_PATH_DIRECTORY		2

typedef struct _MSGPIPE {
  HFILE		hRead;
  HFILE		hWrite;
} MSGPIPE, *PMSGPIPE;

typedef struct _RWMTX {
  HMTX		hMtx;
  ULONG		ulReadLocks;
} RWMTX, *PRWMTX;


ULONG lockFlag(PULONG flag);
#pragma aux lockFlag = \
  "lock bts     dword ptr [eax],0"\
  "     jnc     @@RET            "\
  "     xor	eax,eax		 "\
  "   @@RET:                     "\
  parm [eax] value [eax];

VOID clearFlag(PULONG flag);
#pragma aux clearFlag = \
  "lock and [eax],0	"\
  parm [eax]; 

VOID SysInit();
VOID SysDone();
//VOID SysTime64( PULLONG pullTime );
VOID SysTime(PULONG pulTime);
// threads
VOID SysMutexCreate(PHMTX phMtx);
VOID SysMutexDestroy(PHMTX phMtx);
VOID SysMutexLock(PHMTX phMtx);
//#define SysMutexLock(pmtx) dbgSysMutexLock(pmtx, __FILE__, __LINE__)
//VOID dbgSysMutexLock(PHMTX phMtx, PSZ pszFile, ULONG ulLine);

VOID SysMutexUnlock(PHMTX phMtx);
ULONG SysThreadCreate(PTHREAD pThread, PTHREADPROC ThreadProc, PVOID pData);
#ifdef WITH_SYSTEM_THREADS
VOID SysWaitThread(PTHREAD pThread);
#endif
VOID SysSleep(ULONG ulTime);
VOID SysRWMutexCreate(PRWMTX psRWMtx);
VOID SysRWMutexDestroy(PRWMTX psRWMtx);
VOID SysRWMutexLockWrite(PRWMTX psRWMtx);
//#define SysRWMutexLockWrite(pmtx) dbgSysRWMutexLockWrite(pmtx, __FILE__, __LINE__)
//VOID dbgSysRWMutexLockWrite(PRWMTX psRWMtx, PSZ pszFile, ULONG ulLine);

VOID SysRWMutexUnlockWrite(PRWMTX psRWMtx);
VOID SysRWMutexLockRead(PRWMTX psRWMtx);
//#define SysRWMutexLockRead(pmtx) dbgSysRWMutexLockRead(pmtx, __FILE__, __LINE__)
//VOID dbgSysRWMutexLockRead(PRWMTX psRWMtx, PSZ pszFile, ULONG ulLine);

VOID SysRWMutexUnlockRead(PRWMTX psRWMtx);
// network
VOID SysSockClose(ULONG ulSocket);
BOOL SysInetAddr(PSZ pszAddr, struct in_addr *psInAddr);
VOID SysStrAddr(PSZ pszBuf, ULONG ulBufLen, struct in_addr *psInAddr);
BOOL SysGetHostByName(PSZ pszAddr, struct in_addr *psInAddr);
ULONG SysCreateServSock(PULONG pulSock, struct in_addr *psInAddr,
                        USHORT usPort, ULONG ulQueueLen);
ULONG SysCreateClientSock(PULONG pulSock, struct in_addr *psInAddr, USHORT usPort);
ULONG SysWaitClientSock(PULONG pulSock);
BOOL SysSelect(fd_set *fdsRead, fd_set *fdsWrite, fd_set *fdsExcept, ULONG ulTimeout);
BOOL SysSockAccept(PULONG pulSock, struct in_addr *psInAddr);
BOOL SysSockAddr(PULONG pulSock, struct in_addr *psInAddr);
BOOL SysSockPeerAddr(PULONG pulSock, struct in_addr *psInAddr);
BOOL SysSockRead(ULONG ulSock, PCHAR pcBuf, ULONG ulBufLen, PULONG pulActual);
BOOL SysSockWrite(ULONG ulSock, PCHAR pcBuf, ULONG ulBufLen, PULONG pulActual);
BOOL SysSockReadReady(ULONG ulSock);
BOOL SysSockWriteReady(ULONG ulSock);
USHORT SysSockPort(ULONG ulSock);
BOOL SysSockConnected(ULONG ulSock);
// message pipes
/*BOOL SysMsgPipeCreate(PMSGPIPE psMsgPipe, ULONG ulSize);
VOID SysMsgPipeDestroy(PMSGPIPE psMsgPipe);
BOOL SysMsgPipeWrite(PMSGPIPE psMsgPipe, PVOID pMsg, ULONG ulMsgSize);
BOOL SysMsgPipeRead(PMSGPIPE psMsgPipe, PVOID pMsg, PULONG pulMsgSize);*/
// files
ULONG SysCheckPath(PSZ pszPath);

#endif // SYSTEM_H
