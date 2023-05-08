@echo off
setlocal

echo Prepare directories...
set script_dir=%~dp0
set script_dir=%script_dir:~0,-1%
set src_dir=%script_dir%\..
set build_dir=%script_dir%\..\build
set windows_kit_folder=%ProgramFiles(x86)%\Windows Kits
mkdir "%build_dir%"

set nuget_version=1.0.1150.38
set wil_version=1.0.230411.1
set windows_kit_version=10.0.22000.0
set windows_kit_include_folder=%windows_kit_folder%\10\include\%windows_kit_version%
set windows_kit_lib_folder=%windows_kit_folder%\10\lib\%windows_kit_version%
set wil_src_root=%script_dir%\wil-1.0.230411.1
set wil_lib_root=%wil_src_root%\build\clang64release\packages\Microsoft.Windows.CppWinRT.2.0.221121.5\build\native\lib

echo Setting up environment for Go...
rem Argument quoting works for Go 1.18 and later but as of 2022-06-26 GitHub Actions has Go 1.17.11.
rem See https://go-review.googlesource.com/c/go/+/334732/
rem TODO: Use proper quoting when GHA has Go 1.18 or later.
set "CGO_CXXFLAGS=-I%script_dir%\microsoft.web.webview2.%nuget_version%\build\native\include -I%wil_src_root%\include -I"%windows_kit_include_folder%\winrt""
echo %CGO_CXXFLAGS%
set CGO_ENABLED=1

rem Go needs go.mod to be in the working directory.
pushd "%src_dir%" || exit /b

echo Building Go examples
mkdir "%build_dir%\examples\go"
go build -ldflags="-H windowsgui" -o "%build_dir%\examples\go\basic.exe" examples\basic.go || goto :go_end
go build -ldflags="-H windowsgui" -o "%build_dir%\examples\go\bind.exe" examples\bind.go || goto :go_end

:go_end
set go_error=%errorlevel%
popd
if not "%go_error%" == "0" exit /b "%go_error%"