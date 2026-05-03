@echo off
set VSBASE=C:\Program Files\Microsoft Visual Studio\18\Community
set MSVCVER=14.44.35207
set KITVER=10.0.26100.0
set KITSBASE=C:\Program Files (x86)\Windows Kits\10

set INCLUDE=%VSBASE%\VC\Tools\MSVC\%MSVCVER%\include;%KITSBASE%\Include\%KITVER%\ucrt;%KITSBASE%\Include\%KITVER%\um;%KITSBASE%\Include\%KITVER%\shared
set LIB=%VSBASE%\VC\Tools\MSVC\%MSVCVER%\lib\x64;%KITSBASE%\Lib\%KITVER%\ucrt\x64;%KITSBASE%\Lib\%KITVER%\um\x64

set CL="%VSBASE%\VC\Tools\MSVC\%MSVCVER%\bin\HostX64\x64\cl.exe"

cd /d "C:\Users\gimmy\Projects\Games\Chipmatic-DrillBoost-Mod"
%CL% /O2 /W2 src\chipmatic_mod.c /Fe:chipmatic_mod.exe /link comctl32.lib shell32.lib /SUBSYSTEM:WINDOWS
echo BUILD_RESULT=%ERRORLEVEL%
