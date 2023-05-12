set script_dir=%~dp0
set script_dir=%script_dir:~0,-1%
del /F /S /Q %script_dir%\..\dll
del /F /Q %script_dir%\..\build\webview.dll
call %script_dir%\build_open_window.bat
copy %script_dir%\..\dll\x64\webview.dll %script_dir%\..\build\examples\c\webview.dll 
copy %script_dir%\..\dll\x64\webview.dll %script_dir%\..\build\examples\go\webview.dll 
xcopy /Y %script_dir%\..\native\webview_*.h %script_dir%\..\lib\go_webview\include\native
xcopy /S /Y %script_dir%\..\dll\*.dll %script_dir%\..\lib\go_webview\win
xcopy /S /Y %script_dir%\..\dll\*.lib %script_dir%\..\lib\go_webview\win