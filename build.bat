@echo off
rem Builds the client into build\Leviathan.exe.
rem
rem Uses the Ninja generator on purpose: CMake copies the runtime DLLs and the
rem data folder into the build directory root, so a multi-config generator like
rem "Visual Studio" would put DDNet.exe into build\Release\ where it finds
rem neither of them and fails to start with missing DLL errors.
setlocal

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
	echo ERROR: vswhere.exe not found, is Visual Studio installed?
	exit /b 1
)

for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
if not defined VSPATH (
	echo ERROR: no Visual Studio installation with the C++ toolchain found.
	exit /b 1
)

call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
set "PATH=%VSPATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%"

cmake -S "%~dp0." -B "%~dp0build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCLIENT_EXECUTABLE=Leviathan -DDISCORD=ON || exit /b 1
cmake --build "%~dp0build" --target game-client || exit /b 1

echo.
echo Built %~dp0build\Leviathan.exe
