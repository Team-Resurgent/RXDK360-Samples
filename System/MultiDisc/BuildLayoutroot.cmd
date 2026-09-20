REM Ignore the build process if the CodeAnalysis
@if %1 == "CodeAnalysis\" goto end

REM Create the directory structure, hiding errors if the directories exist
@mkdir layoutroot 2>null

REM Now, create the layout
for /L %%i in (1,1,4) do (
mkdir layoutroot\Disc%%i 2>null
@Echo.
@Echo.
@Echo --Disc %%i Post Build Process--
@Echo.
@Echo Copying "%~1\Media" to layoutroot\Disc%%i\Media
xcopy /Y /E /I "%~1\Media" layoutroot\Disc%%i\Media
@Echo.
@Echo Copying "%~1\MultiDisc.xdb" to "layoutroot\Disc%%i\MultiDisc.xdb"
copy /Y "%~1\MultiDisc.xdb" layoutroot\Disc%%i\MultiDisc.xdb
@Echo.
@Echo ImageXEX /config:Disc%%i_XEX.xml /IN:"%~1\MultiDisc.exe" /OUT:layoutroot\Disc%%i\default.xex
imagexex /config:Disc%%i_XEX.xml /IN:"%~1\MultiDisc.exe" /OUT:layoutroot\Disc%%i\default.xex)
@Echo.
@Echo ----
@Echo.
:end