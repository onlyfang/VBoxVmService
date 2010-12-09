@echo off
if "%1"=="" goto ERROR
if not "%2"=="" goto ERROR

if "%1"=="release" goto RELEASE

:DEBUG
vcbuild VBoxVmService.sln "Debug|win32"
goto END

:RELEASE
vcbuild VBoxVmService.sln "Release|win32"
goto END

:ERROR
echo Usage: build [debug / release]

:END
