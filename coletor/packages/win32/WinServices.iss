[code]
//  Code pasted from the following address, for examples and more visit it:
//  http://www.vincenzo.net/isxkb/index.php?title=Service_-_Functions_to_Start%2C_Stop%2C_Install%2C_Remove_a_Service


// function IsServiceInstalled(ServiceName: string) : boolean;
// function IsServiceRunning(ServiceName: string) : boolean;
// function InstallService(FileName, ServiceName, DisplayName, Description : string;ServiceType,StartType :cardinal) : boolean;
// function RemoveService(ServiceName: string) : boolean;
// function StartService(ServiceName: string) : boolean;
// function StopService(ServiceName: string) : boolean;
// function SetupService(service, port, comment: string) : boolean;

type
    SERVICE_STATUS = record
        dwServiceType               : cardinal;
        dwCurrentState              : cardinal;
        dwControlsAccepted          : cardinal;
        dwWin32ExitCode             : cardinal;
        dwServiceSpecificExitCode   : cardinal;
        dwCheckPoint                : cardinal;
        dwWaitHint                  : cardinal;
    end;
    HANDLE = cardinal;
    SC_ACTION = record
        actType : cardinal;
        delay   : cardinal;
    end;
    SERVICE_FAILURE_ACTIONS = record
        dwResetPeriod : cardinal;
        lpRebootMsg   : string;
        lpCommand     : string;
        cActions      : cardinal;
        lpsaActions   : array[0..2] of SC_ACTION;
        //lpsaActions   : cardinal;
    end;

const
    SERVICE_QUERY_CONFIG        = $1;
    SERVICE_CHANGE_CONFIG       = $2;
    SERVICE_QUERY_STATUS        = $4;
    SERVICE_START               = $10;
    SERVICE_STOP                = $20;
    SERVICE_ALL_ACCESS          = $f01ff;
    SC_MANAGER_ALL_ACCESS       = $f003f;
    SERVICE_WIN32_OWN_PROCESS   = $10;
    SERVICE_WIN32_SHARE_PROCESS = $20;
    SERVICE_WIN32               = $30;
    SERVICE_INTERACTIVE_PROCESS = $100;
    SERVICE_BOOT_START          = $0;
    SERVICE_SYSTEM_START        = $1;
    SERVICE_AUTO_START          = $2;
    SERVICE_DEMAND_START        = $3;
    SERVICE_DISABLED            = $4;
    SERVICE_DELETE              = $10000;
    SERVICE_CONTROL_STOP        = $1;
    SERVICE_CONTROL_PAUSE       = $2;
    SERVICE_CONTROL_CONTINUE    = $3;
    SERVICE_CONTROL_INTERROGATE = $4;
    SERVICE_STOPPED             = $1;
    SERVICE_START_PENDING       = $2;
    SERVICE_STOP_PENDING        = $3;
    SERVICE_RUNNING             = $4;
    SERVICE_CONTINUE_PENDING    = $5;
    SERVICE_PAUSE_PENDING       = $6;
    SERVICE_PAUSED              = $7;
    SC_ACTION_NONE = 0;
    SC_ACTION_REBOOT = 2;
    SC_ACTION_RESTART = 1;
    SC_ACTION_RUN_COMMAND = 3;
    SERVICE_CONFIG_DESCRIPTION = 1;
    SERVICE_CONFIG_FAILURE_ACTIONS = 2;

// #######################################################################################
// nt based service utilities
// #######################################################################################
function OpenSCManager(lpMachineName, lpDatabaseName: string; dwDesiredAccess :cardinal): HANDLE;
external 'OpenSCManagerA@advapi32.dll stdcall';

function OpenService(hSCManager :HANDLE;lpServiceName: string; dwDesiredAccess :cardinal): HANDLE;
external 'OpenServiceA@advapi32.dll stdcall';

function CloseServiceHandle(hSCObject :HANDLE): boolean;
external 'CloseServiceHandle@advapi32.dll stdcall';

function CreateService(hSCManager :HANDLE;lpServiceName, lpDisplayName: string;dwDesiredAccess,dwServiceType,dwStartType,dwErrorControl: cardinal;lpBinaryPathName,lpLoadOrderGroup: String; lpdwTagId : cardinal;lpDependencies,lpServiceStartName,lpPassword :string): cardinal;external 'CreateServiceA@advapi32.dll stdcall';

function DeleteService(hService :HANDLE): boolean;
external 'DeleteService@advapi32.dll stdcall';

function StartNTService(hService :HANDLE;dwNumServiceArgs : cardinal;lpServiceArgVectors : cardinal) : boolean;
external 'StartServiceA@advapi32.dll stdcall';

function ControlService(hService :HANDLE; dwControl :cardinal;var ServiceStatus :SERVICE_STATUS) : boolean;
external 'ControlService@advapi32.dll stdcall';

function QueryServiceStatus(hService :HANDLE;var ServiceStatus :SERVICE_STATUS) : boolean;
external 'QueryServiceStatus@advapi32.dll stdcall';

function QueryServiceStatusEx(hService :HANDLE;ServiceStatus :SERVICE_STATUS) : boolean;
external 'QueryServiceStatus@advapi32.dll stdcall';

function GetLastError() : cardinal;
external 'GetLastError@kernel32.dll stdcall';

{
function ServiceRestartOnFailure(hService: HANDLE): boolean;
external 'services.dll@files:{app\services.dll stdcall setuponly';     
function ChangeServiceConfig2(hService: HANDLE; dwInfoLevel :cardinal; var lpInfo :pointer): boolean;
external 'ChangeServiceConfig2A@advapi32.dll stdcall';

function ServiceRestartOnFailure(hService: HANDLE): boolean;
var
  ScActions: array[0..2] of SC_ACTION;
  act: SERVICE_FAILURE_ACTIONS;
begin
    ScActions[0].delay := 20000;
    ScActions[0].actType := SC_ACTION_RESTART;
    ScActions[1].delay := 20000;
    ScActions[1].actType := SC_ACTION_RESTART;
    ScActions[2].delay := 20000;
    ScActions[2].actType := SC_ACTION_RESTART;
    
    act.dwResetPeriod := 0;
    act.lpRebootMsg := '';
    act.lpCommand := '';
    act.cActions := 3;
    act.lpsaActions := ScActions;
    ChangeServiceConfig2(hService, SERVICE_CONFIG_FAILURE_ACTIONS, act);
end;
}

function OpenServiceManager() : HANDLE;
begin
    if UsingWinNT() = true then begin
        Result := OpenSCManager('','',SC_MANAGER_ALL_ACCESS);
        if Result = 0 then
            MsgBox('the servicemanager is not available', mbError, MB_OK)
    end
    else begin
            MsgBox('only nt based systems support services', mbError, MB_OK)
            Result := 0;
    end
end;

function IsServiceInstalled(ServiceName: string) : boolean;
var
    hSCM    : HANDLE;
    hService: HANDLE;
begin
    hSCM := OpenServiceManager();
    Result := false;
    if hSCM <> 0 then begin
        hService := OpenService(hSCM,ServiceName,SERVICE_QUERY_CONFIG);
        if hService <> 0 then begin
            Result := true;
            CloseServiceHandle(hService)
        end;
        CloseServiceHandle(hSCM)
    end
end;

function InstallService(FileName, ServiceName, DisplayName, Description : string;ServiceType,StartType :cardinal) : boolean;
var
    hSCM    : HANDLE;
    hService: HANDLE;
begin
    hSCM := OpenServiceManager();
    Result := false;
    if hSCM <> 0 then begin
        hService := CreateService(hSCM,ServiceName,DisplayName,SERVICE_ALL_ACCESS,ServiceType,StartType,0,FileName,'',0,'','','');
        if hService <> 0 then begin
            Result := true;
            // Win2K & WinXP supports aditional description text for services
            if Description<> '' then
                RegWriteStringValue(HKLM,'System\CurrentControlSet\Services\' + ServiceName,'Description',Description);
            //ServiceRestartOnFailure(hService);
            CloseServiceHandle(hService)
        end;
        CloseServiceHandle(hSCM)
    end
end;

function RemoveService(ServiceName: string) : boolean;
var
    hSCM    : HANDLE;
    hService: HANDLE;
begin
    hSCM := OpenServiceManager();
    Result := false;
    if hSCM <> 0 then begin
        hService := OpenService(hSCM,ServiceName,SERVICE_DELETE);
        if hService <> 0 then begin
            Result := DeleteService(hService);
            CloseServiceHandle(hService)
        end;
        CloseServiceHandle(hSCM)
    end
end;

function StartService(ServiceName: string) : boolean;
var
    hSCM    : HANDLE;
    hService: HANDLE;
begin
    hSCM := OpenServiceManager();
    Result := false;
    if hSCM <> 0 then begin
        hService := OpenService(hSCM,ServiceName,SERVICE_START);
        if hService <> 0 then begin
            Result := StartNTService(hService,0,0);
            CloseServiceHandle(hService)
        end;
        CloseServiceHandle(hSCM)
    end;
end;

function StopService(ServiceName: string) : boolean;
var
    hSCM    : HANDLE;
    hService: HANDLE;
    Status  : SERVICE_STATUS;
begin
    hSCM := OpenServiceManager();
    Result := false;
    if hSCM <> 0 then begin
        hService := OpenService(hSCM,ServiceName,SERVICE_STOP);
        if hService <> 0 then begin
            Result := ControlService(hService,SERVICE_CONTROL_STOP,Status);
            CloseServiceHandle(hService)
        end;
        CloseServiceHandle(hSCM)
    end;
end;

function IsServiceRunning(ServiceName: string) : boolean;
var
    hSCM    : HANDLE;
    hService: HANDLE;
    Status  : SERVICE_STATUS;
begin
    hSCM := OpenServiceManager();
    Result := false;
    if hSCM <> 0 then begin
        hService := OpenService(hSCM,ServiceName,SERVICE_QUERY_STATUS);
        if hService <> 0 then begin
            if QueryServiceStatus(hService,Status) then begin
                Result :=(Status.dwCurrentState = SERVICE_RUNNING)
            end;
            CloseServiceHandle(hService)
            end;
        CloseServiceHandle(hSCM)
    end
end;
