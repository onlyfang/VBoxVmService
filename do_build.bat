@echo off
@echo setup build environment
if defined VS120COMNTOOLS call "%VS120COMNTOOLS%vsvars32.bat"

if not exist build mkdir build
cd build
cmake ..
msbuild VBoxVmService.sln /p:Configuration=Release;Platform=Win32 /m
cd ..

if not exist build_x64 mkdir build_x64
cd build_x64
cmake .. -G "Visual Studio 12 2013 Win64"
msbuild VBoxVmService.sln /p:Configuration=Release;Platform=x64 /m
cd ..

"C:\Program Files (x86)\Inno Setup 5\ISCC.exe" setup.iss
