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
#include <stdlib.h>
#include <string.h>
#include <signal.h> 
#include "system.h"
#include "httpserv/httpserv.h"
#include "ssmessages.h"
#include "ssfileres.h"
#include "sources.h"
#include "shoutserv.h"
#include "templates.h"
#include "control.h"
#include "logmsg.h"
#include "format.h"
#include "relays.h"

#include <direct.h>
#include <dos.h>

static BOOL	bStop = FALSE;

static VOID _SetWorkPath(PSZ pszPath)
{
  CHAR		acDrive[_MAX_DRIVE];
  CHAR		acDir[_MAX_DIR];
  CHAR		acFName[_MAX_FNAME];
  CHAR		acExt[_MAX_EXT];
  unsigned int	ulTotal;

  _splitpath( pszPath, &acDrive, &acDir, &acFName, &acExt );
  _dos_setdrive( acDrive[0] - 'A' + 1, &ulTotal ); 
  chdir( &acDir );
}

static void _BreakHandler(int signo)
{ 
  bStop = TRUE;
} 

static VOID _Break()
{
  _BreakHandler( 0 );
  while( bStop )
    SysSleep( 1 );
}

static VOID _Init()
{
  LogInit();
  SysInit();
  FormatInit();
  HTTPServInit( "mime.types", 3, SERVER_STRING );
  TemplInit();
  SourceInit();
  SsFileResInit();
  ShoutServInit();
  RelayInit();
  CtrlInit( "config.xml" );
}

static VOID _Run(BOOL bConsole)
{
//RelayReady(RelayNew( "/test", "http://yes1.sahen.elektra.ru:8000/radio.ogg" ));
//RelayReady(RelayNew( "/test", "http://www.yahoo.com/radio.ogg" ));
//RelayReady(RelayNew( "/test", "http://www-2.snc.ru" ));
//RelayReady(RelayNew( "/test", "http://jabber.snc.ru:8000" ));
  if ( bConsole )
  {
    signal( SIGINT, _BreakHandler ); 
    signal( SIGBREAK, _BreakHandler ); 
  }

  while( !bStop )
  {
    ShoutLoop();
    SourceGarbageCollector();
    SysSleep( 50 );
  }
  bStop = FALSE;
}

static VOID _Done()
{
  CtrlDone();
  RelayDone();
  HTTPServDone();
  ShoutServDone();
  SsFileResDone();
  SourceDone();
  TemplDone();
  FormatDone();
  SysDone();
  LogDone();
}


#ifdef __OS2__

void main(int argc, char* argv[])
{
  _SetWorkPath( argv[0] );
  _Init();
  _Run( TRUE );
  _Done();
}

#else

#define FIRECAST_SERVICE_NAME	"Firecast"
#define FIRECAST_SERVICE_OPTION	"-service"

#define SW_INSTALL	1
#define SW_REMOVE	2
#define SW_SILENT	4

static SERVICE_DESCRIPTION Firecast_ServiceDescription =
  { "Fast Internet Radio Engine" };
static SERVICE_STATUS		svcStatus;
static SERVICE_STATUS_HANDLE	svcStatusHandle;
static HANDLE			hEvent;


static BOOL _WInit()
{
  LONG	lError;

  hEvent = CreateMutex( NULL, TRUE, FIRECAST_SERVICE_NAME );
  lError = GetLastError();
  if ( lError == ERROR_ALREADY_EXISTS || lError == ERROR_ACCESS_DENIED )
  {
    return FALSE;
  }

  _Init();
  return TRUE;
}

static VOID _WDone()
{
  _Done();
  CloseHandle( hEvent );
}

static LONG _InstallService()
{
  SC_HANDLE	schService;
  SC_HANDLE	schSCManager;
  CHAR		acServicePath[512];
  CHAR		acPath[512];
  LONG		lRes;
    
  if ( GetModuleFileName( NULL, acServicePath, 512 ) == 0 )
  {
    return -1;
  }

  snprintf( acPath, sizeof( acPath ), "%s %s", acServicePath,
            FIRECAST_SERVICE_OPTION );

  schSCManager = OpenSCManager( NULL, NULL, SC_MANAGER_CREATE_SERVICE );
  if ( !schSCManager )
  {
    return -1;
  }

  schService = OpenService( schSCManager, FIRECAST_SERVICE_NAME,
                            SERVICE_ALL_ACCESS );
  if ( schService != 0 )
  {
    CloseServiceHandle( schService );
    lRes = -2;
  }
  else
  {
    schService = CreateService( schSCManager,	/* SCManager database     */
        FIRECAST_SERVICE_NAME,			/* name of service        */
        FIRECAST_SERVICE_NAME,			/* name to display        */
        SERVICE_ALL_ACCESS,			/* desired access         */
        SERVICE_WIN32_OWN_PROCESS,		/* service type           */
        SERVICE_AUTO_START,			/* start type             */
        SERVICE_ERROR_NORMAL,			/* error control type     */
        (const char *)acPath,			/* service's binary       */
        NULL,					/* no load ordering group */
        NULL,					/* no tag identifier      */
        NULL,					/* dependencies           */
        NULL,					/* LocalSystem account    */
        NULL );					/* no password            */

    if ( schService == 0 )
    {
      lRes = -1;
    }
    else
    {
      ChangeServiceConfig2( schService, SERVICE_CONFIG_DESCRIPTION,
                            &Firecast_ServiceDescription );
      StartService( schService, 0, NULL );
      CloseServiceHandle( schService );
      lRes = 0;
    }
  }

  CloseServiceHandle( schSCManager );

  return lRes;
}

static LONG _RemoveService()
{
  SC_HANDLE	schService;
  SC_HANDLE	schSCManager;
  LONG		lRes;

  schSCManager = OpenSCManager( NULL, NULL, SC_MANAGER_ALL_ACCESS );
  if ( !schSCManager )
  {
    return -1;
  }

  schService = OpenService( schSCManager, FIRECAST_SERVICE_NAME,
                            SERVICE_ALL_ACCESS );
  if ( schService == 0 )
  {
    lRes = -2;
  }
  else
  {
    if ( ControlService( schService, SERVICE_CONTROL_STOP, &svcStatus ) )
    {
      Sleep( 1 );
      while ( QueryServiceStatus( schService, &svcStatus ) )
      {
        if ( svcStatus.dwCurrentState == SERVICE_STOP_PENDING )
          Sleep( 1 );
        else
          break;
      }
    }

    lRes = ( DeleteService( schService ) != 0 ) ? 0 : -1;
    CloseServiceHandle( schService );
  }

  CloseServiceHandle( schSCManager );

  return lRes;
}


static VOID SetFirecastServiceStatus(DWORD dwCurrentState,
                                     DWORD dwWin32ExitCode,
                                     DWORD dwWaitHint)
{
   static DWORD dwCheckPoint = 1;

   if ( dwCurrentState == SERVICE_START_PENDING )
      svcStatus.dwControlsAccepted = 0;
   else
      svcStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;

   svcStatus.dwCurrentState = dwCurrentState;
   svcStatus.dwWin32ExitCode = dwWin32ExitCode;
   svcStatus.dwWaitHint = dwWaitHint;

   if ( dwCurrentState == SERVICE_RUNNING || dwCurrentState == SERVICE_STOPPED )
      svcStatus.dwCheckPoint = 0;
   else
      svcStatus.dwCheckPoint++;

   SetServiceStatus( svcStatusHandle, &svcStatus );
}


void WINAPI ServiceControl(DWORD dwControlCode)
{
  switch( dwControlCode )
  {
    case SERVICE_CONTROL_STOP:
    {
      SetFirecastServiceStatus( SERVICE_STOP_PENDING, NO_ERROR, 0 );
      _Break();
      _WDone();
      SetFirecastServiceStatus( SERVICE_STOPPED, NO_ERROR, 0 );
      break;
    }

/*    case SERVICE_CONTROL_INTERROGATE:
    {
      SetFirecastServiceStatus( svcStatus.dwCurrentState, NO_ERROR, 0 );
      break;
    }*/
    default:
      SetFirecastServiceStatus( svcStatus.dwCurrentState, NO_ERROR, 0 );
  }
}

void WINAPI ServiceMain(DWORD argc, LPSTR* argv)
{
  svcStatusHandle = RegisterServiceCtrlHandler( FIRECAST_SERVICE_NAME,
                                                ServiceControl );
  if ( !svcStatusHandle )
  {
     return;
  }

  svcStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
  SetFirecastServiceStatus( SERVICE_START_PENDING, NO_ERROR, 4000 );
  _WInit();
  SetFirecastServiceStatus( SERVICE_RUNNING, NO_ERROR, 0 );
  _Run( FALSE );

  return;
}

int main(int argc, char* argv[])
{
  PSZ		pszMessage;
  LONG		lRC;
  ULONG		ulIdx;
  ULONG		ulSwitches;

  SERVICE_TABLE_ENTRY	DispatcherTable[] =
    { { FIRECAST_SERVICE_NAME, (LPSERVICE_MAIN_FUNCTION)ServiceMain },
      { NULL, NULL } };

  _SetWorkPath( argv[0] );

  if ( (argc == 2) && strcmp(argv[1], FIRECAST_SERVICE_OPTION) == 0 )
  {
    if ( StartServiceCtrlDispatcher( DispatcherTable ) == 0 )
    {
      printf( "StartServiceCtrlDispatcher: Error %ld\n", GetLastError() );
      return 1;
    }
    return 0;
  }


  for( ulSwitches = 0, ulIdx = 1; ulIdx < argc; ulIdx++ )
  {
    if ( stricmp( argv[ulIdx], "/install" ) == 0 )
    {
      ulSwitches |= SW_INSTALL;
    }
    else if ( stricmp( argv[ulIdx], "/remove" ) == 0 )
    {
      ulSwitches |= SW_REMOVE;
    }
    else if ( stricmp( argv[ulIdx], "/nomb" ) == 0 )
    {
      ulSwitches |= SW_SILENT;
    }
  }

  if ( (ulSwitches & SW_INSTALL) != 0 )
  {
    lRC = _InstallService();
    switch( lRC )
    {
      case -1:
      {
        pszMessage = "Can't install service";
        break;
      }
      case -2:
      {
        pszMessage = "The service already installed";
        break;
      }
      default:
        return 0;
    }
  }
  else if ( (ulSwitches & SW_REMOVE) != 0 )
  {
    lRC = _RemoveService();
    switch( lRC )
    {
      case -1:
      {
        pszMessage = "Can't remove service";
        break;
      }
      case -2:
      {
        pszMessage = "The service was not installed";
        break;
      }
      default:
        return 0;
    }
  }
  else
  {
    if ( !_WInit() )
    {
      pszMessage = "Another instance of Firecast is already running";
    }
    else
    {
      _Run( TRUE );
      _WDone();
      return 0;
    }
  }

  if ( (ulSwitches & SW_SILENT) == 0 )
    MessageBox( NULL, pszMessage, "Error", MB_OK | MB_ICONERROR );
  else
    printf( "Error: %s.\n", pszMessage );

  return 1;
}

#endif
