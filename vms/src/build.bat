@echo off
if "%1"=="" goto ERROR
if not "%2"=="" goto ERROR

if "%1"=="release" goto RELEASE

:DEBUG
msbuild VboxVmService.vcxproj /t:Rebuild /p:Configuration=Debug;Platform=x64
goto END

:RELEASE
msbuild VBoxVmService.vcxproj /t:Rebuild /p:Configuration=Release;Platform=Win32
copy Release\*.exe ..
msbuild VmServiceControl.vcxproj /t:Rebuild /p:Configuration=Release;Platform=Win32
copy Release\*.exe ..

msbuild VBoxVmService.vcxproj /t:Rebuild /p:Configuration=Release;Platform=x64
copy x64\Release\VBoxVmService.exe ..\VBoxVmService64.exe
goto END

:ERROR
echo Usage: build [debug / release]

:END
