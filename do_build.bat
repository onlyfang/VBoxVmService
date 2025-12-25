@echo off
@echo setup build environment
if not defined DevEnvDir (
  call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
)

if not exist build mkdir build
cmake -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Release

"C:\Program Files (x86)\Inno Setup 6\ISCC.exe" setup.iss
