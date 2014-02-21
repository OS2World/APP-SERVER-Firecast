/* */

'cmd /c clean.cmd'
cur_date = date('S')
files = '..\firecast\*'
cmd = 'pkzip /add /lev=9 /dir=specify /excl=*.zip /excl=*.exe /excl=*.map firecast-sources-' || cur_date || ' ' || files
cmd