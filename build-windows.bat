@echo off
rem Baut OliCraft mit dem Standard-Generator (z. B. Visual Studio) und startet es.
cmake -S . -B build || goto :fail
cmake --build build --config Release || goto :fail
echo.
echo Fertig. Starte OliCraft ...
if exist build\Release\OliCraft.exe (
  build\Release\OliCraft.exe
) else (
  build\OliCraft.exe
)
goto :eof
:fail
echo Build fehlgeschlagen.
pause
