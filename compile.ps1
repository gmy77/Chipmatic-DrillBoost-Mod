$vsbase  = 'C:\Program Files\Microsoft Visual Studio\18\Community'
$msvcver = '14.44.35207'
$kitver  = '10.0.26100.0'
$kits    = 'C:\Program Files (x86)\Windows Kits\10'

$env:INCLUDE = "$vsbase\VC\Tools\MSVC\$msvcver\include;" +
               "$kits\Include\$kitver\ucrt;" +
               "$kits\Include\$kitver\um;" +
               "$kits\Include\$kitver\shared;" +
               "$kits\Include\$kitver\winrt"

$env:LIB = "$vsbase\VC\Tools\MSVC\$msvcver\lib\x64;" +
           "$kits\Lib\$kitver\ucrt\x64;" +
           "$kits\Lib\$kitver\um\x64"

$env:PATH = "$vsbase\VC\Tools\MSVC\$msvcver\bin\HostX64\x64;" + $env:PATH

Set-Location $PSScriptRoot

Write-Host "Compilando chipmatic_mod.c..." -ForegroundColor Cyan

& cl.exe /O2 /W2 /nologo src\chipmatic_mod.c /Fe:chipmatic_mod.exe `
    /link comctl32.lib shell32.lib user32.lib gdi32.lib /SUBSYSTEM:WINDOWS 2>&1

if ($LASTEXITCODE -eq 0) {
    Write-Host "`n OK  chipmatic_mod.exe creato!" -ForegroundColor Green
    Write-Host "Avvia: .\chipmatic_mod.exe" -ForegroundColor Yellow
} else {
    Write-Host "`n[ERRORE] Compilazione fallita (exit $LASTEXITCODE)" -ForegroundColor Red
}
