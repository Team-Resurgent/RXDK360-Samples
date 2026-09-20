@echo off
setlocal

set RESXCREATE="%XEDK%\bin\win32\xui2resx.exe" 
set RESXMERGE="%XEDK%\bin\win32\resxloc.exe" 

set DEFAULT_LOCALE=en-us
set LOC_DIR=%DEFAULT_LOCALE%
set LOC_TEMP=Loc-Temp

REM Create a temporary directory
IF NOT EXIST %LOC_TEMP%\. ( 
	md %LOC_TEMP%
) ELSE (
	del /F %LOC_TEMP%\*
)

REM Generate resx files from the XUI definitions
%RESXCREATE% *.xui %LOC_TEMP%\
xcopy /F Strings.resx %LOC_TEMP%\

REM Create the output directory
IF NOT EXIST %LOC_DIR%\. ( 
	md %LOC_DIR% 
)

REM Merge the individual resx files into a single loc.resx
%RESXMERGE% %LOC_TEMP%\*.resx %LOC_DIR%\loc.resx

REM Remove the individual resx files
REM You can now obtain these files by using ResXSplit.cmd
del /F /Q %LOC_TEMP%\*
rd %LOC_TEMP%


exit /b 0
