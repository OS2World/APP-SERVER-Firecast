[Setup]
AppName=Firecast
AppVerName=Firecast 200804005
DefaultDirName={pf}\Firecast
SourceDir=.\NT
Compression=lzma/max
;Compression=none
;DisableFinishedPage=yes
DisableReadyPage=yes
RestartIfNeededByRun=no
Uninstallable=yes
DefaultGroupName="Firecast"

[Files]
Source: "firecast.exe"; DestDir: "{app}"
Source: "firecast.url"; DestDir: "{app}"
Source: "firecast.ico"; DestDir: "{app}"
Source: "firecast_reg.ico"; DestDir: "{app}"
Source: "firecast_rem.ico"; DestDir: "{app}"
Source: "..\common\*"; DestDir: "{app}"; Flags: recursesubdirs

[Icons]
Name: "{group}\Firecast (console mode)"; Filename: "{app}\Firecast.exe"; IconFilename: "{app}\firecast.ico";
Name: "{group}\Firecast control"; Filename: "{app}\Firecast.url"
Name: "{group}\Register Firecast as system service"; Filename: "{app}\Firecast.exe"; Parameters: "/install"; IconFilename: "{app}\firecast_reg.ico"; Flags: runminimized
Name: "{group}\Remove Firecast service"; Filename: "{app}\Firecast.exe"; Parameters: "/remove"; IconFilename: "{app}\firecast_rem.ico"; Flags: runminimized
Name: "{group}\Uninstall"; Filename: "{uninstallexe}" 
; IconFilename: "{app}\firecast.ico"

[Run]
Filename: "{app}\Firecast.exe"; Parameters: "/install"; Description: "Register service"; StatusMsg: "Installing service..."; Flags: postinstall runminimized

[UninstallRun]
Filename: "{app}\Firecast.exe"; Parameters: "/remove /nomb"; StatusMsg: "Remove service..."; Flags: runminimized

[UninstallDelete]
Type: files; Name: "{app}\config.xml"
Type: files; Name: "{app}\config.BAK"
