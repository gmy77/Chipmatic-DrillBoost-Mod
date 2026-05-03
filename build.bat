@echo off
setlocal

:: Cerca gcc (MinGW64/UCRT64 via MSYS2 o PATH)
set FOUND=0
for %%G in (
    "C:\msys64\mingw64\bin\gcc.exe"
    "C:\msys64\ucrt64\bin\gcc.exe"
    "C:\mingw64\bin\gcc.exe"
    "C:\MinGW\bin\gcc.exe"
) do (
    if exist %%G (
        set GCC=%%~G
        set FOUND=1
        goto :compile
    )
)

:: Prova anche gcc nel PATH
where gcc >nul 2>&1
if %errorlevel%==0 (
    set GCC=gcc
    set FOUND=1
    goto :compile
)

echo [ERRORE] gcc non trovato.
echo Installa MinGW64 via MSYS2: pacman -S mingw-w64-x86_64-gcc
pause
exit /b 1

:compile
echo Compilando con: %GCC%
"%GCC%" -O2 -o chipmatic_mod.exe src\chipmatic_mod.c ^
    -mwindows -lcomctl32 -lshell32 -luser32 -lgdi32 -lm

if %errorlevel%==0 (
    echo.
    echo  OK  chipmatic_mod.exe pronto!
    echo.
    echo  Esegui:  chipmatic_mod.exe
) else (
    echo.
    echo [ERRORE] Compilazione fallita.
)
pause
