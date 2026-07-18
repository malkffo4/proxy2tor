PEM @echo off
cd /d "%~dp0"  REM Переходим в каталог, где находится сам батник

REM Указываем относительные пути для tor.exe и torrc
start "" .\tor\tor.exe -f .\torrc
