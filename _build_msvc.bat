@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
cd /d "C:\Users\gimmy\Projects\Games\Chipmatic-DrillBoost-Mod"
cl /O2 /W2 /Fe:chipmatic_mod.exe src\chipmatic_mod.c /link comctl32.lib shell32.lib
echo BUILD_EXIT=%ERRORLEVEL%
