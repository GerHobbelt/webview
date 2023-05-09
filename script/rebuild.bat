set script_dir=%~dp0
set script_dir=%script_dir:~0,-1%
del /F /S /Q %script_dir%\..\dll
del /F /Q %script_dir%\..\build\webview.dll
call %script_dir%\build_open_window.bat
copy %script_dir%\..\dll\x64\webview.dll %script_dir%\..\build\examples\c\webview.dll 