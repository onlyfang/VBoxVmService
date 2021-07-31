@echo off
@echo setup build environment
if not defined DevEnvDir (
  call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars32.bat"
)

if not exist build mkdir build
cmake -B build -G "Visual Studio 16 2019" -A Win32
cmake --build build --config Release

if not exist build mkdir build_x64
cmake -B build_x64 -G "Visual Studio 16 2019" -A x64
cmake --build build_x64 --config Release

"C:\Program Files (x86)\Inno Setup 6\ISCC.exe" setup.iss
