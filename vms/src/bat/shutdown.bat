@echo off

rem #  INIT THE TOKEN BUILDER VARIABLES
set cyear=
set cdate=
set chours=
set ctime=

rem #  ASSIGN THE BASIC INFOS WE GOT FROM THE SHUTDOWN CALL
set VmId=%1%
set VMSInstallPath=%cd%

goto START


rem #  SETUP SOME HELPER METHODS AND JUMPPOINTS
:WAIT_FOR_STARTUP
"%VMSInstallPath%\tools\sleep.exe" 3
echo Waiting for a previous startup of VM%VmId% to complete... >> "%VMSInstallPath%\VBoxVmService.log"
goto START


rem #  ACTUAL SCRIPT PROCESSING STARTS HERE
:START


rem #  CHECK FOR A LOCKFILE SET BY A PREVIOUSLY TRIGGERED SHUTDOWN CALL TO THIS VM

if not exist "%VMSInstallPath%\run\*.vm%VmId%" goto ABORT_NOT_RUNNING

if exist "%VMSInstallPath%\lock\*.DOWN.vm%VmId%" goto ABORT_ALREADY_RUNNING_SHUTDOWN

if exist "%VMSInstallPath%\lock\*.UP.vm%VmId%" goto WAIT_FOR_STARTUP


FOR /F "tokens=1,2,3,4 delims=/. " %%a in ('date/T') do set cyear=%%c%%d
FOR /F "tokens=1,2,3,4 delims=/. " %%a in ('date/T') do set cdate=%%b%%a
FOR /F "tokens=1,2,3,4 delims=: " %%a in ('time/T') do set chours=%%a
FOR /F "tokens=1,2,3,4 delims=:, " %%a in ('echo %time%') do set ctime=%%b%%c%%d
echo 1 > "%VMSInstallPath%\lock\%cyear%%cdate%%chours%%ctime%.DOWN.vm%VmId%"
set tokenid=%cyear%%cdate%%chours%%ctime%


rem #  CHECK FOR LOCKFILES OF PREVIOUSLY TRIGGERED OPERATIONS
goto LOOP

:WAIT_FOR_PREVIOUS
echo Waiting for previously triggered operations to finish... >> "%VMSInstallPath%\VBoxVmService.log"
"%VMSInstallPath%\tools\sleep.exe" 3

:LOOP
dir /b /o:n "%VMSInstallPath%\lock\" > "%VMSInstallPath%\tmp\vm%VmId%down.chk"
set /P continue=<"%VMSInstallPath%\tmp\vm%VmId%down.chk"
if not "%continue%"=="%tokenid%.DOWN.vm%VmId%" goto WAIT_FOR_PREVIOUS

del "%VMSInstallPath%\tmp\vm%VmId%down.chk"

echo No previously started VM operations to wait for. Continuing... >> "%VMSInstallPath%\VBoxVmService.log"


rem #  RESETTING ALL VARIABLES (NOT TO MESS WITH ANY LEFTOVERS)
set PathToVBoxManage=%VBOX_INSTALL_PATH%VBoxManage.exe
set VmName=
set StartupMethod=
set ShutdownMethod=


rem #  POPULATING ALL VARIABLES
echo Reading global parameters... >> "%VMSInstallPath%\VBoxVmService.log"
"%VMSInstallPath%\tools\inifile.exe" VBoxVmService.ini [Settings] > "%VMSInstallPath%\tmp\d_%VmId%_globals.bat"
call "%VMSInstallPath%\tmp\d_%VmId%_globals.bat"
del "%VMSInstallPath%\tmp\d_%VmId%_globals.bat"

echo Reading parameters for VM%VmId%... >> "%VMSInstallPath%\VBoxVmService.log"
"%VMSInstallPath%\tools\inifile.exe" VBoxVmService.ini [Vm%VmId%] > "%VMSInstallPath%\tmp\d_%VmId%_vmvals.bat"
call "%VMSInstallPath%\tmp\d_%VmId%_vmvals.bat"
del "%VMSInstallPath%\tmp\d_%VmId%_vmvals.bat"

rem #  VALIDATE THE OBTAINED VALUES
if "%VmName%" == "" goto NORUN
if "%StartupMethod%" == "" goto NORUN
if "%ShutdownMethod%" == "" goto NORUN

echo Shutting down serviced VM: "%VmName%" with option %ShutdownMethod% ... >> "%VMSInstallPath%\VBoxVmService.log"
call "%PathToVBoxManage%" controlvm "%VmName%" "%ShutdownMethod%" >> "%VMSInstallPath%\VBoxVmService.log"


rem # NOW CALL THE ACTUAL SHUTDOWN COMMANDS
"%VMSInstallPath%\tools\sleep.exe" 15
goto MARK_SHUTDOWN

:NORUN
echo Errors occurred trying to shut down VM%VmId%! >> "%VMSInstallPath%\VBoxVmService.log"
echo Check the VBoxVmServ.ini values, sections [Settings] and [Vm%VmId%]! >> "%VMSInstallPath%\VBoxVmService.log"
set NoRun=%VmId%
goto CLEANUP


:MARK_SHUTDOWN
rem # REMOVE THE RUN LOCKFILE
del "%VMSInstallPath%\run\*.vm%VmId%"
del "%VMSInstallPath%\lock\*.DOWN.vm%VmId%"
echo Removing lockfile: RUN  >> "%VMSInstallPath%\VBoxVmService.log"


rem # DO SOME FINAL CLEANUP ON FILES
:CLEANUP

del "%VMSInstallPath%\tmp\vm%VmId%down.chk"

rem # AND ON THE VARIABLES 
set StartupMethod=
set ShutdownMethod=
set VmName=
set VmId=
set NoRun=
set VMSInstallPath=

goto DONE


:ABORT_NOT_RUNNING
echo This VM does not seem to be currently running. Shutdown call aborted. >> "%VMSInstallPath%\VBoxVmService.log"
goto END


:ABORT_ALREADY_RUNNING_SHUTDOWN
echo This VM seems to be currently in shutdown. Additional shutdown call aborted. >> "%VMSInstallPath%\VBoxVmService.log"
goto END


:DONE


:END
