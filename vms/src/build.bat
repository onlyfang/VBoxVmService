@echo off
if "%1"=="" goto ERROR
if not "%2"=="" goto ERROR

if "%1"=="release" goto RELEASE

:DEBUG
vcbuild VBoxVmService.sln "Debug|x64"
goto END

:RELEASE
del Release\*.obj
vcbuild VBoxVmService.sln "Release|win32"
copy Release\*.exe ..
del Release\*.obj
vcbuild VBoxVmService.sln "Release|x64"
copy Release\VBoxVmService.exe ..\VBoxVmService64.exe
goto END

:ERROR
echo Usage: build [debug / release]

:END
