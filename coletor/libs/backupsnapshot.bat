@echo off
SETLOCAL EnableDelayedExpansion enableextensions

set DATABASENAME=%2
set BACKUPPATH=%4
set BACKUPPART=%5
set INSTANCENAME=%7
set BACKUPTYPE=%9
set BaseDir=%0
call :DeQuote BaseDir
cd %BaseDir%\..

for /F "tokens=1,2 delims= " %%i in (pganalytics.conf) DO ( 
	set COL1=%%i
	set COL2=%%j
	IF %%i==customer (
		set CUSTOMERNAME=%%j
		call :DeQuote CUSTOMERNAME
	) 
	IF %%i==server_name (
		set SERVERNAME=%%j
		call :DeQuote SERVERNAME
	) 
	IF %%i==collect_dir (
		set COLLECTDIR=%%j
		call :DeQuote COLLECTDIR
	) 
)

set TAB=	
set RUNNING_BACKUP_FILE=%COLLECTDIR%\pganalytics-%DATABASENAME%-backup.running
IF %BACKUPTYPE%==begin (
	echo %RUNNING_BACKUP_FILE% generated
	call :import_backup_p1 > %RUNNING_BACKUP_FILE%
	exit /b %ERRORLEVEL%
)
IF %BACKUPTYPE%==end (
	call :import_backup_p2 TIME_EPOCH >> %RUNNING_BACKUP_FILE%
	gzip.exe -c %RUNNING_BACKUP_FILE% > %COLLECTDIR%/new/!TIME_EPOCH!-000000-0000-pgdump-%DATABASENAME%.pga
	echo %COLLECTDIR%/new/!TIME_EPOCH!-000000-0000-pgdump-%DATABASENAME%.pga
	exit /b %ERRORLEVEL%
)
exit /b

:import_backup_p1
setlocal enableextensions
set snap_type=pg_dump
call :getTime
set date_begin=%ldt%
call :GetUnixTime TIME_EPOCH
echo # snap_type %snap_type%
echo # customer_name %CUSTOMERNAME%
echo # server_name %SERVERNAME%
echo # datetime %TIME_EPOCH%
echo # real_datetime %TIME_EPOCH%
echo # instance_name %INSTANCENAME%
echo # datname %DATABASENAME%
echo.
echo K sn_data_info data_key,data_value
echo BACKUP_BEGIN%TAB%%date_begin%
echo BACKUP_FILE%TAB%%BACKUPPATH%
echo BACKUP_PART%TAB%%BACKUPPART%
endlocal & goto :EOF

:import_backup_p2
setlocal enableextensions
call :getFilesize %BACKUPPATH%
call :getTime
set date_end=%ldt%
call :GetUnixTime TIME_EPOCH
echo BACKUP_SIZE%TAB%%fileSize%
echo BACKUP_END%TAB%%date_end%
echo \.
endlocal & set "%1=%TIME_EPOCH%" & goto :EOF

:getTime
for /F "usebackq tokens=1,2 delims==" %%i in (`wmic os get LocalDateTime /VALUE 2^>NUL`) do if '.%%i.'=='.LocalDateTime.' set ldt=%%j
set ldt=%ldt:~0,4%-%ldt:~4,2%-%ldt:~6,2% %ldt:~8,2%:%ldt:~10,2%:%ldt:~12,6%
exit /b

:getFilesize
set filesize=%~z1
exit /b

:DeQuote
for /f "delims=" %%A in ('echo %%%1%%') do set %1=%%~A
exit /b

:GetUnixTime
setlocal enableextensions
for /f %%x in ('wmic path win32_utctime get /format:list ^| findstr "="') do (
    set %%x)
set /a z=(14-100%Month%%%100)/12, y=10000%Year%%%10000-z
set /a ut=y*365+y/4-y/100+y/400+(153*(100%Month%%%100+12*z-3)+2)/5+Day-719469
set /a ut=ut*86400+100%Hour%%%100*3600+100%Minute%%%100*60+100%Second%%%100
endlocal & set "%1=%ut%" & goto :EOF

exit /b %ERRORLEVEL%



