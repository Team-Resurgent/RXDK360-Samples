@echo off
REM ---------------------------------------------------------------------------
REM Build the Xbox Live Arcade Package directory for our project configuration.
REM Usage: 
REM   BuildArcadePackageDir 
REM     [/package:path]
REM     [/dbgfiles:path]
REM     [/livefiles:path]
REM     [/titleid:hex#]
REM     [/config:path]
REM     [/content:path]
REM     /xex:path
REM ---------------------------------------------------------------------------
setlocal

REM ---------------------------------------------------------------------------
REM Process arguments
REM ---------------------------------------------------------------------------

set BLAST="%XEDK%\bin\win32\blast.exe" /nologo
set ARCADELICENSE="%XEDK%\bin\win32\ArcadeLicense.exe" /nologo
set XBCP="%XEDK%\bin\win32\xbcp.exe"

set PACKAGE_DIR=Package
set DEBUGFILES_DIR=DebugFiles
set LIVEFILES_DIR=LiveFiles
set CONFIG_XLAST=ArcadeSample.xlast
set CONTENT_XLAST=ArcadeSamplePackage.xlast
set TITLEID=58410843

set TARGET_NAME=
set TARGET_EXT=
set TARGET_DIR=

for %%i in (%*) do (

    if /i "%%~i" EQU "/help" (
        goto usage
    ) else (
    
        for /F "tokens=1* delims=/:" %%j in ("%%~i") do (
        
            if /i "%%j" EQU "package" (
                set PACKAGE_DIR=%%k
            ) else if /i "%%j" EQU "dbgfiles" (
                set DEBUGFILES_DIR=%%k
            ) else if /i "%%j" EQU "xex" (
                set TARGET_NAME=%%~nk
                set TARGET_EXT=%%~xk
                set TARGET_DIR=%%~dpk
            ) else if /i "%%j" EQU "livefiles" (
                set LIVEFILES_DIR=%%k
            ) else if /i "%%j" EQU "content" (
                set CONTENT_XLAST=%%k
            ) else if /i "%%j" EQU "config" (
                set CONFIG_XLAST=%%k
            ) else if /i "%%j" EQU "titleid" (
                set TITLEID=%%k
            ) else (
                @echo ERROR: Unknown parameter [%%~i]
                goto usage
            )
        )
    )
)

REM ---------------------------------------------------------------------------
REM Check arguments
REM ---------------------------------------------------------------------------

if "%PACKAGE_DIR%" EQU "" (
    @echo /package not specified.
    goto usage
)
if "%DEBUGFILES_DIR%" EQU "" (
    @echo /dbgfiles not specified.
    goto usage
)
if "%TARGET_NAME%" EQU "" (
    @echo /xex not specified.
    goto usage
)
if "%TARGET_DIR%" EQU "" (
    @echo XEX directory resolved empty.
    goto usage
)
if "%LIVEFILES_DIR%" EQU "" (
    @echo /livefiles not specified.
    goto usage
)
if not exist "%TARGET_DIR%" (
    @echo Target directory "%TARGET_DIR%" does not exist
    goto usage
)
if not exist "%LIVEFILES_DIR%" (
    @echo LiveFiles directory "%LIVEFILES_DIR%" does not exist
    goto usage
)
if not exist "%LIVEFILES_DIR%\%CONFIG_XLAST%" (
    @echo Title config xlast "%LIVEFILES_DIR%\%CONFIG_XLAST%" does not exist
    goto usage
)
if not exist "%CONTENT_XLAST%" (
    @echo Content package xlast "%CONTENT_XLAST%" does not exist
    goto usage
)


REM ---------------------------------------------------------------------------
REM Summary 
REM ---------------------------------------------------------------------------
@echo.
@echo Building [%PACKAGE_DIR%] and [%DEBUGFILES_DIR%] from
@echo   XEX Name:      [%TARGET_NAME%%TARGET_EXT%]
@echo   XEX Dir:       [%TARGET_DIR%]
@echo   LiveFiles:     [%LIVEFILES_DIR%]
@echo   Config Xlast:  [%CONFIG_XLAST%]
@echo   Content Xlast: [%CONTENT_XLAST%]
@echo.


REM ---------------------------------------------------------------------------
REM Build Arcade Package Dir
REM ---------------------------------------------------------------------------
IF NOT EXIST "%PACKAGE_DIR%\." md %PACKAGE_DIR%
IF NOT EXIST "%DEBUGFILES_DIR%\." md %DEBUGFILES_DIR%

@echo.
echo Cleaning %DEBUGFILES_DIR% directory
del /F /Q "%DEBUGFILES_DIR%\*"

@echo.
echo Copying PDB to %DEBUGFILES_DIR%\
xcopy /F /Y "%TARGET_DIR%%TARGET_NAME%.pdb" "%DEBUGFILES_DIR%\"

@echo.
echo Copying XEX to %DEBUGFILES_DIR%\
xcopy /F /Y "%TARGET_DIR%%TARGET_NAME%%TARGET_EXT%" "%DEBUGFILES_DIR%\"

@echo.
echo Cleaning %PACKAGE_DIR% directory
del /F /Q "%PACKAGE_DIR%\*"

@echo.
echo Copying XEX to %PACKAGE_DIR%\default.xex
copy /Y "%TARGET_DIR%%TARGET_NAME%%TARGET_EXT%" "%PACKAGE_DIR%\default.xex"

@echo.
echo Copying XZP to %PACKAGE_DIR%\
xcopy /F /Y "%TARGET_DIR%%TARGET_NAME%.xzp" "%PACKAGE_DIR%\"

@echo.
echo Copying ArcadeInfo.xml to %PACKAGE_DIR%\
xcopy /F /Y "%TARGET_DIR%ArcadeInfo.xml" "%PACKAGE_DIR%\"

@echo.
echo Copying LiveFiles to %PACKAGE_DIR%\
xcopy /F /Y "%LIVEFILES_DIR%\achievement*.png" "%PACKAGE_DIR%\"
xcopy /F /Y "%LIVEFILES_DIR%\Rating*.png" "%PACKAGE_DIR%\"
xcopy /F /Y "%LIVEFILES_DIR%\TitleIcon.png" "%PACKAGE_DIR%\"


REM ---------------------------------------------------------------------------
REM Build Content Packages
REM ---------------------------------------------------------------------------

@echo -------------------------------------------------------------------------
@echo -------------------------------------------------------------------------
@echo Building Arcade Content Package with BLAST
@echo   You need a development kit attached for this to work.
@echo -------------------------------------------------------------------------
@echo -------------------------------------------------------------------------
BLAST /install:Local %CONTENT_XLAST%

if ERRORLEVEL 1 (
    @echo -------------------------------------------------------------------------
    @echo [NOTE] BLAST failed to build content. 
    @echo   Use XLAST or BLAST to build and install the Arcade content package.
    @echo   You need an Xbox Development Kit attached to build the Arcade Content Package.
    @echo -------------------------------------------------------------------------
    goto end
    
) else (
    @echo -------------------------------------------------------------------------
    @echo -------------------------------------------------------------------------
    echo Copy Trial Content Package to Full Content Package
    copy /Y "Online\%TITLEID%00000000" "Online\%TITLEID%00000001"
    if ERRORLEVEL 1 (
        @echo -------------------------------------------------------------------------
        @echo [NOTE] Trial Arcade package not found. 
        @echo   Build the Trial package with XLAST or BLAST, then use ArcadeLicense 
        @echo   to modify the Trial/Full license of an Arcade package. You need an 
        @echo   Xbox Development Kit attached to build the Arcade Content Package.
        @echo -------------------------------------------------------------------------
        goto end
        
    ) else (
        @echo -------------------------------------------------------------------------
        @echo -------------------------------------------------------------------------
        echo Apply Proper Licenses
        ARCADELICENSE Trial "Online\%TITLEID%00000000"
        ARCADELICENSE Full  "Online\%TITLEID%00000001"
        
        if ERRORLEVEL 1 (
            @echo -------------------------------------------------------------------------
            @echo [NOTE] ArcadeLicense failed to adjust Arcade Content Package licenses.
            @echo   Use ArcadeLicense manually to adjust Trial/Full licenses.
            @echo -------------------------------------------------------------------------
            goto end
            
        ) else (
            @echo -------------------------------------------------------------------------
            @echo -------------------------------------------------------------------------
            echo Copy Arcade Content Packages to the Development Kit
            XBCP /Y /T "Online\%TITLEID%00000000" HDD:\Content\0000000000000000\%TITLEID%\000D0000\
            XBCP /Y /T "Online\%TITLEID%00000001" HDD:\Content\0000000000000000\%TITLEID%\000D0000\
            
            if ERRORLEVEL 1 (
                @echo -------------------------------------------------------------------------
                @echo [NOTE] Failed to copy Arcade Content Packages to the devkit.
                @echo   Packages in Arcade Dashboard have not been updated. 
                @echo   Copy them to: HDD:\Content\0000000000000000\%TITLEID%\000D0000\
                @echo -------------------------------------------------------------------------
                goto end
            )
        )
    )
)

goto end


REM ---------------------------------------------------------------------------
REM Error
:error
@echo *************************************************************************
@echo ERROR: .
@echo *************************************************************************
exit /b 1

REM ---------------------------------------------------------------------------
REM Print Usage
:usage
@echo BuildArcadePackageDir: Script to build the Xbox Live Arcade Package 
@echo   directory for our project configuration.
@echo.
@echo Usage:
@echo   BuildArcadePackageDir [/package:path] [/dbgfiles:path]
@echo     [/livefiles:path] [/titleid:hex] [/content:path] /xex:path 
@echo.
@echo   /package:path       Output Package directory.
@echo   /dbgfiles:path      Output DebugFiles directory. 
@echo   /livefiles:path     LiveFiles directory.
@echo   /titleid:hex#       TitleID
@echo   /content:path       Path to content package xlast
@echo   /xex:path           XEX path. 
@echo.
exit /b 1

:end
exit /b 0
