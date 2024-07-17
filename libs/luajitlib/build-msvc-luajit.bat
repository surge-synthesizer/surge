@echo off

set BD=%~1
if "%BD%"=="" (
    echo Usage: build-msvc-luajit.bat BUILDDIR
    exit /b 1
)
echo Building in %BD%

set "SD=%BD%\src"
set "OD=%BD%\bin"
set "HD=%BD%\include"

if exist "%BD%" rd /s /q "%BD%"
mkdir "%BD%"

mkdir "%SD%"
mkdir "%OD%"
mkdir "%HD%"

xcopy /e /i /h LuaJIT "%SD%\LuaJIT"
copy /y msvcbuild.bat "%SD%\LuaJIT\src\"
cd "%SD%\LuaJIT\src"

call msvcbuild.bat static

copy lua51.lib "%OD%"
echo "%OD%"
dir "%OD%"

copy *.h "%HD%"
copy *.hpp "%HD%"
echo "%HD%"
dir "%HD%"

exit /b 0
