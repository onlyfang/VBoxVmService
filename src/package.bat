@echo off

set VERSION=3.1-Coconut

mkdir vms
copy ..\*.exe vms
copy ..\VBoxVmService.ini vms
mkdir vms\doc
copy ..\doc\*.* vms\doc
mkdir vms\src
copy *.bat vms\src
copy *.h vms\src
copy *.c vms\src
copy *.cpp vms\src
copy *.rc vms\src
copy *.sln vms\src
copy *.vcproj vms\src
mkdir vms\src\gfx
copy gfx\*.* vms\src\gfx

zip -r VBoxVmService-%VERSION%.zip vms

del /q vms\src\gfx\*.*
rmdir vms\src\gfx
del /q vms\src\*.*
rmdir vms\src
del /q vms\doc\*.*
rmdir vms\doc
del /q vms\*.*
rmdir vms

