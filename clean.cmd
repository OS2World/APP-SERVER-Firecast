@echo off

del lib\OS2\httpserv.lib 2>nul
del lib\OS2\iconv.lib 2>nul
del lib\OS2\libxml2.lib 2>nul
del lib\OS2\system.lib 2>nul

del lib\NT\httpserv.lib 2>nul
rem del lib\NT\iconv.lib 2>nul
del lib\NT\libxml2.lib 2>nul
del lib\NT\system.lib 2>nul

del bin\OS2\firecast.exe 2>nul
del bin\OS2\firecast.map 2>nul

del bin\NT\firecast.exe 2>nul
del bin\NT\firecast.map 2>nul

del bin\common\firecast.exe 2>nul
del bin\common\config.xml 2>nul

cd system
wmake -s -h clean
cd ..\httpserv
wmake -s -h clean
cd ..\iconv-os2
wmake -s -h clean
cd ..\libxml2
wmake -s -h clean
cd ..\src
wmake -s -h clean
