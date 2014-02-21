/* make documentation */

'xsltproc --nonet docbook.xsl firecast.xml'

fname = 'firecast-doc-ru-' || date('S')
'pkzip /add /lev=9 /dir=cur ' || fname || ' *.html'


