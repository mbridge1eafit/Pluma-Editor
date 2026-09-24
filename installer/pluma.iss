; Instalador de Pluma para Windows (Inno Setup 6.3 o posterior).
;
; Compilación (la hace scripts\package_release.ps1):
;   iscc /DAppVersion=0.3.0 installer\pluma.iss
; Opcionales: /DSourceExe=<ruta de pluma.exe>  /DOutputDir=<carpeta de salida>
;
; - Instalación por usuario (%LOCALAPPDATA%\Programs\Pluma), sin permisos de administrador: así Pluma
;   puede actualizarse a sí mismo sin UAC.
; - Una versión nueva se instala encima de la anterior (mismo AppId): conserva la carpeta, las tareas
;   elegidas (asociación de archivos, acceso directo) y la configuración del usuario.
; - Pluma lanza este instalador para actualizarse con:
;     /SILENT /SUPPRESSMSGBOXES /NORESTART /SP- /PLUMAUPDATE=1 [/PLUMAOPEN="documento.md"]
;   El instalador espera a que Pluma termine y, al acabar, vuelve a abrirlo (src/update/updater.cpp).

#ifndef AppVersion
  #error Defina la versión: iscc /DAppVersion=X.Y.Z installer\pluma.iss
#endif
#ifndef SourceExe
  #define SourceExe "..\build\release\bin\pluma.exe"
#endif
#ifndef OutputDir
  #define OutputDir "..\dist"
#endif

#define AppName "Pluma"
#define AppExeName "pluma.exe"
#define AppPublisher "Mauricio Bridge"
#define AppUrl "https://github.com/mbridge1eafit/Pluma-Editor"
#define ProgId "Pluma.Markdown"
; Resource IDs of the icons embedded in pluma.exe (res/resource.h)
#define AppIconId "101"
#define DocIconId "105"

[Setup]
; Never change the AppId: upgrades find the previous installation through it
; (it is also kInstallerAppId in src/update/updater.h).
AppId={{89A612FA-5872-409B-B129-2AA12F350F83}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppUrl}
AppSupportURL={#AppUrl}/issues
AppUpdatesURL={#AppUrl}/releases
VersionInfoVersion={#AppVersion}
VersionInfoDescription=Instalador de {#AppName}
DefaultDirName={autopf}\{#AppName}
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.17763
OutputDir={#OutputDir}
OutputBaseFilename=pluma-v{#AppVersion}-setup-x64
SetupIconFile=..\res\icons\app.ico
UninstallDisplayIcon={app}\{#AppExeName}
UninstallDisplayName={#AppName}
WizardStyle=modern
Compression=lzma2/max
SolidCompression=yes
ChangesAssociations=yes
; Running instances are handled in [Code] (mutex held by pluma.exe).
CloseApplications=no
ShowLanguageDialog=auto

[Languages]
Name: "es"; MessagesFile: "compiler:Languages\Spanish.isl"
Name: "en"; MessagesFile: "compiler:Default.isl"

[CustomMessages]
es.AssociateTask=Abrir los archivos Markdown (.md, .markdown, .mdown) con Pluma de forma predeterminada
en.AssociateTask=Open Markdown files (.md, .markdown, .mdown) with Pluma by default
es.IntegrationGroup=Integración con Windows:
en.IntegrationGroup=Windows integration:
es.ConfirmDefaultApp=Confirmar Pluma como aplicación predeterminada en Configuración de Windows (otra aplicación abre ahora los archivos .md)
en.ConfirmDefaultApp=Confirm Pluma as the default app in Windows Settings (another app currently opens .md files)
es.MarkdownDocument=Documento Markdown
en.MarkdownDocument=Markdown document
es.AppDescription=Editor Markdown nativo para Windows
en.AppDescription=Native Markdown editor for Windows
es.AppRunning=Pluma se está ejecutando.%n%nCierre todas las ventanas de Pluma (guardando sus documentos) y pulse Reintentar.
en.AppRunning=Pluma is running.%n%nClose every Pluma window (saving your documents) and click Retry.

[Tasks]
Name: "associate"; Description: "{cm:AssociateTask}"; GroupDescription: "{cm:IntegrationGroup}"
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#SourceExe}"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\README.md"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\{#AppName}"; Filename: "{app}\{#AppExeName}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExeName}"; Tasks: desktopicon

[Registry]
; Same keys as Pluma::Platform::RegisterMarkdownHandler (src/platform/file_association.cpp), per user.
; Always: Pluma appears in "Abrir con" and in Settings > Default apps.
Root: HKCU; Subkey: "Software\Classes\{#ProgId}"; ValueType: string; ValueName: ""; ValueData: "{cm:MarkdownDocument}"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\{#ProgId}"; ValueType: string; ValueName: "FriendlyTypeName"; ValueData: "{cm:MarkdownDocument}"
Root: HKCU; Subkey: "Software\Classes\{#ProgId}\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#AppExeName},-{#DocIconId}"
Root: HKCU; Subkey: "Software\Classes\{#ProgId}\shell\open"; ValueType: string; ValueName: "FriendlyAppName"; ValueData: "{#AppName}"
Root: HKCU; Subkey: "Software\Classes\{#ProgId}\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#AppExeName}"" ""%1"""

Root: HKCU; Subkey: "Software\Classes\Applications\{#AppExeName}"; ValueType: string; ValueName: "FriendlyAppName"; ValueData: "{#AppName}"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\Applications\{#AppExeName}\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#AppExeName},-{#AppIconId}"
Root: HKCU; Subkey: "Software\Classes\Applications\{#AppExeName}\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#AppExeName}"" ""%1"""
Root: HKCU; Subkey: "Software\Classes\Applications\{#AppExeName}\SupportedTypes"; ValueType: none; ValueName: ".md"
Root: HKCU; Subkey: "Software\Classes\Applications\{#AppExeName}\SupportedTypes"; ValueType: none; ValueName: ".markdown"
Root: HKCU; Subkey: "Software\Classes\Applications\{#AppExeName}\SupportedTypes"; ValueType: none; ValueName: ".mdown"

Root: HKCU; Subkey: "Software\Pluma"; Flags: uninsdeletekeyifempty
Root: HKCU; Subkey: "Software\Pluma\Capabilities"; ValueType: string; ValueName: "ApplicationName"; ValueData: "{#AppName}"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Pluma\Capabilities"; ValueType: string; ValueName: "ApplicationDescription"; ValueData: "{cm:AppDescription}"
Root: HKCU; Subkey: "Software\Pluma\Capabilities"; ValueType: string; ValueName: "ApplicationIcon"; ValueData: "{app}\{#AppExeName},-{#AppIconId}"
Root: HKCU; Subkey: "Software\Pluma\Capabilities\FileAssociations"; ValueType: string; ValueName: ".md"; ValueData: "{#ProgId}"
Root: HKCU; Subkey: "Software\Pluma\Capabilities\FileAssociations"; ValueType: string; ValueName: ".markdown"; ValueData: "{#ProgId}"
Root: HKCU; Subkey: "Software\Pluma\Capabilities\FileAssociations"; ValueType: string; ValueName: ".mdown"; ValueData: "{#ProgId}"
Root: HKCU; Subkey: "Software\RegisteredApplications"; ValueType: string; ValueName: "{#AppName}"; ValueData: "Software\Pluma\Capabilities"; Flags: uninsdeletevalue

Root: HKCU; Subkey: "Software\Classes\.md\OpenWithProgids"; ValueType: none; ValueName: "{#ProgId}"; Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\Classes\.markdown\OpenWithProgids"; ValueType: none; ValueName: "{#ProgId}"; Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\Classes\.mdown\OpenWithProgids"; ValueType: none; ValueName: "{#ProgId}"; Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\Classes\.md"; ValueType: string; ValueName: "Content Type"; ValueData: "text/markdown"; Flags: createvalueifdoesntexist
Root: HKCU; Subkey: "Software\Classes\.markdown"; ValueType: string; ValueName: "Content Type"; ValueData: "text/markdown"; Flags: createvalueifdoesntexist
Root: HKCU; Subkey: "Software\Classes\.mdown"; ValueType: string; ValueName: "Content Type"; ValueData: "text/markdown"; Flags: createvalueifdoesntexist
Root: HKCU; Subkey: "Software\Classes\.md"; ValueType: string; ValueName: "PerceivedType"; ValueData: "text"; Flags: createvalueifdoesntexist
Root: HKCU; Subkey: "Software\Classes\.markdown"; ValueType: string; ValueName: "PerceivedType"; ValueData: "text"; Flags: createvalueifdoesntexist
Root: HKCU; Subkey: "Software\Classes\.mdown"; ValueType: string; ValueName: "PerceivedType"; ValueData: "text"; Flags: createvalueifdoesntexist

; Task "associate": Pluma becomes the default handler of the extensions. Windows applies it unless the
; user explicitly picked another app (UserChoice), which only Settings can change: see [Run].
Root: HKCU; Subkey: "Software\Classes\.md"; ValueType: string; ValueName: ""; ValueData: "{#ProgId}"; Flags: uninsdeletevalue; Tasks: associate
Root: HKCU; Subkey: "Software\Classes\.markdown"; ValueType: string; ValueName: ""; ValueData: "{#ProgId}"; Flags: uninsdeletevalue; Tasks: associate
Root: HKCU; Subkey: "Software\Classes\.mdown"; ValueType: string; ValueName: ""; ValueData: "{#ProgId}"; Flags: uninsdeletevalue; Tasks: associate

[Run]
; Interactive installation
Filename: "ms-settings:defaultapps?registeredAppUser={#AppName}"; Description: "{cm:ConfirmDefaultApp}"; Flags: postinstall shellexec nowait skipifsilent; Tasks: associate; Check: OtherAppIsDefault
Filename: "{app}\{#AppExeName}"; Description: "{cm:LaunchProgram,{#AppName}}"; Flags: postinstall nowait skipifsilent
; Silent upgrade started by Pluma: reopen it with the document that was open
Filename: "{app}\{#AppExeName}"; Parameters: "{code:RelaunchParameters}"; Flags: nowait; Check: IsPlumaUpdate

[Code]
const
  PlumaMutexName = 'PlumaEditorAppMutex'; { Pluma::Update::kAppMutexName }
  UpdateWaitMs = 30000;

function IsPlumaUpdate: Boolean;
begin
  Result := ExpandConstant('{param:PLUMAUPDATE|0}') = '1';
end;

function RelaunchParameters(Param: String): String;
var
  DocumentPath: String;
begin
  DocumentPath := ExpandConstant('{param:PLUMAOPEN|}');
  if (DocumentPath <> '') and FileExists(DocumentPath) then
    Result := AddQuotes(DocumentPath)
  else
    Result := '';
end;

{ Waits until no Pluma process holds the mutex. Returns False when the user cancels
  (or, with /SUPPRESSMSGBOXES, when Pluma is still running). }
function WaitForPlumaToClose(InitialWaitMs: Integer): Boolean;
var
  Waited: Integer;
begin
  Waited := 0;
  while CheckForMutexes(PlumaMutexName) and (Waited < InitialWaitMs) do
  begin
    Sleep(250);
    Waited := Waited + 250;
  end;
  while CheckForMutexes(PlumaMutexName) do
  begin
    Log('Pluma is running.');
    if SuppressibleMsgBox(CustomMessage('AppRunning'), mbError, MB_RETRYCANCEL, IDCANCEL) <> IDRETRY then
    begin
      Result := False;
      exit;
    end;
  end;
  Result := True;
end;

function InitializeSetup: Boolean;
begin
  { Pluma exits right after starting an update: give it time to close. }
  if IsPlumaUpdate then
    Result := WaitForPlumaToClose(UpdateWaitMs)
  else
    Result := WaitForPlumaToClose(0);
end;

function InitializeUninstall: Boolean;
begin
  Result := WaitForPlumaToClose(0);
end;

{ The user's explicit choice overrides the default set by this installer, and Windows only lets the
  user change it in Settings. Recent Windows 11 builds keep it in UserChoiceLatest, older ones in
  UserChoice. }
function OtherAppIsDefault: Boolean;
var
  KeyPath, ProgId: String;
begin
  Result := False;
  KeyPath := 'Software\Microsoft\Windows\CurrentVersion\Explorer\FileExts\.md\';
  if not RegQueryStringValue(HKCU, KeyPath + 'UserChoiceLatest', 'ProgId', ProgId) then
    if not RegQueryStringValue(HKCU, KeyPath + 'UserChoice', 'ProgId', ProgId) then
      exit;
  Result := (CompareText(ProgId, '{#ProgId}') <> 0) and (CompareText(ProgId, 'Applications\{#AppExeName}') <> 0);
end;
