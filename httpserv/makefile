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

!ifndef PLATFORM
PLATFORM = OS2
!endif

OBJS = filehost.obj fileres.obj httpserv.obj netacl.obj httpclnt.obj
OBJS += netmessages.obj netresolv.obj netserv.obj reqfields.obj requests.obj

CFLAGS = -i=$(%WATCOM)\H\$(PLATFORM);$(%WATCOM)\H;.\include;..\system -bt=$(PLATFORM) -d0 -q -ot -bm -s 

# OBJS += a.obj
# a.exe: $(OBJS)
#   wlink system os2v2 file {$(OBJS:.obj=)} libpath ..\lib\os2 lib {system} name $* op map=$* op el

httpserv.lib: $(OBJS)
  @if exist $@ @del $@
  @for %f in ($(OBJS)) do @wlib -b $* +%f >nul
  copy $@ ..\lib\$(PLATFORM) >nul

.c.obj:
  wcc386 $(CFLAGS) $<

clean: .SYMBOLIC
    @if exist httpserv.lib del httpserv.lib
    @if exist *.obj del *.obj
    @if exist *.err del *.err
