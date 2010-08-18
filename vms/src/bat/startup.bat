@echo off

rem #  INIT THE TOKEN BUILDER VARIABLES
set cyear=
set cdate=
set chours=
set ctime=

rem #  ASSIGN THE BASIC INFOS WE GOT FROM THE STARTUP CALL
set VmId=%1%
set VMSInstallPath=%cd%

goto START


rem #  SETUP SOME HELPER METHODS AND JUMPPOINTS
:WAIT_FOR_SHUTDOWN
"%VMSInstallPath%\tools\sleep.exe" 3
echo Waiting for a previous shutdown of VM%VmId% to complete... >> "%VMSInstallPath%\VBoxVmService.log"
goto START


rem #  ACTUAL SCRIPT PROCESSING STARTS HERE
:START

rem #  CHECK FOR THE BRUTEFORCE OPTION
if "%2%" == "BRUTEFORCE" goto BRUTEFORCE
goto NOBRUTEFORCE


:BRUTEFORCE
rem #  KILL ANY POSSIBLY EXISTING RUNFILES AND/OR LOCKFILES BEFORE STARTUP
echo BRUTEFORCE: Killing all existing run- / lockfiles for this VM...  >> "%VMSInstallPath%\VBoxVmService.log"

if exist "%VMSInstallPath%\run\*.vm%VmId%" echo killing: "%VMSInstallPath%\run\*.vm%VmId%" >> "%VMSInstallPath%\VBoxVmService.log"
if exist "%VMSInstallPath%\run\*.vm%VmId%" del "%VMSInstallPath%\run\*.vm%VmId%"

if exist "%VMSInstallPath%\lock\*.UP.vm%VmId%" echo killing: "%VMSInstallPath%\lock\*.UP.vm%VmId%" >> "%VMSInstallPath%\VBoxVmService.log"
if exist "%VMSInstallPath%\lock\*.UP.vm%VmId%" del "%VMSInstallPath%\lock\*.UP.vm%VmId%"

if exist "%VMSInstallPath%\lock\*.DOWN.vm%VmId%" echo killing: "%VMSInstallPath%\lock\*.DOWN.vm%VmId%" >> "%VMSInstallPath%\VBoxVmService.log"
if exist "%VMSInstallPath%\lock\*.DOWN.vm%VmId%" del "%VMSInstallPath%\lock\*.DOWN.vm%VmId%"

goto SKIPCHECK


:NOBRUTEFORCE
rem #  CHECK FOR A LOCKFILE SET BY A PREVIOUSLY TRIGGERED SHUTDOWN CALL TO THIS VM
if exist "%VMSInstallPath%\run\*.vm%VmId%" goto ABORT_ALREADY_RUNNING
if exist "%VMSInstallPath%\lock\*.UP.vm%VmId%" goto ABORT_ALREADY_RUNNING_STARTUP
if exist "%VMSInstallPath%\lock\*.DOWN.vm%VmId%" goto WAIT_FOR_SHUTDOWN

:SKIPCHECK

FOR /F "tokens=1,2,3,4 delims=/. " %%a in ('date/T') do set cyear=%%c%%d
FOR /F "tokens=1,2,3,4 delims=/. " %%a in ('date/T') do set cdate=%%b%%a
FOR /F "tokens=1,2,3,4 delims=: " %%a in ('time/T') do set chours=%%a
FOR /F "tokens=1,2,3,4 delims=:, " %%a in ('echo %time%') do set ctime=%%b%%c%%d
echo 1 > "%VMSInstallPath%\lock\%cyear%%cdate%%chours%%ctime%.UP.vm%VmId%"
set tokenid=%cyear%%cdate%%chours%%ctime%


rem #  CHECK FOR LOCKFILES OF PREVIOUSLY TRIGGERED OPERATIONS
goto LOOP

:WAIT_FOR_PREVIOUS
echo Waiting for previously triggered operations to finish... >> "%VMSInstallPath%\VBoxVmService.log"
"%VMSInstallPath%\tools\sleep.exe" 3

:LOOP
dir /b /o:n "%VMSInstallPath%\lock\" > "%VMSInstallPath%\tmp\vm%VmId%up.chk"
set /P continue=<"%VMSInstallPath%\tmp\vm%VmId%up.chk"
if not "%continue%"=="%tokenid%.UP.vm%VmId%" goto WAIT_FOR_PREVIOUS

del "%VMSInstallPath%\tmp\vm%VmId%up.chk"

echo No previously started VM operations to wait for. Continuing... >> "%VMSInstallPath%\VBoxVmService.log"


rem #  RESETTING ALL VARIABLES (NOT TO MESS WITH ANY LEFTOVERS)
set PathToVBoxManage=%VBOX_INSTALL_PATH%VBoxManage.exe
set VmName=
set StartupMethod=
set ShutdownMethod=
set VrdpPort=


rem #  POPULATING ALL VARIABLES
echo Reading global parameters... >> "%VMSInstallPath%\VBoxVmService.log"
"%VMSInstallPath%\tools\inifile.exe" VBoxVmService.ini [Settings] > "%VMSInstallPath%\tmp\u_%VmId%_globals.bat"
call "%VMSInstallPath%\tmp\u_%VmId%_globals.bat"
del "%VMSInstallPath%\tmp\u_%VmId%_globals.bat"

echo Reading parameters for VM%VmId%... >> "%VMSInstallPath%\VBoxVmService.log"
"%VMSInstallPath%\tools\inifile.exe" VBoxVmService.ini [Vm%VmId%] > "%VMSInstallPath%\tmp\u_%VmId%_vmvals.bat"
call "%VMSInstallPath%\tmp\u_%VmId%_vmvals.bat"
del "%VMSInstallPath%\tmp\u_%VmId%_vmvals.bat"

rem #  CAST THE VM INFOS TO THE LOGFILE
call "%PathToVBoxManage%" showvminfo "%VmName%" >> "%VMSInstallPath%\VBoxVmService.log"

rem #  VALIDATE THE OBTAINED VALUES
if "%VmName%" == "" goto NORUN
if "%StartupMethod%" == "" goto NORUN
if "%ShutdownMethod%" == "" goto NORUN


rem #  DECIDING WHICH STARTUP METHOD TO USE
if "%StartupMethod%" == "vrdp" goto STARTUP_VRDP
if "%StartupMethod%" == "VRDP" goto STARTUP_VRDP
goto NORUN

:STARTUP_VRDP
if "%VrdpPort%" == "" goto STARTUP_VRDP_DEFAULTPORT
if "%VrdpPort%" == "0" goto STARTUP_VRDP_DEFAULTPORT
if "%VrdpPort%" == "3389" goto STARTUP_VRDP_DEFAULTPORT

echo Using custom VRDP-Port: %VrdpPort% >> "%VMSInstallPath%\VBoxVmService.log"
goto STARTUP_VRDP_STEP_2

:STARTUP_VRDP_DEFAULTPORT
echo Using default VRDP-Port (might be problematic!): 3389 >> "%VMSInstallPath%\VBoxVmService.log"

:STARTUP_VRDP_STEP_2
call "%PathToVBoxManage%" modifyvm "%VmName%" --vrdpport %VrdpPort% >> "%VMSInstallPath%\VBoxVmService.log"
call "%PathToVBoxManage%" startvm "%VmName%" --type vrdp >> "%VMSInstallPath%\VBoxVmService.log"


rem # NOW CALL THE ACTUAL STARTUP COMMANDS
echo Starting VM: "%VmName%" via %StartupMethod% as a service ... >> "%VMSInstallPath%\VBoxVmService.log"
"%VMSInstallPath%\tools\sleep.exe" 15
goto MARK_RUNNING

:NORUN
echo Errors occurred trying to start VM%VmId%! >> "%VMSInstallPath%\VBoxVmService.log"
echo Check the VBoxVmServ.ini values, sections [Settings] and [Vm%VmId%]! >> "%VMSInstallPath%\VBoxVmService.log"
del "%VMSInstallPath%\lock\%tokenid%.UP.vm%VmId%"
echo Removing lockfile: STARTUP  >> "%VMSInstallPath%\VBoxVmService.log"
set NoRun=%VmId%
goto CLEANUP


:MARK_RUNNING
rem # MOVE THE STARTUP LOCKFILE TO A RUN LOCKFILE
move "%VMSInstallPath%\lock\%tokenid%.UP.vm%VmId%" "%VMSInstallPath%\run\%tokenid%.vm%VmId%"
echo Moving lockfile: STARTUP to RUN  >> "%VMSInstallPath%\VBoxVmService.log"


rem # DO SOME FINAL CLEANUP ON FILES
:CLEANUP

del "%VMSInstallPath%\tmp\vm%VmId%up.chk"

rem # AND ON THE VARIABLES 
set StartupMethod=
set ShutdownMethod=
set VmName=
set VmId=
set NoRun=
set VMSInstallPath=

goto DONE


:ABORT_ALREADY_RUNNING
echo This VM seems to be currently running. Startup call aborted. >> "%VMSInstallPath%\VBoxVmService.log"
goto END


:ABORT_ALREADY_RUNNING_STARTUP
echo This VM seems to be currently in startup. Additional startup call aborted. >> "%VMSInstallPath%\VBoxVmService.log"
goto END


:DONE

:END
