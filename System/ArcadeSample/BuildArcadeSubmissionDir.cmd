@echo off
setlocal

IF NOT EXIST Submission\. (
	md Submission
)

del /S /F /Q Submission\*

xcopy /Y /F /S /I DebugFiles Submission\DebugFiles
xcopy /Y /F /S /I Docs Submission\Docs
xcopy /Y /F /S /I LiveFiles Submission\LiveFiles
xcopy /Y /F /S /I Package Submission\Package
xcopy /Y /F /S /I Online Submission\Online
copy /Y ArcadeSamplePackage.xlast Submission\
