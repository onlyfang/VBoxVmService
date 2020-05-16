@echo off
@echo setup build environment
if not defined DevEnvDir (
  call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars32.bat"
)

if not exist build mkdir build
cd build
cmake .. -G "Visual Studio 16 2019" -A Win32
msbuild VBoxVmService.sln /p:Configuration=Release;Platform=Win32 /m
cd ..

if not exist build_x64 mkdir build_x64
cd build_x64
cmake .. -G "Visual Studio 16 2019" -A x64
msbuild VBoxVmService.sln /p:Configuration=Release;Platform=x64 /m
cd ..

"C:\Program Files (x86)\Inno Setup 6\ISCC.exe" setup.iss
