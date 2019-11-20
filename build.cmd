@echo off ::keep it cleaner for better readebility of the output

REM We can still at least try to build even if Git is not installed.
git submodule update --init --recursive || echo "Git failed, submodules not updated."

premake5 vs2017 || goto error
REM msbuild /target:Build surge-vst2.vcxproj /maxcpucount /p:Configuration=Release;Platform=x64 || goto error
REM msbuild /target:Build surge-vst3.vcxproj /maxcpucount /p:Configuration=Release;Platform=x64 || goto error
msbuild buildtask.xml /maxcpucount

rd /s /q build
cmake -Bbuild .
msbuild /target:Build build\surge-headless.vcxproj

exit /b 0

:error
echo build failed!
pause ::pause so the user knows something went wrong
exit /b 1
