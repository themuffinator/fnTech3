@echo off
rem Registers q3a:// links to start fnTech3's Quake III Arena profile.
rem Keep it beside fntech3.exe in the Quake III Arena folder, which it uses as
rem both fs_basepath and fs_homepath; q3url_rem.cmd removes the registration.
set CWD=%~dp0 
set CWD=%CWD:~0,-2%
set CWD=%CWD:\=\\%
set F=%TEMP%\q3a.reg
set RPATH=HKEY_CURRENT_USER\Software\Classes\q3a
rem set RPATH=HKEY_CLASSES_ROOT\q3a
echo REGEDIT4 > %F%
echo [%RPATH%] >> %F%
echo @="URL:Q3A (Quake III Arena)" >> %F%
echo "URL Protocol"="" >> %F%
echo [%RPATH%\shell\open\command] >> %F%
echo @="\"%CWD%\\fntech3.exe\" +set com_gameProfile quake3 +set fs_basepath \"%CWD%\" +set fs_homepath \"%CWD%\" +connect \"%%1\"" >> %F%
regedit -s %F%
del %F%