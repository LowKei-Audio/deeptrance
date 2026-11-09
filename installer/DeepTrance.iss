; DeepTrance Windows installer (Inno Setup 6).
; Built by CI: iscc /DAppVersion=1.0.1 /DBuildDir=..\build installer\DeepTrance.iss
; Installs the VST3 and CLAP plug-ins to the standard shared folders (all DAWs
; scan them) and, optionally, the standalone app with a Start menu shortcut.

#ifndef AppVersion
  #define AppVersion "1.0.1"
#endif
#ifndef BuildDir
  #define BuildDir "..\build"
#endif
#define Artefacts BuildDir + "\DeepTrance_artefacts\Release"

[Setup]
AppId={{6F3A9C2E-7D41-4B8A-9E2D-DEE9712A2CE0}
AppName=DeepTrance
AppVersion={#AppVersion}
AppVerName=DeepTrance {#AppVersion}
AppPublisher=LowKei
DefaultDirName={autopf}\LowKei\DeepTrance
DefaultGroupName=DeepTrance
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
OutputDir=Output
OutputBaseFilename=DeepTrance-{#AppVersion}-Windows-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName=DeepTrance {#AppVersion}

[Types]
Name: "full"; Description: "VST3 and CLAP plug-ins and standalone app"
Name: "vst3"; Description: "Plug-ins only"
Name: "custom"; Description: "Custom"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 plug-in (Ableton Live, FL Studio, Serato Studio)"; Types: full vst3 custom; Flags: fixed
Name: "clap"; Description: "CLAP plug-in (FL Studio, Bitwig Studio, Reaper)"; Types: full vst3 custom
Name: "standalone"; Description: "Standalone app (no DAW needed)"; Types: full

[Files]
Source: "{#Artefacts}\VST3\DeepTrance.vst3\*"; DestDir: "{commoncf64}\VST3\DeepTrance.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
; CLAP is a single file in the shared CLAP folder.
Source: "{#Artefacts}\CLAP\DeepTrance.clap"; DestDir: "{commoncf64}\CLAP"; Components: clap; Flags: ignoreversion
Source: "{#Artefacts}\Standalone\DeepTrance.exe"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion
Source: "..\docs\DeepTrance_User_Guide.pdf"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\docs\README.txt"; DestDir: "{app}"; Flags: ignoreversion isreadme

[Icons]
Name: "{group}\DeepTrance"; Filename: "{app}\DeepTrance.exe"; Components: standalone
Name: "{group}\DeepTrance User Guide"; Filename: "{app}\DeepTrance_User_Guide.pdf"
Name: "{group}\Uninstall DeepTrance"; Filename: "{uninstallexe}"

[Run]
Filename: "{app}\DeepTrance_User_Guide.pdf"; Description: "Open the User Guide"; Flags: postinstall shellexec skipifsilent unchecked
