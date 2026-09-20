@echo off
setlocal
pushd %~p0
pushd Content\UI

set XUIPKG="%XEDK%\bin\win32\xuipkg.exe" /nologo

set XUI_XZP=%1\ArcadeSample.xzp
set XUI_SCENES=*Scene.xui *Scene.resx
set XUI_SKINS=Skin.xui Skin.resx
set XUI_STRINGS=Strings.resx
set XUI_FONTS=*.ttf
set XUI_IMAGES=*.png


@echo Building ArcadeSample.xzp from Localized Content


REM Split the single loc.resx files into individual component resx files.
call ResXSplitAllLocales.cmd


REM Recursively descend into the Loc directories adding Scene Xui & ResX files
%XUIPKG% /R /O %XUI_XZP% %XUI_SCENES%

REM Recursively descend into the Loc directories adding Skin Xui & ResX files
%XUIPKG% /R /A %XUI_XZP% %XUI_SKINS%

REM Recursively descend into the Loc directories adding Image Files
%XUIPKG% /A %XUI_XZP% %XUI_IMAGES%

REM Recursively descend into the Loc directories adding Font Files
%XUIPKG% /A %XUI_XZP% %XUI_FONTS%

REM Add Strings.resx and generate Strings.h
%XUIPKG% /R /I /A %XUI_XZP% %XUI_STRINGS%


popd
popd
exit /b 0
