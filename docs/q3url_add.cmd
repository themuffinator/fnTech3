@echo off
rem Registers q3a:// links to start fnTech3's Quake III Arena profile.
rem Keep it beside fntech3.exe, in fnTech3's own folder, and name the Quake III
rem Arena installation, which the links read and never write:
rem   q3url_add.cmd "C:\Games\Quake III Arena" ["home folder"]
rem Configs, downloads and demos go to the home folder, by default
rem %APPDATA%\fnTech3\quake3. q3url_rem.cmd removes the registration.
setlocal
if "%~1"=="" goto usage
if not exist "%~1\baseq3\" goto notquake3
if not exist "%~dp0fntech3.exe" goto noengine
set "EXE=%~dp0fntech3.exe"
set "BASE=%~f1"
if "%BASE:~-1%"=="\" set "BASE=%BASE:~0,-1%"
set "QHOME=%APPDATA%\fnTech3\quake3"
if not "%~2"=="" set "QHOME=%~f2"
if "%QHOME:~-1%"=="\" set "QHOME=%QHOME:~0,-1%"
set "EXE=%EXE:\=\\%"
set "BASE=%BASE:\=\\%"
set "QHOME=%QHOME:\=\\%"
set "F=%TEMP%\q3a.reg"
set "RPATH=HKEY_CURRENT_USER\Software\Classes\q3a"
> "%F%" echo REGEDIT4
>> "%F%" echo [%RPATH%]
>> "%F%" echo @="URL:Q3A (Quake III Arena)"
>> "%F%" echo "URL Protocol"=""
>> "%F%" echo [%RPATH%\shell\open\command]
>> "%F%" echo @="\"%EXE%\" +set com_gameProfile quake3 +set fs_basepath \"%BASE%\" +set fs_homepath \"%QHOME%\" +connect \"%%1\""
regedit -s "%F%"
del "%F%"
exit /b 0

:usage
echo Usage: %~nx0 "Quake III Arena folder" ["home folder"]
exit /b 1

:notquake3
echo "%~1" is not a Quake III Arena installation: it has no baseq3 folder.
exit /b 1

:noengine
echo Keep %~nx0 beside fntech3.exe.
exit /b 1
