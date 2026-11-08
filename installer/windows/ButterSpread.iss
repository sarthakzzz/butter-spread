; Windows installer for Butter Spread (VST3). Built by .github/workflows/release.yml:
;   iscc /DPluginVersion=1.0.0 /DSourceDir=<folder holding Butter Spread.vst3> /DOutputDir=<dist> /DReadme=<README-Windows.txt> ButterSpread.iss
#define PluginName "Butter Spread"
#ifndef PluginVersion
  #define PluginVersion "1.0.0"
#endif

[Setup]
AppId={{Plugins for Breakfast Butter Spread}
AppName={#PluginName}
AppVersion={#PluginVersion}
AppPublisher=Plugins for Breakfast
AppPublisherURL=https://pluginsforbreakfast.com
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
DirExistsWarning=no
UninstallFilesDir={commonappdata}\Plugins for Breakfast\{#PluginName}\uninstall
OutputDir={#OutputDir}
OutputBaseFilename=Butter-Spread-{#PluginVersion}-windows-setup
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
InfoBeforeFile={#Readme}

[Files]
Source: "{#SourceDir}\{#PluginName}.vst3\*"; DestDir: "{commoncf64}\VST3\{#PluginName}.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs

[Messages]
FinishedLabel=Butter Spread is installed. Open your music app, rescan plug-ins, and find it under Plugins for Breakfast.
