@echo off
setlocal

set RESXSPLIT="%XEDK%\bin\win32\resxloc.exe" /nologo
set LOCALE=%1

set INFILE=%LOCALE%\loc.resx

IF NOT EXIST %INFILE% (
	echo ERROR: [%INFILE%] does not exist.
	exit /b 1
) ELSE (
	REM Split the infile into separate resx files
	%RESXSPLIT% %INFILE%
)

exit /b 0
