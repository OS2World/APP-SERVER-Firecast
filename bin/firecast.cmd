/*
  Make firecast's package (OS/2) for distribution.

  Result will be placed into .\OS2\firecast-NNNNNNNN.zip
*/

fname = 'firecast-' || date('S')

if directory('OS2') = '' then
  say 'Cannot change current directory to ./OS2'
else
  do
   'del 'fname'.map'
   'ren firecast.map 'fname'.*'

   'del 'fname'.zip'
   'pkzip /add /lev=9 'fname' README firecast.exe firecast.ico'
   if directory('..\common') = '' then
     say 'Cannot change current directory to ./common'
   else
     'pkzip /add /lev=9 /dir=cur ..\OS2\'fname' *'
  end
