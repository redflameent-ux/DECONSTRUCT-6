; Inno Setup script - builds DECONSTRUCT-6-Setup.exe
[Setup]
AppName=DECONSTRUCT-6
AppVersion=0.1.0
AppPublisher=TRANCE-6
DefaultDirName={autopf}\DECONSTRUCT-6
DefaultGroupName=DECONSTRUCT-6
OutputBaseFilename=DECONSTRUCT-6-Setup
OutputDir=Output
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
Compression=lzma2
SolidCompression=yes
DisableProgramGroupPage=yes
WizardStyle=modern

[Files]
; The VST3 plugin goes where every DAW (including FL Studio and Ableton) looks for it
Source: "..\build\DC6_artefacts\Release\VST3\DECONSTRUCT-6.vst3\*"; DestDir: "{commoncf64}\VST3\DECONSTRUCT-6.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
; Stand-alone app for trying it on your mic / audio input without a DAW
Source: "..\build\DC6_artefacts\Release\Standalone\DECONSTRUCT-6.exe"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\DECONSTRUCT-6 (stand-alone)"; Filename: "{app}\DECONSTRUCT-6.exe"
