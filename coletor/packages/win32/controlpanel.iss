
[Tasks]
Name: "cpl"; Description: "Adicionar Configurador do Sistema no Painel de Controle"; GroupDescription: "Configurações"

[Registry]
#define CplEntryName "pgAnalytics"
#define CplInfoTip "Configurar o Coletor do pgAnalytics"
#define CplSysAppName "{{#MyAppName}"
#define CplSysCategory "0,5"
#define CplIcon "{app}\{{#MyAppExeName},0"
#define CplShellCmd "{app}\{{#MyAppExeName}.exe config"
; Create a icon at the control panel
; From: http://msdn.microsoft.com/en-us/library/bb776843(VS.85).aspx#exe_reg
; Author: Matheus de Oliveira <matioli.matheus@gmail.com>
Root: HKLM; Subkey: "SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\ControlPanel\NameSpace\{#AppGUID}"; Flags: uninsdeletekey; Tasks:cpl
Root: HKLM; Subkey: "SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\ControlPanel\NameSpace\{#AppGUID}"; ValueType: string; ValueName: ""; ValueData: "{#MyAppVersion}"; Tasks:cpl
Root: HKCR; Subkey: "CLSID\{#AppGUID}"; Flags: uninsdeletekey; Tasks:cpl
Root: HKCR; Subkey: "CLSID\{#AppGUID}"; ValueType: string; ValueName: "";                             ValueData: "{#CplEntryName}"; Tasks:cpl
;Root: HKCR; Subkey: "CLSID\{#AppGUID}"; ValueType: expandsz; ValueName: "LocalizedString";             ValueData: "@{app}\Configurador.exe, -1"
;Root: HKCR; Subkey: "CLSID\{#AppGUID}"; ValueType: string; ValueName: "System.Software.TasksFileUrl"; ValueData: "{app}\CPLTask.xml"
Root: HKCR; Subkey: "CLSID\{#AppGUID}"; ValueType: string; ValueName: "InfoTip";                      ValueData: "{#CplInfoTip}"; Tasks:cpl
Root: HKCR; Subkey: "CLSID\{#AppGUID}"; ValueType: string; ValueName: "System.ApplicationName";       ValueData: "{#CplSysAppName}"; Tasks:cpl
Root: HKCR; Subkey: "CLSID\{#AppGUID}"; ValueType: string; ValueName: "System.ControlPanel.Category"; ValueData: "{#CplSysCategory}"; Tasks:cpl
Root: HKCR; Subkey: "CLSID\{#AppGUID}\DefaultIcon"; Flags: uninsdeletekey; Tasks:cpl
Root: HKCR; Subkey: "CLSID\{#AppGUID}\DefaultIcon"; Flags: uninsdeletekey; ValueType: string; ValueName: ""; ValueData: "{#CplIcon}"; Tasks:cpl
Root: HKCR; Subkey: "CLSID\{#AppGUID}\Shell";              Flags: uninsdeletekey; Tasks:cpl
Root: HKCR; Subkey: "CLSID\{#AppGUID}\Shell\Open";         Flags: uninsdeletekey; Tasks:cpl
Root: HKCR; Subkey: "CLSID\{#AppGUID}\Shell\Open\Command"; Flags: uninsdeletekey; Tasks:cpl
Root: HKCR; Subkey: "CLSID\{#AppGUID}\Shell\Open\Command"; Flags: uninsdeletekey; ValueType: string; ValueName: ""; ValueData: "{#CplShellCmd}"; Tasks:cpl
;Root: HKLM; Subkey: "SOFTWARE\Microsoft\Windows\CurrentVersion\Run"; Flags: uninsdeletekey; ValueType: string; ValueName: "mPlan Autobackup"; ValueData: "{app}\mPlan.exe -autobackup"; Tasks:autobkp

