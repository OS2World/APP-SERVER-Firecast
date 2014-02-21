@echo off

if .%WATCOM%. == .. goto needow

if .%1. == .. goto help
if .%1. == .os2. set platform=OS2
if .%1. == .Os2. set platform=OS2
if .%1. == .OS2. set platform=OS2
if .%1. == .OS/2. set platform=OS2
if .%1. == .os/2. set platform=OS2
if .%1. == .Os/2. set platform=OS2
if .%1. == .ecs. set platform=OS2
if .%1. == .eCS. set platform=OS2
if .%1. == .ECS. set platform=OS2
if .%1. == .win. set platform=NT
if .%1. == .Win. set platform=NT
if .%1. == .WIN. set platform=NT
if .%1. == .win32. set platform=NT
if .%1. == .Win32. set platform=NT
if .%1. == .WIN32. set platform=NT
if .%1. == .nt. set platform=NT
if .%1. == .Nt. set platform=NT
if .%1. == .NT. set platform=NT

if .%platform%. == .. goto help
set target=OS/2, eComStation
if %platform% == NT set target=Windows

echo Target platform: %target%
echo ----------------------------------

echo make system.lib...
cd system
del *.obj 2>nul
del system.lib 2>nul
wmake -s -h platform=%platform%

echo make httpserv.lib...
cd ..\httpserv
del *.obj 2>nul
del httpserv.lib 2>nul
wmake -s -h platform=%platform%

if .%platform%. == .NT. goto l01
echo make iconv.lib...
cd ..\iconv-os2
del iconv.obj 2>nul
del iconv.lib 2>nul
wmake -s -h
:l01

echo make libxml2.lib...
cd ..\libxml2
del *.obj 2>nul
del libxml2.lib 2>nul
wmake -s -h platform=%platform%

echo make firecast.exe...
del ..\bin\%platform%\firecast.exe 2>nul
del ..\bin\%platform%\firecast.map 2>nul
cd ..\src
del *.obj 2>nul
del firecast.map 2>nul
del firecast.exe 2>nul
wmake -s -h platform=%platform%

exit

:help
echo Usage:
echo   make.cmd OS2
echo   make.cmd WIN
exit

:needow
echo Open Watcom must be installed on your system.
exit
