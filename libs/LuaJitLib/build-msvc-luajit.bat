@echo off

set "BD=%1"
if "%BD%"=="" (
    echo Usage: build-mingw-luajit BUILDDIR
    exit /b 1
)
echo Building in %BD%

set "OD=%BD%\bin"
set "SD=%BD%\src"

if exist "%BD%" rd /s /q "%BD%"
mkdir "%BD%"

mkdir "%OD%"
mkdir "%SD%"

xcopy /e /i /h LuaJIT "%SD%\LuaJIT"
cd "%SD%\LuaJIT\src"

call msvcbuild.bat amalg static
if %errorlevel% neq 0 (
    echo That's OK though
)

copy lua*.lib "%OD%"
dir "%OD%"

exit /b 0