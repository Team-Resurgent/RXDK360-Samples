attrib -R xedfile.dll
copy "%XEDK%\bin\win32\xedfile.dll"

attrib -R *.oc


call convertclosed.bat
call convertopen.bat
