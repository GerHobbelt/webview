@echo off
setlocal

echo Prepare directories...
set script_dir=%~dp0
set script_dir=%script_dir:~0,-1%
set src_dir=%script_dir%\..
set native_code_dir=%src_dir%\native
set build_dir=%script_dir%\..\build
mkdir "%build_dir%"

echo Webview directory: %src_dir%
echo Build directory: %build_dir%
echo Native code directory: %native_code_dir%

:: If you update the nuget package, change its version here
set nuget_version=1.0.1150.38
echo Using Nuget Package microsoft.web.webview2.%nuget_version%
if not exist "%script_dir%\microsoft.web.webview2.%nuget_version%" (
	curl -sSLO https://dist.nuget.org/win-x86-commandline/latest/nuget.exe
	nuget.exe install Microsoft.Web.Webview2 -Version %nuget_version% -OutputDirectory %script_dir% -Source https://api.nuget.org/v3/index.json
	echo Nuget package installed
)

echo Looking for vswhere.exe...
set "vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%vswhere%" set "vswhere=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%vswhere%" (
	echo ERROR: Failed to find vswhere.exe
	exit /b 1
)
echo Found %vswhere%

echo Looking for VC...
for /f "usebackq tokens=*" %%i in (`"%vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
  set vc_dir=%%i
)
if not exist "%vc_dir%\Common7\Tools\vsdevcmd.bat" (
	echo ERROR: Failed to find VC tools x86/x64
	exit /b 1
)
echo Found %vc_dir%

:: 4100: unreferenced formal parameter
set warning_params=/W4 /wd4100

:: build dlls if not found
if not exist "%src_dir%\dll\x64\webview.dll" (
	mkdir "%src_dir%\dll\x86"
	mkdir "%src_dir%\dll\x64"
	copy  "%script_dir%\microsoft.web.webview2.%nuget_version%\build\native\x64\WebView2Loader.dll" "%src_dir%\dll\x64"
	copy  "%script_dir%\microsoft.web.webview2.%nuget_version%\build\native\x86\WebView2Loader.dll" "%src_dir%\dll\x86"

	call "%vc_dir%\Common7\Tools\vsdevcmd.bat" -arch=x86 -host_arch=x64

	echo Building webview.dll ^(x86^)
	cl %warning_params% ^
		/D "WEBVIEW_API=__declspec(dllexport)" ^
		/I "%script_dir%\microsoft.web.webview2.%nuget_version%\build\native\include" ^
		/I "%script_dir%\wil-1.0.230411.1\include" ^
		/I "%native_code_dir%" ^
		/std:c++17 /EHsc "/Fo%build_dir%"\ ^
		"%native_code_dir%\webview.cc" "%native_code_dir%\webview_go_glue.c" /link /DLL "/OUT:%src_dir%\dll\x86\webview.dll" || exit /b

	call "%vc_dir%\Common7\Tools\vsdevcmd.bat" -arch=x64 -host_arch=x64
	echo Building webview.dll ^(x64^)
	cl %warning_params% ^
		/D "WEBVIEW_API=__declspec(dllexport)" ^
		/I "%script_dir%\microsoft.web.webview2.%nuget_version%\build\native\include" ^
		/I "%script_dir%\wil-1.0.230411.1\include" ^
		/I "%native_code_dir%" ^
		/std:c++17 /EHsc "/Fo%build_dir%"\ ^
		"%native_code_dir%\webview.cc" "%native_code_dir%\webview_go_glue.c" /link /DLL "/OUT:%src_dir%\dll\x64\webview.dll" || exit /b
)
if not exist "%build_dir%\webview.dll" (
	copy "%src_dir%\dll\x64\webview.dll" %build_dir%
)
if not exist "%build_dir%\WebView2Loader.dll" (
	copy "%script_dir%\microsoft.web.webview2.%nuget_version%\build\native\x64\WebView2Loader.dll" "%build_dir%"
)

call "%vc_dir%\Common7\Tools\vsdevcmd.bat" -arch=x64 -host_arch=x64


echo Building C examples (x64)
mkdir "%build_dir%\examples\c"
cl %warning_params% ^
	/I "%native_code_dir%" ^
	/I "%script_dir%\microsoft.web.webview2.%nuget_version%\build\native\include" ^
	"%src_dir%\dll\x64\webview.lib" ^
	/std:c++17 /EHsc "/Fo%build_dir%\examples\c"\ ^
	"%src_dir%\dll\x64\webview.lib" ^
	"%src_dir%\examples\basic.c" /link "/OUT:%build_dir%\examples\c\basic.exe" || exit /b
cl %warning_params% ^
	/I "%native_code_dir%" ^
	/I "%script_dir%\microsoft.web.webview2.%nuget_version%\build\native\include" ^
	"%src_dir%\dll\x64\webview.lib" ^
	/std:c++17 /EHsc "/Fo%build_dir%\examples\c"\ ^
	"%src_dir%\dll\x64\webview.lib" ^
	"%src_dir%\examples\open_window.c" /link "/OUT:%build_dir%\examples\c\open_window.exe" || exit /b

cl %warning_params% ^
	/I "%native_code_dir%" ^
	/I "%script_dir%\microsoft.web.webview2.%nuget_version%\build\native\include" ^
	"%src_dir%\dll\x64\webview.lib" ^
	/std:c++17 /EHsc "/Fo%build_dir%\examples\c"\ ^
	"%src_dir%\dll\x64\webview.lib" ^
	"%src_dir%\examples\bind.c" /link "/OUT:%build_dir%\examples\c\bind.exe" || exit /b

set "CGO_CXXFLAGS=-DWEBVIEW_EDGE -I%native_code_dir% -I%script_dir%\microsoft.web.webview2.%nuget_version%\build\native\include -I%script_dir%\wil-1.0.230411.1\include"
set "CGO_LDFLAGS=-L%src_dir%\dll\x64\ -lwebview -ladvapi32 -lole32 -lshell32 -lshlwapi -luser32 -lversion"
echo CGO_CXXFLAGS=%CGO_CXXFLAGS%

rem Go needs go.mod to be in the working directory.
pushd "%src_dir%" || exit /b

echo Building Go examples
mkdir "%build_dir%\examples\go"
go build -o "%build_dir%\examples\go\open_window.exe" examples\open_window.go || goto :go_end

:go_end
set go_error=%errorlevel%
popd
if not "%go_error%" == "0" exit /b "%go_error%"
