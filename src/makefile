#
# Copyright 2008 Vasilkin Andrey <digi@os2.snc.ru>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
#   1. Redistributions of source code must retain the above copyright notice,
#      this list of conditions and the following disclaimer.
#
#   2. Redistributions in binary form must reproduce the above copyright
#      notice, this list of conditions and the following disclaimer in the
#      documentation and/or other materials provided with the distribution.
#
#   3. The name of the author may not be used to endorse or promote products
#      derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
# WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
# EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

# Open Watcom
# usage:
#   wmake platform=OS2
#   wmake platform=NT

!ifeq PLATFORM NT
LINKPLATFORM = nt
OS_LIBS = wsock32 ws2_32
!else
PLATFORM = OS2
LINKPLATFORM = os2v2
OS_LIBS = libconv
!endif

INCPATH = $(%WATCOM)\H\$(PLATFORM);$(%WATCOM)\H
INCPATH +=;..\system
INCPATH +=;..\libxml2\include
INCPATH +=;..\iconv-os2
INCPATH +=;..\httpserv\include

LIBS = system iconv libxml2 httpserv $(OS_LIBS)

LIBPATH = ..\lib\$(PLATFORM)

CFLAGS = -i=$(INCPATH) -bt=$(PLATFORM) -d0 -q -bm -s

OBJS = clients.obj control.obj format.obj main.obj match.obj shoutserv.obj
OBJS += sources.obj ssfileres.obj templates.obj logmsg.obj relays.obj

firecast.exe: $(OBJS)
  wlink system $(LINKPLATFORM) file {$(OBJS:.obj=)} libpath $(LIBPATH) lib {$(LIBS)} name $* op map=$* op el
  copy $*.map ..\bin\$(PLATFORM) >nul
  copy $@ ..\bin\$(PLATFORM) >nul
#  copy $@ ..\bin\common >nul

.c.obj:
  wcc386 $(CFLAGS) $<

clean: .SYMBOLIC
    @if exist firecast.exe del *.exe
    @if exist firecast.map del firecast.map
    @if exist *.obj del *.obj
    @if exist *.err del *.err
