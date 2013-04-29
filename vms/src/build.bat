@echo off
if "%1"=="" goto ERROR
if not "%2"=="" goto ERROR

if "%1"=="release" goto RELEASE

:DEBUG
msbuild VboxVmService.vcxproj /t:Rebuild /p:Configuration=Debug;Platform=x64
msbuild VmServiceTray.vcxproj /t:Rebuild /p:Configuration=Debug;Platform=Win32
goto END

:RELEASE
msbuild VBoxVmService.vcxproj /t:Rebuild /p:Configuration=Release;Platform=Win32
copy Release\VBoxVmService.exe ..
msbuild VBoxVmService.vcxproj /t:Rebuild /p:Configuration=Release;Platform=x64
copy x64\Release\VBoxVmService.exe ..\VBoxVmService64.exe
msbuild VmServiceControl.vcxproj /t:Rebuild /p:Configuration=Release;Platform=Win32
copy Release\VmServiceControl.exe ..
msbuild VmServiceTray.vcxproj /t:Rebuild /p:Configuration=Release;Platform=Win32
copy Release\VmServiceTray.exe ..

goto END

:ERROR
echo Usage: build [debug / release]

:END
