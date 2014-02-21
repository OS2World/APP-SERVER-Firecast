# open watcom

!ifndef PLATFORM
PLATFORM = OS2
!endif

OBJS  = c14n.obj parser.obj catalog.obj chvalid.obj           
OBJS += dict.obj                
OBJS += encoding.obj entities.obj error.obj             
OBJS += globals.obj hash.obj                
OBJS += legacy.obj list.obj              
OBJS += nanoftp.obj parser.obj SAX.obj SAX2.obj
OBJS += parserInternals.obj pattern.obj relaxng.obj           
# nanohttp.obj HTMLparser.obj HTMLtree.obj debugXML.obj DOCBparser.obj
OBJS +=                 schematron.obj          threads.obj           
OBJS += tree.obj                trio.obj                
# trionan.obj triostr.obj
OBJS +=              uri.obj                 valid.obj             
OBJS += xinclude.obj            xlink.obj               xmlcatalog.obj        
OBJS += xmlIO.obj               xmllint.obj             xmlmemory.obj         
OBJS += xmlmodule.obj           xmlreader.obj           xmlregexp.obj         
OBJS += xmlsave.obj             xmlschemas.obj          xmlschemastypes.obj   
OBJS += xmlstring.obj           xmlunicode.obj          xmlwriter.obj         
# OBJS += xpath.obj               xpointer.obj            

INCPATH = .;$(%WATCOM)\H\OS2;$(%WATCOM)\H
#INCPATH +=;..\iconv-os2;..\zlib-1.2.3;..\zlib-1.2.3
INCPATH +=;.\include;..\iconv-os2

CFLAGS  = -dHAVE_$(PLATFORM) -i=$(INCPATH) -bt=$(PLATFORM) -s -mf -bm -d0 -q

libxml2.lib: $(OBJS)
  @if exist $@ @del $@
  @for %f in ($(OBJS)) do @wlib -b $* +%f >nul
  copy $@ ..\lib\$(PLATFORM) >nul

encoding.obj:
  wcc386 $(CFLAGS) -aq encoding

!ifeq PLATFORM NT
trio.obj:
  wcc386 $(CFLAGS) -wcd=131 trio
!endif

.c.obj:
  wcc386 $(CFLAGS) $<

clean: .SYMBOLIC
    @if exist *.lnk del *.lnk
    @if exist *.obj del *.obj
    @if exist *.map del *.map
    @if exist *.err del *.err
    @if exist libxml2.lib del libxml2.lib

.BEFORE
  set INCLUDE=$(%WATCOM)\H\$(PLATFORM);$(%WATCOM)\H;.;
  set LIB=$(%WATCOM)\lib386\$(PLATFORM);.;
